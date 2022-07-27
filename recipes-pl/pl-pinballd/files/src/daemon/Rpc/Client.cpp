#include <algorithm>
#include <cstring>
#include <system_error>

#include <cbor.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <fmt/format.h>
#include <plog/Log.h>

#include "Utils/Cbor.h"
#include "EventLoop.h"
#include "RpcTypes.h"
#include "Server.h"
#include "Client.h"

using namespace Rpc;

/**
 * @brief Create a new client data structure
 *
 * Initialize a buffer event (used for event notifications, like the connection being closed; as
 * well as when buffered data is available) for the client.
 *
 * @param server RPC server to which the client connected
 * @param fd File descriptor for client (we take ownership of this)
 */
Client::Client(const std::shared_ptr<Server> &server, const int fd) : socket(fd), server(server) {
    // create the event
    this->event = bufferevent_socket_new(EventLoop::Current()->getEvBase(), this->socket, 0);
    if(!this->event) {
        throw std::runtime_error("failed to create bufferevent");
    }

    // set watermark: don't invoke read callback til a full header has been read at least
    bufferevent_setwatermark(this->event, EV_READ, sizeof(struct rpc_header),
            EV_RATE_LIMIT_MAX);

    // install callbacks
    bufferevent_setcb(this->event, [](auto bev, auto ctx) {
        try {
            reinterpret_cast<Client *>(ctx)->bevRead(bev);
        } catch(const std::exception &e) {
            PLOG_ERROR << "Failed to handle client read: " << e.what();
            throw;
        }
    }, nullptr, [](auto bev, auto what, auto ctx) {
        try {
            reinterpret_cast<Client *>(ctx)->bevEvent(bev, what);
        } catch(const std::exception &e) {
            PLOG_ERROR << "Failed to handle client event: " << e.what();
            throw;
        }
    }, this);

    // enable event for "client data available to read" events
    int err = bufferevent_enable(this->event, EV_READ);
    if(err == -1) {
        throw std::runtime_error("failed to enable bufferevent");
    }
}

/**
 * @brief Ensure all client resources are released.
 *
 * This closes the client socket, as well as releasing the libevent resources.
 */
Client::~Client() {
    if(this->event) {
        bufferevent_free(this->event);
    }

    if(this->socket != -1) {
        close(this->socket);
    }
}



/**
 * @brief A client connection is ready to read
 *
 * Reads data from the given client connection.
 */
void Client::bevRead(struct bufferevent *ev) {
    // read client data
    /**
     * TODO: rework this so data is buffered over time in the client receive buffer, rather than
     * being overwritten each time, in case clients decide to do partial writes down the line!
     */
    auto buf = bufferevent_get_input(ev);
    const size_t pending = evbuffer_get_length(buf);

    this->receiveBuf.resize(pending);
    int read = evbuffer_remove(buf, static_cast<void *>(this->receiveBuf.data()), pending);

    if(read == -1) {
        throw std::runtime_error("failed to drain client read buffer");
    }

    // read the header
    if(read < sizeof(struct rpc_header)) {
        // we haven't yet read enough bytes; this should never happen, so abort
        throw std::runtime_error(fmt::format("read too few bytes ({}) from client", read));
    }

    const auto hdr = reinterpret_cast<const struct rpc_header *>(this->receiveBuf.data());

    if(hdr->version != kRpcVersionLatest) {
        throw std::runtime_error(fmt::format("unsupported rpc version ${:04x}", hdr->version));
    } else if(hdr->length < sizeof(struct rpc_header)) {
        throw std::runtime_error(fmt::format("invalid header length ({}, too short)",
                    hdr->length));
    }

    const auto payloadLen = hdr->length - sizeof(struct rpc_header);
    if(payloadLen > this->receiveBuf.size()) {
        throw std::runtime_error(fmt::format("invalid header length ({}, too long)",
                    hdr->length));
    }

    // decode as CBOR, if desired
    struct cbor_load_result result{};

    auto item = cbor_load(reinterpret_cast<const cbor_data>(hdr->payload), payloadLen,
            &result);
    if(result.error.code != CBOR_ERR_NONE) {
        throw std::runtime_error(fmt::format("cbor_load failed: {} (at ${:x})", result.error.code,
                    result.error.position));
    }

    // invoke endpoint handler
    try {
        this->dispatchPacket(*hdr, item);
    } catch(const std::exception &e) {
        cbor_decref(&item);
        throw;
    }

    // clean up
    cbor_decref(&item);
}

