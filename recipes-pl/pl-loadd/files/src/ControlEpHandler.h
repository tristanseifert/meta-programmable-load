#ifndef CONTROLEPHANDLER_H
#define CONTROLEPHANDLER_H

#include <cstddef>
#include <memory>
#include <vector>

#include "Coprocessor.h"

class RpcServer;

/**
 * @brief Control endpoint handler
 *
 * It exposes the control endpoint to local tasks via our local RPC interface.
 */
class ControlEpHandler: public Coprocessor::EndpointHandler {
    public:
        ControlEpHandler(const int fd, const std::shared_ptr<RpcServer> &lrpc);
        ~ControlEpHandler() override;

    private:
        void handleRpmsgRead(struct bufferevent *bev);

    private:
        /// Should rpmsg received packets be dumped to log?
        constexpr static const bool kDumpRpmsgPackets{true};

        /// event wrapping the local rpmsg channel
        struct bufferevent *rpmsgBev{nullptr};
        /// Read buffer for messages from rpmsg
        std::vector<std::byte> rpmsgRxBuf;

        /// Local RPC server
        std::weak_ptr<RpcServer> lrpc;
};

#endif
