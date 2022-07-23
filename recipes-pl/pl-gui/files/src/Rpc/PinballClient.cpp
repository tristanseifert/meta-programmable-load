#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cerrno>
#include <string>
#include <system_error>

#include <cbor.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <fmt/format.h>
#include <plog/Log.h>
#include <shittygui/Event.h>
#include <shittygui/Screen.h>

#include "Gui/Renderer.h"
#include "Utils/Cbor.h"
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
 * @brief Send a raw packet to the remote
 *
 * This assumes the packet already has a `struct rpc_header` prepended.
 */
void PinballClient::sendRaw(std::span<const std::byte> payload) {
    int err = bufferevent_write(this->bev, payload.data(), payload.size());

    if(err == -1) {
        throw std::runtime_error("failed to send packet");
    }
}

/**
 * @brief Send a packet to the remote, adding a packet header
 *
 * Generate a full packet (including packet header) and send it to the remote.
 *
 * @return Tag value associated with the packet
 */
uint8_t PinballClient::sendPacket(const uint8_t endpoint, std::span<const std::byte> payload) {
    std::vector<std::byte> buffer;
    buffer.resize(sizeof(struct rpc_header) + payload.size(), std::byte{0});

    // build up the header
    auto hdr = reinterpret_cast<struct rpc_header *>(buffer.data());
    hdr->version = kRpcVersionLatest;
    hdr->length = sizeof(*hdr) + payload.size();
    hdr->endpoint = endpoint;

    do {
        hdr->tag = ++this->nextTag;
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
 * @brief Process a received user event broadcast
 *
 * Generate an event and inject it into the GUI subsystem as appropriate.
 */
void PinballClient::processUiEvent(std::span<const std::byte> payload) {
    struct cbor_load_result result{};

    // set up decoder
    auto item = cbor_load(reinterpret_cast<const cbor_data>(payload.data()), payload.size(),
            &result);
    if(result.error.code != CBOR_ERR_NONE) {
        throw std::runtime_error(fmt::format("cbor_load failed: {} (at {})", result.error.code,
                    result.error.position));
    }
    if(!cbor_isa_map(item)) {
        cbor_decref(&item);
        throw std::invalid_argument("invalid payload: expected map");
    }

    /*
     * Figure out the type of event we're dealing with by iterating the map until we find the
     * `type` key.
     */
    enum class EventType {
        Unknown, Touch, Button, Encoder
    };

    EventType type{EventType::Unknown};

    auto typeKey = Util::CborMapGet(item, "type");
    if(!typeKey || !cbor_isa_string(typeKey)) {
        cbor_decref(&item);
        throw std::runtime_error("missing or invalid event type key");
    }

    const std::string_view value{reinterpret_cast<const char *>(cbor_string_handle(typeKey)),
        cbor_string_length(typeKey)};

    if(value == "touch") {
        type = EventType::Touch;
    } else if(value == "button") {
        type = EventType::Button;
    } else if(value == "encoder") {
        type = EventType::Encoder;
    } else {
        PLOG_WARNING << fmt::format("Unknown UI event type '{}'", value);
        goto beach;
    }

    // invoke the appropriate handler
    switch(type) {
        case EventType::Touch:
            this->processUiTouchEvent(item);
            break;

        // shouldn't get here
        default:
            break;
    }

beach:;
    // clean up
    cbor_decref(&item);
}

/**
 * @brief Process a touch event
 *
 * This is a map which has a `touchData` key, which in turn is another map indexed by the touch
 * index. Each index may either be set to null, or another map which contains information about
 * a particular touch.
 */
void PinballClient::processUiTouchEvent(const struct cbor_item_t *root) {
    // get touch data
    auto touchData = Util::CborMapGet(root, "touchData");
    if(!touchData) {
        throw std::runtime_error("invalid touch event (missing touchData payload)");
    }

    // iterate over all touches we've event data for
    const auto numTouches = cbor_map_size(touchData);
    auto touches = cbor_map_handle(touchData);

    for(size_t j = 0; j < numTouches; j++) {
        auto &touchPair = touches[j];

        if(!cbor_isa_uint(touchPair.key)) {
            throw std::runtime_error("invalid touch key");
        } else if(!cbor_isa_map(touchPair.value) && !cbor_isa_float_ctrl(touchPair.value)) {
            throw std::runtime_error("invalid touch value (expected map or null)");
        }

        const auto touchId = Util::CborReadUint(touchPair.key);

        // currently, ignore the second+ touches
        if(touchId != 0) {
            continue;
        }

        // emit the appropriate touch event
        if(cbor_is_null(touchPair.value)) {
            this->emitTouchUp();
        } else {
            uint16_t posX{0}, posY{0};

            // get the position of the touch
            auto posArray = Util::CborMapGet(touchPair.value, "position");
            if(!posArray || !cbor_isa_array(posArray)) {
                throw std::runtime_error("invalid touch position (expected array)");
            }

            posX = Util::CborReadUint(cbor_array_get(posArray, 0));
            posY = Util::CborReadUint(cbor_array_get(posArray, 1));

            // emit an event
            this->emitTouchDown(posX, posY);
        }
    }
}

/**
 * @brief Send a touch event to the GUI layer
 */
void PinballClient::emitTouchEvent(const int16_t x, const int16_t y, const bool isDown) {
    // get the GUI instance
    auto gui = this->gui.lock();
    if(!gui) {
        PLOG_WARNING << "GUI went away, can't send touch event!";
        return;
    }

    gui->getScreen()->queueEvent(shittygui::event::Touch({x, y}, isDown));

    if(kLogEvents) {
        PLOG_VERBOSE << fmt::format("Touch event ({}, {}) {}", x, y, isDown ? "down" : "up");
    }
}


/**
 * @brief Update the remote on what types of broadcast packets we wish to receive
 *
 * @param mask Logical OR of PinballBroadcastType
 */
void PinballClient::setDesiredBroadcasts(const PinballBroadcastType mask) {
    // set up the CBOR map
    auto root = cbor_new_definite_map(3);

    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("touch")),
        .value = cbor_move(cbor_build_bool(mask & PinballBroadcastType::TouchEvent))
    });

    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("button")),
        .value = cbor_move(cbor_build_bool(mask & PinballBroadcastType::ButtonEvent))
    });

    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("encoder")),
        .value = cbor_move(cbor_build_bool(mask & PinballBroadcastType::EncoderEvent))
    });

    // encode it and send it as a packet
    size_t rootBufLen;
    unsigned char *rootBuf{nullptr};
    const size_t serializedBytes = cbor_serialize_alloc(root, &rootBuf, &rootBufLen);
    cbor_decref(&root);

    try {
        this->sendPacket(kRpcEndpointBroadcastConfig,
                {reinterpret_cast<std::byte *>(rootBuf), serializedBytes});
        free(rootBuf);
    } catch(const std::exception &e) {
        free(rootBuf);
        throw;
    }
}