/**
 * @brief A client connection event ocurred
 *
 * This handles errors on read/write, as well as the connection being closed. In all cases, we'll
 * proceed by releasing this connection, which will close it if not already done.
 */
void Client::bevEvent(struct bufferevent *ev, const size_t flags) {
    // connection closed
    if(flags & BEV_EVENT_EOF) {
        PLOG_DEBUG << "Client " << this->socket << " closed connection";
    }
    // IO error
    else if(flags & BEV_EVENT_ERROR) {
        PLOG_WARNING << "Client " << this->socket << " error: flags=" << flags;
    }

    // in either case, remove the client struct
    if(auto server = this->server.lock()) {
        server->releaseClient(this->getId());
    }
}



/**
 * @brief Reply to a previously received message
 *
 * Send a reply to a previous message, including the given (optional) payload. Replies include the
 * same endpoint and tag values as the incoming request, and have the "reply" flag set.
 *
 * @param req Message header of the request we're replying to
 * @param payload Optional payload to add to the reply
 */
void Client::replyTo(const struct rpc_header &req, std::span<const std::byte> payload) {
    // calculate total size required and reserve space
    const size_t msgSize = sizeof(struct rpc_header) + payload.size();
    this->transmitBuf.resize(msgSize, std::byte(0));
    std::fill(this->transmitBuf.begin(), this->transmitBuf.begin() + sizeof(struct rpc_header),
            std::byte(0));

    // fill in header
    auto hdr = reinterpret_cast<struct rpc_header *>(this->transmitBuf.data());
    hdr->version = kRpcVersionLatest;
    hdr->length = msgSize;
    hdr->endpoint = req.endpoint;
    hdr->tag = req.tag;
    hdr->flags = (1 << 0);

    // copy payload
    if(!payload.empty()) {
        std::copy(payload.begin(), payload.end(),
                (this->transmitBuf.begin() + offsetof(struct rpc_header, payload)));
    }

    // transmit the message
    this->send(this->transmitBuf);
}

/**
 * @brief Transmit the given packet
 *
 * Sends the packet data over the RPC connection. It's assumed the packet already has a header
 * attached to it.
 */
void Client::send(std::span<const std::byte> buf) {
    int err;
    err = bufferevent_write(this->event, buf.data(), buf.size());

    // IO failed
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "write rpc reply");
    }
}



/**
 * @brief Invoke the handler for a received packet
 *
 * Determine what function handles a received packet, based on its endpoint field.
 */
void Client::dispatchPacket(const struct rpc_header &hdr, const struct cbor_item_t *payload) {
    PLOG_VERBOSE << fmt::format("client {} received packet to ep ${:02x} ({} bytes)",
            this->socket, hdr.endpoint, hdr.length);

    switch(hdr.endpoint) {
        // update client config
        case kRpcEndpointBroadcastConfig:
            this->updateBroadcastConfig(payload);
            break;
        // ignore nops
        case kRpcEndpointNoOp:
            break;
        // unknown message type
        default:
            throw std::runtime_error(fmt::format("unknown rpc endpoint ${:02x}", hdr.endpoint));
    }
}

/**
 * @brief Update a client's broadcast config
 *
 * Parse a message sent to the broadcast config endpoint to update the client's desired broadcast
 * types. The following keys can be specified in the CBOR map payload, as booleans indicating
 * whether the corresponding events are sent:
 *
 * - touch: Touch events (position updates, up/down)
 * - button: Physical button presses (up/down events)
 * - encoder: Encoder rotation events (deltas)
 *
 * @remark If a key is absent from the payload, its current value is _not_ changed.
 */
void Client::updateBroadcastConfig(const struct cbor_item_t *item) {
    if(auto touch = Util::CborMapGet(item, "touch")) {
        if(cbor_float_ctrl_is_ctrl(touch)) {
            this->wantsTouchEvents = cbor_get_bool(touch);
        }
    }
    if(auto button = Util::CborMapGet(item, "button")) {
        if(cbor_float_ctrl_is_ctrl(button)) {
            this->wantsButtonEvents = cbor_get_bool(button);
        }
    }
    if(auto encoder = Util::CborMapGet(item, "encoder")) {
        if(cbor_float_ctrl_is_ctrl(encoder)) {
            this->wantsEncoderEvents = cbor_get_bool(encoder);
        }
    }

    PLOG_VERBOSE << "client " << this->socket << " enabled broadcasts: " <<
        (this->wantsTouchEvents ? "touch " : "") << (this->wantsButtonEvents ? "button " : "")
        << (this->wantsEncoderEvents ? "encoder " : "");
}
