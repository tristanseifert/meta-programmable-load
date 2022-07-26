#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <cbor.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <type_traits>
#include <system_error>

#include <fmt/core.h>
#include <plog/Log.h>
#include <rpc/types.h>

#include "Config.h"
#include "DataStore.h"
#include "RpcServer.h"
#include "Types.h"
#include "watchdog.h"

// declared in main.cpp
extern std::atomic_bool gRun;

/**
 * @brief Initialize the listening socket
 *
 * Create and bind the domain socket used for RPC requests.
 */
void RpcServer::initSocket() {
    int err;
    const auto path = Config::GetRpcSocketPath().c_str();

    // create the socket
    err = socket(AF_UNIX, SOCK_STREAM, 0);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "create rpc socket");
    }

    this->listenSock = err;

    // delete previous file, if any, then bind to that path
    PLOG_DEBUG << "RPC socket path: '" << path << "'";

    err = unlink(path);
    if(err == -1 && errno != ENOENT) {
        throw std::system_error(errno, std::generic_category(), "unlink rpc socket");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    err = bind(this->listenSock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "bind rpc socket");
    }

    // make listening socket non-blocking (to allow accept calls)
    err = fcntl(this->listenSock, F_GETFL);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "get rpc socket flags");
    }

    err = fcntl(this->listenSock, F_SETFL, err | O_NONBLOCK);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "set rpc socket flags");
    }

    // set the permission of the socket to allow all to connect
    err = fchmod(this->listenSock, 0777);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "set rpc socket permissions");
    }

    // allow clients to connect
    err = listen(this->listenSock, kListenBacklog);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "listen rpc socket");
    }

}

/**
 * @brief Initialize the event loop.
 */
void RpcServer::initEventLoop() {
    // create the event base
    this->evbase = event_base_new();
    if(!this->evbase) {
        throw std::runtime_error("failed to allocate event_base");
    }

    // create built-in events
    this->initWatchdogEvent();
    this->initSignalEvents();
    this->initSocketEvent();
}

/**
 * @brief Create watchdog event
 *
 * Create a timer event with half of the period of the watchdog timer. Every time the event fires,
 * it will kick the watchdog to ensure we don't get killed.
 */
void RpcServer::initWatchdogEvent() {
    // bail if watchdog is disabled
    if(!Watchdog::IsActive()) {
        PLOG_VERBOSE << "watchdog disabled, skipping event creation";
        return;
    }

    // get interval
    const auto usec = Watchdog::GetInterval().count();

    // create and add event
    this->watchdogEvent = event_new(this->evbase, -1, EV_PERSIST, [](auto, auto, auto ctx) {
        Watchdog::Kick();
    }, this);
    if(!this->watchdogEvent) {
        throw std::runtime_error("failed to allocate watchdog event");
    }

    struct timeval tv{
        .tv_sec  = static_cast<time_t>(usec / 1'000'000U),
        .tv_usec = static_cast<suseconds_t>(usec % 1'000'000U),
    };

    evtimer_add(this->watchdogEvent, &tv);
}

/**
 * @brief Create termination signal events
 *
 * Create an event that watches for POSIX signals that indicate we should restart: specifically,
 * this is SIGINT, SIGTERM, and SIGHUP.
 */
void RpcServer::initSignalEvents() {
    size_t i{0};

    for(const auto signum : kEvents) {
        auto ev = evsignal_new(this->evbase, signum, [](auto fd, auto what, auto ctx) {
            reinterpret_cast<RpcServer *>(ctx)->handleTermination();
        }, this);
        if(!ev) {
            throw std::runtime_error("failed to allocate signal event");
        }

        event_add(ev, nullptr);
        this->signalEvents[i++] = ev;
    }
}

/**
 * @brief Initialize the event for the listening socket
 *
 * This event fires any time a client connects. It serves as the primary event.
 */
void RpcServer::initSocketEvent() {
    this->listenEvent = event_new(this->evbase, this->listenSock, (EV_READ | EV_PERSIST),
            [](auto fd, auto what, auto ctx) {
        try {
            reinterpret_cast<RpcServer *>(ctx)->acceptClient();
        } catch(const std::exception &e) {
            PLOG_ERROR << "failed to accept client: " << e.what();
        }
    }, this);
    if(!this->listenEvent) {
        throw std::runtime_error("failed to allocate listen event");
    }

    event_add(this->listenEvent, nullptr);
}

