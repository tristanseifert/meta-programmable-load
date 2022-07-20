#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>

#include <cerrno>
#include <system_error>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <fmt/format.h>
#include <plog/Log.h>

#include "ControlEpHandler.h"
#include "Coprocessor.h"
#include "RpcServer.h"
#include "RpcTypes.h"

/**
 * @brief Initialize the control endpoint handler
 *
 * Prepare a buffer event for receiving messages from the remote control endpoint, then send a nop
 * message to prepare the endpoint.
 */
ControlEpHandler::ControlEpHandler(const int fd, const std::shared_ptr<RpcServer> &lrpc) :
    Coprocessor::EndpointHandler(fd), lrpc(lrpc) {
    int err;
    auto evbase = lrpc->getEvBase();

    // send an empty message to notify rpmsg channel we're alive
    struct rpc_header hdr{
        .version = kRpcVersionLatest,
        .length = sizeof(rpc_header)
    };
    err = write(fd, &hdr, sizeof(hdr));
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), "write control wake-up packet");
    }

    // create event for the rpmsg channel
    err = evutil_make_socket_nonblocking(fd);
    if(err == -1) {
        close(fd);
        throw std::system_error(errno, std::generic_category(), "evutil_make_socket_nonblocking (rpmsg)");
    }

    this->rpmsgBev = bufferevent_socket_new(evbase, fd, 0);
    if(!this->rpmsgBev) {
        throw std::runtime_error("failed to create bufferevent (rpmsg)");
    }

    bufferevent_setwatermark(this->rpmsgBev, EV_READ, sizeof(struct rpc_header),
            EV_RATE_LIMIT_MAX);

    bufferevent_setcb(this->rpmsgBev, [](auto bev, auto ctx) {
        try {
            reinterpret_cast<ControlEpHandler *>(ctx)->handleRpmsgRead(bev);
        } catch(const std::exception &e) {
            PLOG_ERROR << "Failed to handle rpmsg read: " << e.what();
            // TODO: abort program
        }
    }, nullptr, [](auto bev, auto what, auto ctx) {
        PLOG_ERROR << "rpmsg event (unhandled): " << what;
    }, this);

    // add events to rpc server's run loop
    err = bufferevent_enable(this->rpmsgBev, EV_READ);
    if(err == -1) {
        throw std::runtime_error("failed to enable bufferevent (rpmsg)");
    }
}

/**
 * @brief Clean up control endpoint resources
 */
ControlEpHandler::~ControlEpHandler() {
    if(this->rpmsgBev) {
        bufferevent_free(this->rpmsgBev);
    }
}

/**
 * @brief Handles data available to read on the rpmsg endpoint
 *
 * All data available to read is sent either to a remote client connected to our local RPC
 * interface or broadcasted to all connected clients.
 */
void ControlEpHandler::handleRpmsgRead(struct bufferevent *bev) {
    auto buf = bufferevent_get_input(bev);
    const size_t pending = evbuffer_get_length(buf);

    this->rpmsgRxBuf.resize(pending);
    int read = evbuffer_remove(buf, static_cast<void *>(this->rpmsgRxBuf.data()), pending);

    if(read == -1) {
        throw std::runtime_error("failed to drain confd read buffer");
    }

    if(kDumpRpmsgPackets) {
        DumpPacket(">>> rpmsg", this->rpmsgRxBuf);
    }
}
