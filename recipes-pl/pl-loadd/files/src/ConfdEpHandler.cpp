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

#include "ConfdEpHandler.h"
#include "Coprocessor.h"
#include "RpcServer.h"
#include "RpcTypes.h"

/**
 * @brief Initialize the confd endpoint handler
 *
 * Open a client connection to the confd daemon, then prepare buffer events for reading from both
 * channels which will forward the data to the other end.
 */
ConfdEpHandler::ConfdEpHandler(const int fd, const std::shared_ptr<RpcServer> &lrpc) :
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
        throw std::system_error(errno, std::generic_category(), "write confd wake-up packet");
    }

    // establish confd connection and create a read event
    this->confdSocket = this->connectToConfd();
    PLOG_VERBOSE << "confd client socket: " << this->confdSocket;

    this->confdBev = bufferevent_socket_new(evbase, this->confdSocket, 0);
    if(!this->confdBev) {
        throw std::runtime_error("failed to create bufferevent (confd)");
    }

    bufferevent_setwatermark(this->confdBev, EV_READ, sizeof(struct rpc_header),
            EV_RATE_LIMIT_MAX);

    bufferevent_setcb(this->confdBev, [](auto bev, auto ctx) {
        try {
            reinterpret_cast<ConfdEpHandler *>(ctx)->handleConfdRead(bev);
        } catch(const std::exception &e) {
            PLOG_ERROR << "Failed to handle confd read: " << e.what();
            // TODO: abort program
        }
    }, nullptr, [](auto bev, auto what, auto ctx) {
        try {
            reinterpret_cast<ConfdEpHandler *>(ctx)->handleConfdEvent(bev, what);
        } catch(const std::exception &e) {
            PLOG_ERROR << "Failed to handle confd event: " << e.what();
            // TODO: abort program
        }
    }, this);

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
            reinterpret_cast<ConfdEpHandler *>(ctx)->handleRpmsgRead(bev);
        } catch(const std::exception &e) {
            PLOG_ERROR << "Failed to handle rpmsg read: " << e.what();
            // TODO: abort program
        }
    }, nullptr, [](auto bev, auto what, auto ctx) {
        PLOG_ERROR << "rpmsg event (unhandled): " << what;
    }, this);

    // add events to rpc server's run loop
    err = bufferevent_enable(this->confdBev, EV_READ);
    if(err == -1) {
        throw std::runtime_error("failed to enable bufferevent (confd)");
    }
    err = bufferevent_enable(this->rpmsgBev, EV_READ);
    if(err == -1) {
        throw std::runtime_error("failed to enable bufferevent (rpmsg)");
    }
}

/**
 * @brief Establish connection to confd
 *
 * @return File descriptor for a domain socket connected to confd daemon
 */
int ConfdEpHandler::connectToConfd() {
    int fd, err;

    // TODO: read the path from config
    constexpr static const std::string_view kSocketPath{"/var/run/confd/rpc.sock"};

    // create the socket
    fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd == -1) {
        throw std::system_error(errno, std::generic_category(), "create confd socket");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, kSocketPath.data(), sizeof(addr.sun_path) - 1);

    // dial it
    err = connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if(err == -1) {
        close(fd);
        throw std::system_error(errno, std::generic_category(), "dial confd socket");
    }

    // mark the socket to use non-blocking IO (for libevent)
    err = evutil_make_socket_nonblocking(fd);
    if(err == -1) {
        close(fd);
        throw std::system_error(errno, std::generic_category(),
                "evutil_make_socket_nonblocking (confd)");
    }

    // it's been connected :D
    return fd;
}

/**
 * @brief Clean up the handler
 *
 * Close the confd client connection and remove all associated libevent resources.
 */
ConfdEpHandler::~ConfdEpHandler() {
    // remove events (if RPC server is still alive)
    if(auto ptr = lrpc.lock()) {
        PLOG_DEBUG << "removing events";

        (void) ptr;
    }

    // release memory
    if(this->confdBev) {
        bufferevent_free(this->confdBev);
    }
    if(this->rpmsgBev) {
        bufferevent_free(this->rpmsgBev);
    }

    // close sockets
    if(this->confdSocket != -1) {
        close(this->confdSocket);
    }
}



/**
 * @brief Handles data available to read on the confd client connection
 *
 * Read all of the data in the confd client buffer and transmit it to the rpmsg connection.
 */
void ConfdEpHandler::handleConfdRead(struct bufferevent *bev) {
    int err;

    // pull it out and into our read buffer
    auto buf = bufferevent_get_input(bev);
    PLOG_VERBOSE << "rx from confd: " << evbuffer_get_length(buf);

    const size_t pending = evbuffer_get_length(buf);

    this->confdRxBuf.resize(pending);
    int read = evbuffer_remove(buf, static_cast<void *>(this->confdRxBuf.data()), pending);

    if(read == -1) {
        throw std::runtime_error("failed to drain confd read buffer");
    }


    // send it (TODO: why can't we write using bufferevent?)
    err = write(this->remoteEp, this->confdRxBuf.data(), this->confdRxBuf.size());
    if(err < 0) {
        throw std::runtime_error("failed to write confd->m4 message");
    }
}

/**
 * @brief Handle an event occurring on the confd socket
 */
void ConfdEpHandler::handleConfdEvent(struct bufferevent *bev, const uintptr_t flags) {
    // connection closed
    if(flags & BEV_EVENT_EOF) {
        PLOG_WARNING << "confd closed connection :(";
    }
    // IO error
    else if(flags & BEV_EVENT_ERROR) {
        PLOG_WARNING << "confd io error: flags=" << flags;
    }

}

/**
 * @brief Handles data available to read on the rpmsg endpoint
 *
 * All data available to read is pushed to the confd socket.
 */
void ConfdEpHandler::handleRpmsgRead(struct bufferevent *bev) {
    int err{0};

    auto buf = bufferevent_get_input(bev);
    PLOG_VERBOSE << "rx from rpmsg: " << evbuffer_get_length(buf);

    const size_t pending = evbuffer_get_length(buf);

    this->confdRxBuf.resize(pending);
    int read = evbuffer_remove(buf, static_cast<void *>(this->confdRxBuf.data()), pending);

    if(read == -1) {
        throw std::runtime_error("failed to drain confd read buffer");
    }

    // send it
    err = bufferevent_write(this->confdBev, this->confdRxBuf.data(), pending);

    if(err != 0) {
        throw std::runtime_error("failed to write confd->m4 message");
    }
}