/**
 * @brief Shut down the RPC server
 *
 * Terminate any remaining client connections, as well as close the main listening socket. We'll
 * also delete the socket file at this time.
 */
RpcServer::~RpcServer() {
    int err;

    // close and unlink listening socket
    PLOG_DEBUG << "Closing RPC server socket";

    close(this->listenSock);

    err = unlink(Config::GetRpcSocketPath().c_str());
    if(err == -1) {
        PLOG_ERROR << "failed to unlink socket: " << strerror(errno);
    }

    // close all clients
    PLOG_DEBUG << "Closing client connections";
    this->clients.clear();

    // release events
    event_free(this->listenEvent);
    for(auto ev : this->signalEvents) {
        if(!ev) {
            continue;
        }
        event_free(ev);
    }

    if(this->watchdogEvent) {
        event_free(this->watchdogEvent);
    }

    // shut down event loop
    event_base_free(this->evbase);
}

/**
 * @brief Wait for a server event
 *
 * Block on the listening socket, and all active client sockets, to wait for data to be received or
 * some other type of event.
 *
 * @remark This will sit here basically forever; kicking of the watchdog is implemented by means
 *         of a timer callback that runs periodically.
 */
void RpcServer::run() {
    event_base_dispatch(this->evbase);
}



/**
 * @brief Accept a single waiting client
 *
 * Accepts one client on the listening socket, and sets up a connection struct for it.
 */
void RpcServer::acceptClient() {
    // accept client
    int fd = accept(this->listenSock, nullptr, nullptr);
    if(fd == -1) {
        throw std::system_error(errno, std::generic_category(), "accept");
    }

    // convert socket to non-blocking
    int err = evutil_make_socket_nonblocking(fd);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "evutil_make_socket_nonblocking");
    }

    // set up our bookkeeping for it and add it to event loop
    auto cl = std::make_shared<Client>(this, fd);
    this->clients.emplace(cl->event, std::move(cl));

    PLOG_DEBUG << "Accepted client " << fd << " (" << this->clients.size() << " total)";
}

/**
 * @brief A client connection is ready to read
 *
 * Reads data from the given client connection.
 */
