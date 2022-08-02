#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "Types.h"

struct cbor_item_t;

namespace Rpc {
class Server;

/**
 * @brief Handler for a connected, remote RPC client
 *
 * This struct encapsulates all information about a connected client, including the events
 * used to wait for activity on the connection.
 */
class Client {
    public:
        Client(const std::shared_ptr<Server> &, const int);
        ~Client();

        void replyTo(const struct rpc_header &, std::span<const std::byte>);
        void send(std::span<const std::byte>);

        /**
         * @brief Determine if the client wants to receive broadcasts of the given type
         *
         * @param type Broadcast packet type
         */
        constexpr inline bool wantsBroadcastOfType(const BroadcastType type) {
            switch(type) {
                case BroadcastType::TouchEvent:
                    return this->wantsTouchEvents;
                case BroadcastType::ButtonEvent:
                    return this->wantsButtonEvents;
                case BroadcastType::EncoderEvent:
                    return this->wantsEncoderEvents;
            }
        }

        /**
         * @brief Broadcast a packet, if the client desires this type
         *
         * @param type Packet type
         * @param packet Full packet (including RPC header) to send
         */
        inline void maybeBroadcast(const BroadcastType type, std::span<const std::byte> packet) {
            if(!this->wantsBroadcastOfType(type)) {
                return;
            }

            this->send(packet);
        }

        /**
         * @brief Return the client's connection id
         *
         * @remark This is just the underlying file descriptor
         */
        constexpr auto inline getId() const {
            return this->socket;
        }

    private:
        void bevRead(struct bufferevent *);
        void bevEvent(struct bufferevent *, const size_t);

        void dispatchPacket(const struct rpc_header &, const struct cbor_item_t *);
        void updateBroadcastConfig(const struct cbor_item_t *);
        void updateIndicators(const struct cbor_item_t *);

    private:
        /// Log received packets
        constexpr static const bool kLogReceived{false};

        /// Underlying client file descriptor
        int socket{-1};
        /// Socket buffer event (used for data ready to read + events)
        struct bufferevent *event{nullptr};
        /// message receive buffer
        std::vector<std::byte> receiveBuf;
        /// message transmit buffer
        std::vector<std::byte> transmitBuf;

        /// Owning server instance
        std::weak_ptr<Server> server;

        /// Client wants to receive touch events
        uintptr_t wantsTouchEvents              :1{false};
        /// Client wants to receive button events
        uintptr_t wantsButtonEvents             :1{false};
        /// Client wants to receive encoder events
        uintptr_t wantsEncoderEvents            :1{false};
};
}

#endif
