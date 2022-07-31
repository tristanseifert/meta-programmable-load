#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <system_error>

#include <cbor.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <fmt/format.h>
#include <plog/Log.h>

#include "load-common/EventLoop.h"
#include "load-common/Rpc/Types.h"
#include "load-common/Rpc/ClientBase.h"

using namespace PlCommon::Rpc;

/**
 * @brief Create a pinball client instance
 */
ClientBase::ClientBase(const std::filesystem::path &rpcSocketPath,
        const std::shared_ptr<PlCommon::EventLoop> &ev) : socketPath(rpcSocketPath) {
    int err;
    auto evbase = ev->getEvBase();

    // validate args
    if(!ev) {
        throw std::invalid_argument("invalid event loop");
    } else if(rpcSocketPath.empty()) {
        throw std::invalid_argument("rpc socket path is empty!");
    }

    // establish connection and create an event
    this->fd = this->connectSocket();

    this->bev = bufferevent_socket_new(evbase, this->fd, 0);
    if(!this->bev) {
        throw std::runtime_error("failed to create bufferevent");
    }

    bufferevent_setwatermark(this->bev, EV_READ, sizeof(struct RpcHeader),
            EV_RATE_LIMIT_MAX);

    bufferevent_setcb(this->bev, [](auto bev, auto ctx) {
        try {
            reinterpret_cast<ClientBase *>(ctx)->bevRead(bev);
        } catch(const std::exception &e) {
            PLOG_ERROR << "Failed to handle read: " << e.what();
        }
    }, nullptr, [](auto bev, auto what, auto ctx) {
        try {
            reinterpret_cast<ClientBase *>(ctx)->bevEvent(bev, what);
        } catch(const std::exception &e) {
            PLOG_ERROR << "Failed to handle event: " << e.what();
        }
    }, this);

    // add events to run loop
    err = bufferevent_enable(this->bev, EV_READ);
    if(err == -1) {
        throw std::runtime_error("failed to enable bufferevent");
    }

}

/**
 * @brief Clean up client resources
 */
ClientBase::~ClientBase() {
    if(this->bev) {
        bufferevent_free(this->bev);
    }

    if(this->fd != -1) {
        close(this->fd);
    }
}

/**
 * @brief Connect the client socket
 *
 * @return File descriptor
 */
int ClientBase::connectSocket() {
    int fd, err;

    // create the socket
    fd = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if(fd == -1) {
        throw std::system_error(errno, std::generic_category(), "create rpc socket");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, this->socketPath.native().c_str(), sizeof(addr.sun_path) - 1);

    // dial it
    err = connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if(err == -1) {
        close(fd);
        throw std::system_error(errno, std::generic_category(), "dial rpc socket");
    }

    // mark the socket to use non-blocking IO (for libevent)
    err = evutil_make_socket_nonblocking(fd);
    if(err == -1) {
        close(fd);
        throw std::system_error(errno, std::generic_category(),
                "evutil_make_socket_nonblocking");
    }

    // it's been connected :D
    return fd;
}



/**
 * @brief Handle a received message
 *
 * Identify what the packet is and route it appropriately.
 */
void ClientBase::bevRead(struct bufferevent *ev) {
    // pull it out and into our read buffer
    auto buf = bufferevent_get_input(bev);
    const size_t pending = evbuffer_get_length(buf);

    this->rxBuf.resize(pending);
    int read = evbuffer_remove(buf, static_cast<void *>(this->rxBuf.data()), pending);

    if(read == -1) {
        throw std::runtime_error("failed to drain read buffer");
    }

    // validate header
    if(pending < sizeof(struct RpcHeader)) {
        PLOG_WARNING << fmt::format("insufficient RPC read (got {})", pending);
        return;
    }

    auto hdr = reinterpret_cast<const struct RpcHeader *>(this->rxBuf.data());
    if(hdr->version != kRpcVersionLatest) {
        PLOG_WARNING << fmt::format("unknown rpc version ${:04x}", hdr->version);
        return;
    } else if(hdr->length < sizeof(struct RpcHeader)) {
        PLOG_WARNING << fmt::format("invalid rpc packet size ({} bytes)", hdr->length);
        return;
    }

    // invoke the handler
    std::span<const std::byte> payload{reinterpret_cast<const std::byte *>(hdr->payload),
        hdr->length - sizeof(*hdr)};

    this->handleIncomingMessageRaw(*hdr, payload);
}

