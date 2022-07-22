#ifndef RPC_PINBALLCLIENT_H
#define RPC_PINBALLCLIENT_H

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

class EventLoop;

namespace Rpc {
/**
 * @brief Interface to pinballd (user interface hardware io deamon)
 */
class PinballClient {
    public:
        PinballClient(const std::shared_ptr<EventLoop> &ev,
                const std::filesystem::path &socketPath);
        ~PinballClient();

    private:
        int connectSocket();

        void bevRead(struct bufferevent *);
        void bevEvent(struct bufferevent *, const uintptr_t);

        void processUiEvent(std::span<const std::byte>);

    private:
        /// The event loop that owns us
        std::weak_ptr<EventLoop> ev;

        /// Path of the RPC socket
        std::filesystem::path socketPath;
        /// File descriptor for the RPC socket
        int fd{-1};
        /// Buffer event wrapping the loadd socket
        struct bufferevent *bev{nullptr};

        /// Packet receive buffer
        std::vector<std::byte> rxBuf;
};
}

#endif
