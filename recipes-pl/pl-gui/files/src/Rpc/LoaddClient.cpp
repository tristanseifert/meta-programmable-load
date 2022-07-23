#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>

#include <cbor.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <fmt/format.h>
#include <plog/Log.h>

#include "Utils/Cbor.h"
#include "EventLoop.h"
#include "LoaddClient.h"
#include "RpcTypes.h"

using namespace Rpc;

/**
 * @brief RPC message endpoints
 */
enum rpc_endpoint {
    kRpcEndpointNoOp                    = 0x00,
    kRpcEndpointMeasurement             = 0x10,
};

/**
 * @brief Open an RPC connection to loadd
 *
 * @param ev Event loop to install events on
 * @param rpcSocketPath Path to the UNIX domain socket loadd is listening on
 */
LoaddClient::LoaddClient(const std::shared_ptr<EventLoop> &ev,
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
    this->fd = this->connectToLoadd();

    this->bev = bufferevent_socket_new(evbase, this->fd, 0);
    if(!this->bev) {
        throw std::runtime_error("failed to create bufferevent (loadd)");
    }

    bufferevent_setwatermark(this->bev, EV_READ, sizeof(struct rpc_header),
            EV_RATE_LIMIT_MAX);

    bufferevent_setcb(this->bev, [](auto bev, auto ctx) {
        try {
            reinterpret_cast<LoaddClient *>(ctx)->handleLoaddRead(bev);
        } catch(const std::exception &e) {
            PLOG_ERROR << "Failed to handle loadd read: " << e.what();
            std::terminate();
        }
    }, nullptr, [](auto bev, auto what, auto ctx) {
        try {
            reinterpret_cast<LoaddClient *>(ctx)->handleLoaddEvent(bev, what);
        } catch(const std::exception &e) {
            PLOG_ERROR << "Failed to handle loadd event: " << e.what();
            std::terminate();
        }
    }, this);

    // add events to rpc server's run loop
    err = bufferevent_enable(this->bev, EV_READ);
    if(err == -1) {
        throw std::runtime_error("failed to enable bufferevent (loadd)");
    }
}

/**
 * @brief Establish connection to loadd
 *
 * @return File descriptor for a domain socket connected to loadd daemon
 */
int LoaddClient::connectToLoadd() {
    int fd, err;

    // create the socket
    fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd == -1) {
        throw std::system_error(errno, std::generic_category(), "create loadd socket");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, this->socketPath.native().c_str(), sizeof(addr.sun_path) - 1);

    // dial it
    err = connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if(err == -1) {
        close(fd);
        throw std::system_error(errno, std::generic_category(), "dial loadd socket");
    }

    // mark the socket to use non-blocking IO (for libevent)
    err = evutil_make_socket_nonblocking(fd);
    if(err == -1) {
        close(fd);
        throw std::system_error(errno, std::generic_category(),
                "evutil_make_socket_nonblocking (loadd)");
    }

    // it's been connected :D
    return fd;
}

/**
 * @brief Shut down loadd connection
 *
 * Release the event from the event loop and then the underlying socket.
 */
LoaddClient::~LoaddClient() {
    if(this->bev) {
        bufferevent_free(this->bev);
    }
}



/**
 * @brief Handle a packet received from loadd
 *
 * Identify what the packet is and route it appropriately.
 */
void LoaddClient::handleLoaddRead(struct bufferevent *ev) {
    // pull it out and into our read buffer
    auto buf = bufferevent_get_input(bev);
    const size_t pending = evbuffer_get_length(buf);

    this->rxBuf.resize(pending);
    int read = evbuffer_remove(buf, static_cast<void *>(this->rxBuf.data()), pending);

    if(read == -1) {
        throw std::runtime_error("failed to drain loadd read buffer");
    }

    // validate header
    if(pending < sizeof(struct rpc_header)) {
        PLOG_WARNING << fmt::format("insufficient loadd read (got {})", pending);
        return;
    }

    auto hdr = reinterpret_cast<const struct rpc_header *>(this->rxBuf.data());
    if(hdr->version != kRpcVersionLatest) {
        PLOG_WARNING << fmt::format("unknown loadd rpc version ${:04x}", hdr->version);
        return;
    } else if(hdr->length < sizeof(struct rpc_header)) {
        PLOG_WARNING << fmt::format("invalid rpc packet size ({} bytes)", hdr->length);
        return;
    }

    // invoke the handler
    std::span<const std::byte> payload{reinterpret_cast<const std::byte *>(hdr->payload),
        hdr->length - sizeof(*hdr)};

    switch(hdr->endpoint) {
        case kRpcEndpointMeasurement:
            this->processMeasurement(payload);
            break;

        default:
            PLOG_WARNING << fmt::format("unknown loadd rpc type ${:02x}", hdr->endpoint);
            break;
    }
}

