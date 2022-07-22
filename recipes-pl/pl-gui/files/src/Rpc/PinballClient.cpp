#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cerrno>
#include <system_error>

#include <cbor.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <fmt/format.h>
#include <plog/Log.h>

#include "EventLoop.h"
#include "RpcTypes.h"
#include "PinballClient.h"

using namespace Rpc;

/**
 * @brief RPC message endpoints
 */
enum rpc_endpoint {
    kRpcEndpointNoOp                    = 0x00,
    kRpcEndpointBroadcastConfig         = 0x01,
    kRpcEndpointUiEvent                 = 0x02,
};

/**
 * @brief Create a pinball client instance
 */
PinballClient::PinballClient(const std::shared_ptr<EventLoop> &ev,
        const std::filesystem::path &rpcSocketPath) : ev(ev), socketPath(rpcSocketPath) {
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
        throw std::runtime_error("failed to create bufferevent (pinballd)");
    }

    bufferevent_setwatermark(this->bev, EV_READ, sizeof(struct rpc_header),
            EV_RATE_LIMIT_MAX);

    bufferevent_setcb(this->bev, [](auto bev, auto ctx) {
        try {
            reinterpret_cast<PinballClient *>(ctx)->bevRead(bev);
        } catch(const std::exception &e) {
            PLOG_ERROR << "Failed to handle pinballd read: " << e.what();
            std::terminate();
        }
    }, nullptr, [](auto bev, auto what, auto ctx) {
        try {
            reinterpret_cast<PinballClient *>(ctx)->bevEvent(bev, what);
        } catch(const std::exception &e) {
            PLOG_ERROR << "Failed to handle pinballd event: " << e.what();
            std::terminate();
        }
    }, this);

    // add events to run loop
    err = bufferevent_enable(this->bev, EV_READ);
    if(err == -1) {
        throw std::runtime_error("failed to enable bufferevent (pinballd)");
    }

}

/**
 * @brief Clean up client resources
 */
PinballClient::~PinballClient() {
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
int PinballClient::connectSocket() {
    int fd, err;

    // create the socket
    fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd == -1) {
        throw std::system_error(errno, std::generic_category(), "create pinballd socket");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, this->socketPath.native().c_str(), sizeof(addr.sun_path) - 1);

    // dial it
    err = connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if(err == -1) {
        close(fd);
        throw std::system_error(errno, std::generic_category(), "dial pinballd socket");
    }

    // mark the socket to use non-blocking IO (for libevent)
    err = evutil_make_socket_nonblocking(fd);
    if(err == -1) {
        close(fd);
        throw std::system_error(errno, std::generic_category(),
                "evutil_make_socket_nonblocking (pinballd)");
    }

    // it's been connected :D
    return fd;
}



/**
 * @brief Handle a received message
 *
 * Identify what the packet is and route it appropriately.
 */
void PinballClient::bevRead(struct bufferevent *ev) {
    // pull it out and into our read buffer
    auto buf = bufferevent_get_input(bev);
    const size_t pending = evbuffer_get_length(buf);

    this->rxBuf.resize(pending);
    int read = evbuffer_remove(buf, static_cast<void *>(this->rxBuf.data()), pending);

    if(read == -1) {
        throw std::runtime_error("failed to drain pinballd read buffer");
    }

    // validate header
    if(pending < sizeof(struct rpc_header)) {
        PLOG_WARNING << fmt::format("insufficient pinballd read (got {})", pending);
        return;
    }

    auto hdr = reinterpret_cast<const struct rpc_header *>(this->rxBuf.data());
    if(hdr->version != kRpcVersionLatest) {
        PLOG_WARNING << fmt::format("unknown pinballd rpc version ${:04x}", hdr->version);
        return;
    } else if(hdr->length < sizeof(struct rpc_header)) {
        PLOG_WARNING << fmt::format("invalid rpc packet size ({} bytes)", hdr->length);
        return;
    }

    // invoke the handler
    std::span<const std::byte> payload{reinterpret_cast<const std::byte *>(hdr->payload),
        hdr->length - sizeof(*hdr)};

    switch(hdr->endpoint) {
        case kRpcEndpointUiEvent:
            this->processUiEvent(payload);
            break;

        case kRpcEndpointNoOp:
            break;
        default:
            PLOG_WARNING << fmt::format("unknown pinballd rpc type ${:02x}", hdr->endpoint);
            break;
    }
}

/**
 * @brief Handle an event on the loadd connection
 *
 * @param flags Bufferevent status flag
 */
void PinballClient::bevEvent(struct bufferevent *, const uintptr_t flags) {
    // connection closed
    if(flags & BEV_EVENT_EOF) {
        PLOG_WARNING << "pinballd closed connection :(";
        this->fd = 0;
    }
    // IO error
    else if(flags & BEV_EVENT_ERROR) {
        PLOG_WARNING << "pinballd io error: flags=" << flags;
    }
}



/**
 * @brief Process a received user event broadcast
 *
 * Generate an event and inject it into the GUI subsystem as appropriate.
 */
void PinballClient::processUiEvent(std::span<const std::byte> payload) {
    PLOG_DEBUG << "Received UI event: " << payload.size() << " bytes";
}