void RpcServer::handleClientRead(struct bufferevent *ev) {
    // get client struct
    auto &client = this->clients.at(ev);

    // read client data
    /**
     * TODO: rework this so data is buffered over time in the client receive buffer, rather than
     * being overwritten each time, in case clients decide to do partial writes down the line!
     */
    auto buf = bufferevent_get_input(ev);
    const size_t pending = evbuffer_get_length(buf);

    client->receiveBuf.resize(pending);
    int read = evbuffer_remove(buf, static_cast<void *>(client->receiveBuf.data()), pending);

    if(read == -1) {
        throw std::runtime_error("failed to drain client read buffer");
    }

    // read the header
    if(read < sizeof(struct rpc_header)) {
        // we haven't yet read enough bytes; this should never happen, so abort
        throw std::runtime_error(fmt::format("read too few bytes ({}) from client", read));
    }

    const auto hdr = reinterpret_cast<const struct rpc_header *>(client->receiveBuf.data());

    if(hdr->version != kRpcVersionLatest) {
        throw std::runtime_error(fmt::format("unsupported rpc version ${:04x}", hdr->version));
    } else if(hdr->length < sizeof(struct rpc_header)) {
        throw std::runtime_error(fmt::format("invalid header length ({}, too short)",
                    hdr->length));
    }

    const auto payloadLen = hdr->length - sizeof(struct rpc_header);
    if(payloadLen > client->receiveBuf.size()) {
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
        switch(hdr->endpoint) {
            case kConfigQuery:
                this->doCfgQuery(client->receiveBuf, item, client);
                break;
            case kConfigUpdate:
                this->doCfgUpdate(client->receiveBuf, item, client);
                break;

            default:
                throw std::runtime_error(fmt::format("unknown rpc endpoint ${:02x}", hdr->endpoint));
        }
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
void RpcServer::handleClientEvent(struct bufferevent *ev, const size_t flags) {
    auto &client = this->clients.at(ev);

    // connection closed
    if(flags & BEV_EVENT_EOF) {
        PLOG_DEBUG << "Client " << client->socket << " closed connection";
    }
    // IO error
    else if(flags & BEV_EVENT_ERROR) {
        PLOG_DEBUG << "Client " << client->socket << " error: flags=" << flags;
    }

    // in either case, remove the client struct
    this->clients.erase(ev);
}

/**
 * @brief Terminate a client connection
 *
 * Aborts the connection to the client, releasing all its resources. This is usually done in
 * response to an error of some sort.
 */
void RpcServer::abortClient(struct bufferevent *ev) {
    this->clients.erase(ev);
}

/**
 * @brief Extract the property key name from a get/set request
 *
 * Given the root of a CBOR message, extract the property key (this is a string under the root
 * item with the string key `key`) from it.
 *
 * @param item Map containing the request (usually the root of the request)
 *
 * @return Property key name the query pertains to
 *
 * @remark The input item _must_ be a map.
 */
std::string RpcServer::ExtractKeyName(struct cbor_item_t *item) {
    std::string keyName;

    // validate map
    const auto numKeys = cbor_map_size(item);

    if(!numKeys) {
        throw std::runtime_error("invalid payload: map has no keys");
    }

    // iterate over all keys to extract the key name and value
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

        // check if it's a known key value
        if(!strncmp(keyStr, "key", keyStrLen)) {
            if(!cbor_isa_string(pair.value)) {
                throw std::runtime_error("invalid type for `key` (expected string)");
            } else if(!cbor_string_is_definite(pair.value)) {
                throw std::runtime_error("indefinite strings not supported");
            }

            auto handle = cbor_string_handle(pair.value);
            const auto valueStrLen = cbor_string_length(pair.value);

            if(!handle) {
                throw std::runtime_error("failed to get key name string");
            }

            keyName = {reinterpret_cast<const char *>(handle), valueStrLen};
        }
        // ignore other keys for forward compat
    }

    return keyName;
}


/**
 * @brief Process a query request to the config endpoint
 *
 * Requests here are simple CBOR serialized get messages to which we'll send a single response;
 * we use the encode/decode helpers in rpc-helper static library for this.
 *
 * @param packet Memory region containing the full RPC packet, starting at the header
 * @param item Root CBOR item in payload of message
 * @param client Pointer to client this request originated on
*/
void RpcServer::doCfgQuery(std::span<const std::byte> packet, cbor_item_t *item,
        const std::shared_ptr<Client> &client) {
    // validate inputs
    if(!cbor_isa_map(item)) {
        throw std::invalid_argument("invalid payload: expected map");
    }

    const auto keyName = ExtractKeyName(item);
    if(keyName.empty()) {
        throw std::runtime_error("failed to get key name (wtf)");
    }

    // get operation flags
    Flags flags{Flags::None};
    this->getCfgQueryFlags(item, flags);

    // TODO: validate access
    PLOG_VERBOSE << fmt::format("key name = '{}' flags = {:04x}", keyName,
            static_cast<uintptr_t>(flags));
    auto result = this->store->getKey(keyName);

    const auto hdr = reinterpret_cast<const struct rpc_header *>(packet.data());
    this->sendKeyValue(hdr, client, keyName, result, flags);
}

/**
 * @brief Parse input request and extract flags
 *
 * @param item CBOR map containing request data
 * @param outFlags Variable to receive all flags
 */
void RpcServer::getCfgQueryFlags(struct cbor_item_t *item, Flags &outFlags) {
    auto keys = cbor_map_handle(item);

    for(size_t i = 0; i < cbor_map_size(item); i++) {
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

        // check if it's a known key value
        if(!strncmp(keyStr, "forceFloat", keyStrLen)) {
            if(!cbor_isa_float_ctrl(pair.value) || !cbor_is_bool(pair.value)) {
                throw std::runtime_error("invalid type for `key` (expected bool)");
            }

            if(cbor_get_bool(pair.value)) {
                outFlags = static_cast<Flags>(outFlags | Flags::SinglePrecisionFloat);
            }
        }
    }
}

/**
 * @brief Serialize the value of a key and send it
 *
 * Prepares a message containing the specified key value, and send it as a response to a query with
 * the specified tag.
 *
 * @param hdr Message to send this as a reply to, or nullptr if none
 * @param client Client connection to send the response to
 * @param key Key name that was queried
 * @param value Value of the key
 * @param flags Flags to modify the behavior of the routine
 */
void RpcServer::sendKeyValue(const struct rpc_header *hdr, const std::shared_ptr<Client> &client,
        const std::string &key, const PropertyValue &value, const Flags flags) {
    bool hasValue;
    const bool found = !std::holds_alternative<std::monostate>(value);
    const bool outputValue = !(flags & Flags::ExcludeValue);

    // set up the generic part of the response
    cbor_item_t *root = cbor_new_definite_map((found ? 3 : 2) - (outputValue ? 0 : 1));
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("key")),
        .value = cbor_move(cbor_build_string(key.c_str()))
    });

    // add the current value (if any)
    if(outputValue) {
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            // null value
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                cbor_map_add(root, (struct cbor_pair) {
                    .key = cbor_move(cbor_build_string("value")),
                    .value = cbor_move(cbor_new_null())
                });
                hasValue = true;
            }
            // UTF-8 string
            else if constexpr (std::is_same_v<T, std::string>) {
                cbor_map_add(root, (struct cbor_pair) {
                    .key = cbor_move(cbor_build_string("value")),
                    .value = cbor_move(cbor_build_string(arg.c_str()))
                });
                hasValue = true;
            }
            // byte vector (BLOB)
            else if constexpr (std::is_same_v<T, Blob>) {
                cbor_map_add(root, (struct cbor_pair) {
                    .key = cbor_move(cbor_build_string("value")),
                    .value = cbor_move(cbor_build_bytestring(reinterpret_cast<cbor_data>(arg.data()),
                                arg.size()))
                });
                hasValue = true;
            }
            // integer
            else if constexpr (std::is_same_v<T, uint64_t>) {
                cbor_map_add(root, (struct cbor_pair) {
                    .key = cbor_move(cbor_build_string("value")),
                    .value = cbor_move(cbor_build_uint64(arg))
                });
                hasValue = true;
            }
            // double (or float if requested)
            else if constexpr (std::is_same_v<T, double>) {
                cbor_map_add(root, (struct cbor_pair) {
                    .key = cbor_move(cbor_build_string("value")),
                    .value = cbor_move((flags & Flags::SinglePrecisionFloat) ? cbor_build_float4(arg) :
                            cbor_build_float8(arg))
                });
                hasValue = true;
            }
            // boolean
            else if constexpr (std::is_same_v<T, bool>) {
                cbor_map_add(root, (struct cbor_pair) {
                    .key = cbor_move(cbor_build_string("value")),
                    .value = cbor_move(cbor_build_bool(arg))
                });
                hasValue = true;
            }
            // no value
            else if constexpr (std::is_same_v<T, std::monostate>) {
                hasValue = false;
            }
        }, value);
    } else {
        hasValue = !std::holds_alternative<std::monostate>(value);
    }

    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string((flags & Flags::IsSetRequest) ? "updated" : "found")),
        .value = cbor_move(cbor_build_bool(hasValue))
    });

    // serialize the payload
    size_t rootBufLen;
    unsigned char *rootBuf{nullptr};
    const size_t serializedBytes = cbor_serialize_alloc(root, &rootBuf, &rootBufLen);
    cbor_decref(&root);

    // send it as a reply (ensuring we don't leak the above buffer… this sucks lol)
    try {
        client->replyTo(*hdr, {reinterpret_cast<const std::byte *>(rootBuf), serializedBytes});
        free(rootBuf);
    } catch(const std::exception &) {
        free(rootBuf);
        throw;
    }
}


