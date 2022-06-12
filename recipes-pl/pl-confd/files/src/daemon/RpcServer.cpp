#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cbor.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <system_error>

#include <fmt/core.h>
#include <plog/Log.h>
#include <rpc/types.h>

#include "Config.h"
#include "RpcServer.h"
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

    // shut down event loop
    event_base_free(this->evbase);

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
        throw std::runtime_error(fmt::format("invalid header length ({})", hdr->length));
    }

    // invoke endpoint handler
    switch(hdr->endpoint) {
        case kConfigQuery:
            this->doCfgQuery(client->receiveBuf, client);
            break;

        default:
            throw std::runtime_error(fmt::format("unknown rpc endpoint ${:02x}", hdr->endpoint));
    }
}

/**
 * @brief A client connection event occurred
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
 * @brief Process a request to the config endpoint
 *
 * Requests here are simple CBOR serialized get/set messages to which we'll send a single response;
 * we use the encode/decode helpers in rpc-helper static library for this.
 *
 * @param packet Memory region containing the full RPC packet, starting at the header
 * @param client Pointer to client this request originated on
*/
void RpcServer::doCfgQuery(std::span<const std::byte> packet, std::shared_ptr<Client> &client) {
    // validate header and the payload size
    const auto hdr = reinterpret_cast<const struct rpc_header *>(packet.data());
    if(hdr->length <= sizeof(struct rpc_header)) {
        throw std::runtime_error("payload is required for kConfigQuery");
    }

    // create CBOR decoder for message
    auto payload = packet.subspan(offsetof(struct rpc_header, payload));
    struct cbor_load_result result{};

    auto item = cbor_load(reinterpret_cast<const cbor_data>(payload.data()), payload.size(),
            &result);
    if(result.error.code != CBOR_ERR_NONE) {
        throw std::runtime_error(fmt::format("cbor_load failed: {} (at ${:x})", result.error.code,
                    result.error.position));
    }

    // extract the config key

    // perform operation (set if the `value` key exists)

    // clean up
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
    if(this->socket != -1) {
        close(this->socket);
    }

    if(this->event) {
        bufferevent_free(this->event);
    }
}
