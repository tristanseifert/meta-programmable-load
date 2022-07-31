#ifndef PLCOMMON_RPC_CLIENTBASE_H
#define PLCOMMON_RPC_CLIENTBASE_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

#include <load-common/EventLoop.h>

struct cbor_item_t;

namespace PlCommon::Rpc {
struct RpcHeader;

/**
 * @brief Abstract base class for an RPC client
 *
 * This class encapsulates an RPC client, which uses asynchronous IO (backed by libevent) to
 * communicate with a remote RPC client.
 *
 * Client implementations simply need to implement a single method to handle received packets,
 * which receives a packet header and decoded CBOR payload.
 */
class ClientBase {
    public:
        ClientBase(const std::filesystem::path &socket) : ClientBase(socket, EventLoop::Current()) {}
        ClientBase(const std::filesystem::path &socket, const std::shared_ptr<EventLoop> &ev);
        virtual ~ClientBase();

    protected:
        void sendRaw(std::span<const std::byte> payload);
        uint8_t sendPacket(const uint8_t endpoint, std::span<const std::byte> payload);

        virtual void handleIncomingMessageRaw(const struct RpcHeader &header,
                std::span<const std::byte> payload);
        /**
         * @brief Process a received message
         *
         * This is a high level message callback, to be implemented by the concrete RPC client
         * subclass. It receives the decoded CBOR message payload, if any, in addition to the raw
         * packet header.
         *
         * You should only need to implement this method to handle messages, unless your protocol
         * does not use CBOR payloads.
         *
         * @seeAlso handleIncomingMessageRaw
         */
        virtual void handleIncomingMessage(const struct RpcHeader &header,
                const struct cbor_item_t *message) = 0;

        virtual void handleConnectionClosed();
        virtual void handleIoError(const uintptr_t flags);

    private:
        int connectSocket();

        void bevRead(struct bufferevent *);
        void bevEvent(struct bufferevent *, const uintptr_t);

    private:
        /// filesystem path for the RPC socket
        std::filesystem::path socketPath;
        /// File descriptor for RPC socket
        int fd{-1};
        /// Buffer event wrapping the socket
        struct bufferevent *bev{nullptr};

        /// Value for the next outgoing packet tag
        uint8_t nextTag{0};

        /// Packet receive buffer
        std::vector<std::byte> rxBuf;
};
}

#endif
