#ifndef RPCSERVER_H
#define RPCSERVER_H

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <unordered_map>

/**
 * @brief Local RPC server
 *
 * Provides an RPC interface over a local domain socket. This interface can be used to communicate
 * with the front panel hardware, to update the state of indicators, and read the state of inputs
 * such as buttons, touch, and encoders.
 */
class RpcServer {
    public:
        /**
         * @brief Initialize the RPC server
         *
         * Open the local RPC listening socket and the associated event loop.
         */
        RpcServer(const std::filesystem::path &socketPath) {
            this->initSocket(socketPath);
            this->initSocketEvent();
        }

        ~RpcServer();

        void broadcastPacket(std::span<const std::byte> packet);

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
        void initSocket(const std::filesystem::path &);

        void initSocketEvent();

        void acceptClient();
        void handleClientRead(struct bufferevent *);
        void handleClientEvent(struct bufferevent *, const size_t);
        void abortClient(struct bufferevent *);

    private:
        /// Maximum amount of clients that may be waiting to be accepted at once
        constexpr static const size_t kListenBacklog{5};

        /// Path to the listening socket on disk
        std::filesystem::path socketPath;
        /// Main RPC listening socket
        int listenSock{-1};
        /// event for listening socket receiving a client
        struct event *listenEvent{nullptr};

        /// connected clients
        std::unordered_map<struct bufferevent *, std::shared_ptr<Client>> clients;

};

#endif
