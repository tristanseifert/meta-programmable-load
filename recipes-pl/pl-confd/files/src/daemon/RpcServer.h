#ifndef RPCSERVER_H
#define RPCSERVER_H

#include <sys/signal.h>

#include <array>
#include <cstddef>

/**
 * @brief Remote access interface
 *
 * This class opens the listening socket for the RPC interface and handles requests there. It
 * hides the management and interfacing with clients behind a single run method, which takes
 * advantage of system facilities to wait on multiple file descriptors at once.
 */
class RpcServer {
    public:
        /**
         * @brief Initialize RPC server
         */
        RpcServer() {
            this->initSocket();
            this->initEventLoop();
        }

        ~RpcServer();

        void run();

    private:
        void initSocket();

        void initEventLoop();
        void initWatchdogEvent();
        void initSignalEvents();
        void initSocketEvent();

        void handleTermination();

        void acceptClient();

    private:
        /// Maximum amount of clients that may be waiting to be accepted at once
        constexpr static const size_t kListenBacklog{5};

        /// Main RPC listening socket
        int listenSock{-1};
        /// event for listening socket receiving a client
        struct event *listenEvent{nullptr};

        /// signals to intercept
        constexpr static const std::array<int, 3> kEvents{{SIGINT, SIGTERM, SIGHUP}};
        /// termination signal events
        std::array<struct event *, 3> signalEvents;

        /// watchdog kicking timer event (if watchdog is active)
        struct event *watchdogEvent{nullptr};

        /// libevent main loop
        struct event_base *evbase{nullptr};
};

#endif
