#ifndef RPCSERVER_H
#define RPCSERVER_H

#include <sys/signal.h>

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <unordered_map>

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
        /**
         * @brief Information for a single connected client
         *
         * This struct encapsulates all information about a connected client, including the events
         * used to wait for activity on the connection.
         */
        struct Client {
            /// Underlying client file descriptor
            int socket{-1};
            /// Socket buffer event (used for data ready to read + events)
            struct bufferevent *event{nullptr};
            /// message receive buffer
            std::vector<std::byte> receiveBuf;

            Client(RpcServer *, const int);
            ~Client();
        };

    private:
        void initSocket();

        void initEventLoop();
        void initWatchdogEvent();
        void initSignalEvents();
        void initSocketEvent();

        void acceptClient();
        void handleClientRead(struct bufferevent *);
        void handleClientEvent(struct bufferevent *, const size_t);
        void abortClient(struct bufferevent *);

        void handleTermination();

        void doCfgQuery(std::span<const std::byte>, std::shared_ptr<Client> &);

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

        /// connected clients
        std::unordered_map<struct bufferevent *, std::shared_ptr<Client>> clients;
};

#endif