/**
 * @brief Handle an event on the loadd connection
 *
 * @param flags Bufferevent status flag
 */
void LoaddClient::handleLoaddEvent(struct bufferevent *, const uintptr_t flags) {
    // connection closed
    if(flags & BEV_EVENT_EOF) {
        PLOG_WARNING << "loaddd closed connection :(";

        this->fd = 0;
    }
    // IO error
    else if(flags & BEV_EVENT_ERROR) {
        PLOG_WARNING << "loadd io error: flags=" << flags;
    }
}



/**
 * @brief Process a measurement packet
 *
 * Decode the measurements inside the packet and invoke all measurement packets. This packet type
 * is an unsolicited periodic message sent by the firmware.
 *
 * @param payload Buffer containing the CBOR encoded payload packet
 */
void LoaddClient::processMeasurement(std::span<const std::byte> payload) {
    struct cbor_load_result result{};
    Measurement meas;

    // set up decoder
    auto item = cbor_load(reinterpret_cast<const cbor_data>(payload.data()), payload.size(),
            &result);
    if(result.error.code != CBOR_ERR_NONE) {
        throw std::runtime_error(fmt::format("cbor_load failed: {} (at {})", result.error.code,
                    result.error.position));
    }
    if(!cbor_isa_map(item)) {
        throw std::invalid_argument("invalid payload: expected map");
    }

    const auto numKeys = cbor_map_size(item);
    auto keys = cbor_map_handle(item);

    for(size_t i = 0; i < numKeys; i++) {
        auto &pair = keys[i];

        // validate key type: must be a string
        if(!cbor_isa_string(pair.key)) {
            throw std::runtime_error("invalid map key type (expected string)");
        }

        const auto keyStr = reinterpret_cast<const char *>(cbor_string_handle(pair.key));
        const auto keyStrLen = cbor_string_length(pair.key);

        if(!keyStr) {
            throw std::runtime_error("failed to get map key string");
        }

        // compare if it's one of the keys we're interested in
        if(!strncmp(keyStr, "v", keyStrLen)) {
            if(!cbor_isa_float_ctrl(pair.value)) {
                throw std::runtime_error("invalid voltage value (expected float)");
            }
            meas.voltage = cbor_float_get_float(pair.value);
        }
        else if(!strncmp(keyStr, "i", keyStrLen)) {
            if(!cbor_isa_float_ctrl(pair.value)) {
                throw std::runtime_error("invalid current value (expected float)");
            }
            meas.current = cbor_float_get_float(pair.value);
        }
        else if(!strncmp(keyStr, "t", keyStrLen)) {
            if(!cbor_isa_float_ctrl(pair.value)) {
                throw std::runtime_error("invalid temperature value (expected float)");
            }
            meas.temperature = cbor_float_get_float(pair.value);
        }
    }

    cbor_decref(&item);

    // invoke callbacks
    for(const auto &[token, cb] : this->measurementCallbacks) {
        cb(meas);
    }
}

/**
 * @brief Register a new measurement callback
 *
 * This is a callback that's invoked when we receive a measurement from the firmware.
 *
 * @return Measurement callback token, which can be used to remove the callback later
 */
uint32_t LoaddClient::addMeasurementCallback(const MeasurementCallback &cb) {
    uint32_t token;

    do {
        token = ++this->nextMeasurementCallbackToken;
    } while(!token || this->measurementCallbacks.contains(token));

    this->measurementCallbacks.emplace(token, cb);

    return token;
}

/**
 * @brief Remove a previously installed measurement callback
 *
 * @token A token value returned by registerMeasurementCallback
 *
 * @return Whether the callback was removed
 *
 * @seeAlso registerMeasurementCallback
 */
bool LoaddClient::removeMeasurementCallback(const uint32_t token) {
    return this->measurementCallbacks.erase(token);
}