/**
 * @brief Handle an event on the loadd connection
 *
 * @param flags Bufferevent status flag
 */
void ClientBase::bevEvent(struct bufferevent *, const uintptr_t flags) {
    // connection closed
    if(flags & BEV_EVENT_EOF) {
        this->handleConnectionClosed();
        this->fd = 0;
    }
    // IO error
    else if(flags & BEV_EVENT_ERROR) {
        this->handleIoError(flags);
    }
}

/**
 * @brief Send a raw packet to the remote
 *
 * This assumes the packet already has a `struct RpcHeader` prepended.
 */
void ClientBase::sendRaw(std::span<const std::byte> payload) {
    int err = write(this->fd, payload.data(), payload.size());
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "write");
    }
}

/**
 * @brief Send a packet to the remote, adding a packet header
 *
 * Generate a full packet (including packet header) and send it to the remote.
 *
 * @return Tag value associated with the packet
 */
uint8_t ClientBase::sendPacket(const uint8_t endpoint, std::span<const std::byte> payload) {
    std::vector<std::byte> buffer;
    buffer.resize(sizeof(struct RpcHeader) + payload.size(), std::byte{0});

    // build up the header
    auto hdr = reinterpret_cast<struct RpcHeader *>(buffer.data());
    hdr->version = kRpcVersionLatest;
    hdr->length = sizeof(*hdr) + payload.size();
    hdr->endpoint = endpoint;

    do {
        hdr->tag = ++this->nextTag;
        // TODO: ensure there isn't a packet with this tag outstanding
    } while(!hdr->tag);

    // copy payload
    if(!payload.empty()) {
        std::copy(payload.begin(), payload.end(), buffer.begin() + sizeof(*hdr));
    }

    // send and return tag
    this->sendRaw(buffer);
    return hdr->tag;
}



/**
 * @brief Process a raw incoming message
 *
 * This decodes the message's CBOR payload, if any, and invokes the high level message handler
 * provided by the implementation.
 *
 * @remark You should usually not need to override this method, unless the RPC protocol has
 * messages which do not carry CBOR payloads.
 *
 * @seeAlso handleIncomingMessage
 */
void ClientBase::handleIncomingMessageRaw(const struct RpcHeader &header,
        std::span<const std::byte> payload) {
    cbor_item_t *message{nullptr};

    if(!payload.empty()) {
        struct cbor_load_result result{};

        // set up decoder
        message = cbor_load(reinterpret_cast<const cbor_data>(payload.data()), payload.size(),
                &result);
        if(result.error.code != CBOR_ERR_NONE) {
            throw std::runtime_error(fmt::format("cbor_load failed: {} (at {})", result.error.code,
                        result.error.position));
        }
    }

    try {
        this->handleIncomingMessage(header, message);
    } catch(const std::exception &) {
        if(message) {
            cbor_decref(&message);
        }
        throw;
    }

    // clean up
    if(message) {
        cbor_decref(&message);
    }
}

/**
 * @brief Handle the connection being closed
 *
 * @remark This is a default handler and it just prints a message to the error log.
 */
void ClientBase::handleConnectionClosed() {
    PLOG_WARNING << "RPC connection closed by remote";
}

/**
 * @brief Handle an IO error on the connection
 *
 * This indicates that some sort of error took place on the connection
 *
 * @param flags Raw libevent error flags
 *
 * @remark This is a default handler and it just prints a message to the error log.
 */
void ClientBase::handleIoError(const uintptr_t flags) {
    PLOG_WARNING << fmt::format("RPC connection error: ${:x}", flags);
}