/**
 * @brief Process a request to update a config key
 *
 * This request should contain both a `key` and a `value` entry, where the latter is either an
 * UTF-8 string, byte string (blob), an integer, or floating point value.
 *
 * @remark If the value is specified as a boolean, the value is coerced to an unsigned integer
 *         (such that false = zero, true = an implementation-defined non-zero value)
 *
 * @param packet Memory region containing the full RPC packet, starting at the header
 * @param item Root CBOR item in payload of message
 * @param client Pointer to client this request originated on
 */
void RpcServer::doCfgUpdate(std::span<const std::byte> packet, cbor_item_t *item,
        std::shared_ptr<Client> &client) {
    PropertyValue value;

    // validate inputs
    if(!cbor_isa_map(item)) {
        throw std::invalid_argument("invalid payload: expected map");
    }

    // get key name
    const auto keyName = ExtractKeyName(item);
    if(keyName.empty()) {
        throw std::runtime_error("failed to get key name (wtf)");
    }

    // TODO: validate key access

    /*
     * Get the value of the key
     *
     * This checks for the "value" key in the input map, and extracts its value if it matches one
     * of the desired/supported types. Note that we skip some validation here on the map (namely
     * whether there's more than one key) since that's already been done by `ExtractKeyName`
     */
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

        // if it's not the `value` key, bail
        if(strncmp(keyStr, "value", keyStrLen) != 0) {
            continue;
        }

        // figure out its type
        if(cbor_isa_string(pair.value)) { // UTF-8 string
            if(!cbor_string_is_definite(pair.value)) {
                throw std::runtime_error("indefinite strings not supported");
            }
            value = reinterpret_cast<const char *>(cbor_string_handle(pair.value));
        } else if(cbor_isa_bytestring(pair.value)) { // blob
            // reject indefinite blobs
            if(!cbor_bytestring_is_definite(pair.value)) {
                throw std::runtime_error("indefinite bytestrings not supported");
            }

            const auto blobNumBytes = cbor_bytestring_length(pair.value);
            const auto blobData = cbor_bytestring_handle(pair.value);

            // copy the blob out
            std::vector<std::byte> buf;
            buf.resize(blobNumBytes);

            memcpy(buf.data(), blobData, blobNumBytes);
            value = buf;
        } else if(cbor_isa_uint(pair.value)) { // unsigned integer
            uint64_t temp;
            switch(cbor_int_get_width(pair.value)) {
                case CBOR_INT_8:
                    temp = cbor_get_uint8(pair.value);
                    break;
                case CBOR_INT_16:
                    temp = cbor_get_uint16(pair.value);
                    break;
                case CBOR_INT_32:
                    temp = cbor_get_uint32(pair.value);
                    break;
                case CBOR_INT_64:
                    temp = cbor_get_uint64(pair.value);
                    break;
            }
            value = temp;
        } else if(cbor_isa_float_ctrl(pair.value)) { // float or bool
            // probably a bool?
            if(cbor_float_ctrl_is_ctrl(pair.value)) {
                value = cbor_get_bool(pair.value);
            }
            // a float value: read it out as a double
            else {
                value = cbor_float_get_float(pair.value);
            }
        } else {
            throw std::invalid_argument(fmt::format("invalid value type {}",
                        cbor_typeof(pair.value)));
        }
    }

    // perform update
    this->store->setKey(keyName, value);

    // send a reply (assume success if we get here)
    const auto hdr = reinterpret_cast<const struct rpc_header *>(packet.data());
    this->sendKeyValue(hdr, client, keyName, value,
            static_cast<Flags>(Flags::IsSetRequest | Flags::ExcludeValue));
}

