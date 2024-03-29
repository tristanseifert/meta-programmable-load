#ifndef CONFDEPHANDLER_H
#define CONFDEPHANDLER_H

#include <atomic>
#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "Coprocessor.h"

class RpcServer;

/**
 * @brief Config endpoint handler
 *
 * This handler proxies requests made via the confd channel to confd, via its local RPC socket.
 */
class ConfdEpHandler: public Coprocessor::EndpointHandler {
    public:
        ConfdEpHandler(const int fd, const std::shared_ptr<RpcServer> &lrpc);
        ~ConfdEpHandler() override;

    private:
        int connectToConfd();

        void handleConfdRead(struct bufferevent *bev);
        void handleConfdEvent(struct bufferevent *bev, const uintptr_t what);

        void handleRpmsgRead(struct bufferevent *bev);

    private:
        /// Should confd received packets be dumped to log?
        constexpr static const bool kDumpConfdPackets{false};
        /// Should rpmsg received packets be dumped to log?
        constexpr static const bool kDumpRpmsgPackets{false};

        /// when set, the socket needs to be re-created for the next request
        std::atomic_bool needsNewSocket{false};
        /// File descriptor of our client connection to confd
        int confdSocket{-1};
        /// event wrapping the confd socket
        struct bufferevent *confdBev{nullptr};

        /// event wrapping the local rpmsg channel
        struct bufferevent *rpmsgBev{nullptr};

        /// Local RPC server
        std::weak_ptr<RpcServer> lrpc;

        /// Read buffer for messages from confd
        std::vector<std::byte> confdRxBuf;
        /// Read buffer for messages from rpmsg
        std::vector<std::byte> rpmsgRxBuf;
};

#endif
