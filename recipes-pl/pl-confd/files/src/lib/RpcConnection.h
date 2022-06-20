#ifndef CONNECTION_H
#define CONNECTION_H

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

/**
 * @brief RPC connection to confd
 *
 * This class encapsulates an RPC socket connection to confd. It holds all of the buffers we re-use
 * during the life of the connection as well.
 */
class RpcConnection {
    constexpr static const std::string_view kDefaultSocketPath{"/var/run/confd/rpc.sock"};

    public:
        uint8_t sendPacket(const uint8_t ep, std::span<const std::byte> payload);
        void sendPacketWithReply(const uint8_t ep, std::span<const std::byte> payload,
                std::span<const std::byte> &outReplyPayload);

        /// Attempt to allocate the shared RPC connection
        static void Init(const std::string_view &path = kDefaultSocketPath) {
            if(gShared) {
                throw std::logic_error("rpc connection already initialized!");
            }
            gShared = new RpcConnection(path);
        }

        /// Release the shared RPC connection
        static void Deinit() {
            delete gShared;
            gShared = nullptr;
        }

        /// Return the global RPC connection instance
        static auto The() {
            return gShared;
        }

        /**
         * @brief Recursion lock
         *
         * All API wrapper functions should take and hold this lock for the duration of their
         * execution to ensure nothing conflicts/breaks.
         */
        std::mutex lock;

    private:
        RpcConnection(const std::string_view &socketPath);
        ~RpcConnection();

        void receivePacket(std::span<const std::byte> &);
        void sendPacket(std::span<const std::byte>);

    private:
        static RpcConnection *gShared;

        /// File descriptor for the socket
        int socket{-1};

        /// Transmit buffer
        std::vector<std::byte> transmitBuf;
        /// Receive message buffer
        std::vector<std::byte> receiveBuf;

        /// Next tag value
        uint8_t nextTag{0};
};

#endif