/**
 * @brief Handle a signal that indicates the process should terminate
 */
void RpcServer::handleTermination() {
    PLOG_INFO << "Received signal, terminating...";

    gRun = false;
    event_base_loopbreak(this->evbase);
}



/**
 * @brief Create a new client data structure
 *
 * Initialize a buffer event (used for event notifications, like the connection being closed; as
 * well as when buffered data is available) for the client.
 *
 * @param server RPC server to which the client connected
 * @param fd File descriptor for client (we take ownership of this)
 */
RpcServer::Client::Client(RpcServer *server, const int fd) : socket(fd) {
    // create the event
    this->event = bufferevent_socket_new(server->evbase, this->socket, 0);
    if(!this->event) {
        throw std::runtime_error("failed to create bufferevent");
    }

    // set watermark: don't invoke read callback til a full header has been read at least
    bufferevent_setwatermark(this->event, EV_READ, sizeof(struct rpc_header),
            EV_RATE_LIMIT_MAX);

    // install callbacks
    bufferevent_setcb(this->event, [](auto bev, auto ctx) {
        try {
            reinterpret_cast<RpcServer *>(ctx)->handleClientRead(bev);
        } catch(const std::exception &e) {
            PLOG_ERROR << "Failed to handle client read: " << e.what();
            reinterpret_cast<RpcServer *>(ctx)->abortClient(bev);
        }
    }, nullptr, [](auto bev, auto what, auto ctx) {
        try {
            reinterpret_cast<RpcServer *>(ctx)->handleClientEvent(bev, what);
        } catch(const std::exception &e) {
            PLOG_ERROR << "Failed to handle client event: " << e.what();
            reinterpret_cast<RpcServer *>(ctx)->abortClient(bev);
        }
    }, server);

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
RpcServer::Client::~Client() {
    if(this->event) {
        bufferevent_free(this->event);
    }

    if(this->socket != -1) {
        close(this->socket);
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
void RpcServer::Client::replyTo(const struct rpc_header &req, std::span<const std::byte> payload) {
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
void RpcServer::Client::send(std::span<const std::byte> buf) {
    int err;
    err = bufferevent_write(this->event, buf.data(), buf.size());

    // IO failed
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "write rpc reply");
    }
}
