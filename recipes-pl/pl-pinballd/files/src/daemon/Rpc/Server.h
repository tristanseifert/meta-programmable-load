#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <unordered_map>

#include "Client.h"
#include "Types.h"

class EventLoop;

namespace Rpc {
/**
 * @brief Local RPC server
 *
 * Provides an RPC interface over a local domain socket. This interface can be used to communicate
 * with the front panel hardware, to update the state of indicators, and read the state of inputs
 * such as buttons, touch, and encoders.
 */
class Server: public std::enable_shared_from_this<Server> {
    friend class Client;

    public:
        /**
         * @brief Initialize the RPC server
         *
         * Open the local RPC listening socket and the associated event loop.
         */
        Server(EventLoop *ev, const std::filesystem::path &socketPath) {
            this->initSocket(socketPath);
            this->initSocketEvent(ev);
        }

        ~Server();

        /**
         * @brief Broadcast a packet to all connected clients
         *
         * Send the specified packet (which should have an rpc_header prepended, followed
         * immediately by the packet payload) to all connected clients.
         *
         * @param type Type of broadcast packet
         * @param packet Packet data to broadcast
         */
        inline void broadcastPacket(const BroadcastType type, std::span<const std::byte> packet) {
            for(const auto &[id, client] : this->clients) {
                client->maybeBroadcast(type, packet);
            }
        }
        void broadcastRaw(const BroadcastType type, const uint8_t endpoint,
                std::span<const std::byte> payload);

    private:


    private:
        void initSocket(const std::filesystem::path &);
        void initSocketEvent(EventLoop *);

        void acceptClient();
        void releaseClient(const int clientId);

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
        std::unordered_map<int, std::shared_ptr<Client>> clients;
};
}

#endif
