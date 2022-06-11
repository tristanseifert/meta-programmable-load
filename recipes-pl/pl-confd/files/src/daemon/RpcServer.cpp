#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <event2/event.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <system_error>

#include <plog/Log.h>

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
        reinterpret_cast<RpcServer *>(ctx)->acceptClient();
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
 * @brief Handle a signal that indicates the process should terminate
 */
void RpcServer::handleTermination() {
    PLOG_INFO << "Received signal, terminating...";

    gRun = false;
    event_base_loopbreak(this->evbase);
}

/**
 * @brief Accept a single waiting client
 *
 * Accepts one client on the listening socket, and sets up a connection struct for it.
 */
void RpcServer::acceptClient() {
    // TODO
}
