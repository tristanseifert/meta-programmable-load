#ifndef RPCSERVER_H
#define RPCSERVER_H

#include <sys/signal.h>

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

/**
 * @brief Local RPC server
 *
 * Provides an RPC interface over a local domain socket. This interface can be used to communicate
 * with the load directly, and also allows other clients to acquire file descriptors for other
 * rpmsg channels.
 */
class RpcServer {
    public:
        /**
         * @brief Initialize the RPC server
         *
         * Open the local RPC listening socket and the associated event loop.
         */
        RpcServer() {
            this->initSocket();
            this->initEventLoop();
        }

        ~RpcServer();

        void run();

        /**
         * @brief Get libevent main loop
         *
         * Return a pointer to the libevent based main loop run by the RPC server; this can be used
         * to add additional event sources to the loop, for example for remote message handlers.
         */
        inline auto getEvBase() {
            return this->evbase;
        }

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
            /// message transmit buffer
            std::vector<std::byte> transmitBuf;

            Client(RpcServer *, const int);
            ~Client();

            void replyTo(const struct rpc_header &, std::span<const std::byte>);
            void send(std::span<const std::byte>);
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
