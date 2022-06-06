#include "RpcListener.h"
#include "Drawer.h"
#include "config.h"

#include "remotelib/rpc.h"

#include <atomic>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <system_error>

#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

extern std::atomic_bool gRun;

/**
 * @brief Initialize the RPC listener by opening the socket
 *
 * Create the UNIX domain socket for the splash screen daemon.
 */
RpcListener::RpcListener() {
    int err;

    // create the socket and bind to path
    this->sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if(this->sock == -1) {
        throw std::system_error(errno, std::generic_category(), "create rpc socket");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    *addr.sun_path = '\0';
    strncpy(addr.sun_path + 1, Config::kControlSocketPath.data(), sizeof(addr.sun_path) - 2);
    err = bind(this->sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));

    if(err) {
        throw std::system_error(errno, std::generic_category(), "bind rpc socket");
    }

    // listening socket must be non-blocking
    err = fcntl(this->sock, F_GETFL);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "get rpc socket flags");
    }

    err = fcntl(this->sock, F_SETFL, err | O_NONBLOCK);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "set rpc socket flags");
    }

    // start for listening
    err = listen(this->sock, kListenBacklog);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "listen rpc socket");
    }
}

/**
 * @brief Close down the RPC listener's resources.
 */
RpcListener::~RpcListener() {
    // close listening socket
    close(this->sock);
}

/**
 * @brief Handle RPC listener events
 *
 * This call multiplexes events between the main RPC socket (for incoming connections) and all
 * active client connections. This is implemented by watching the listening socket + all active
 * client sockets for activity.
 *
 * @remark This could _probably_ use epoll() or friends but that's a lot of effort.
 *
 * @param d Drawer instance responsible for drawing the splash screen (will receive value updates)
 *
 * @return Whether we shall continue to handle events; only `false` if we receive a quit request
 */
void RpcListener::handleEvents(Drawer &d) {
    std::unordered_set<int> deadClients;
    int err;

    // set up the event set
    int highest{this->sock};
    fd_set rfds, efds;
    FD_ZERO(&rfds);
    FD_ZERO(&efds);

    FD_SET(this->sock, &rfds);
    FD_SET(this->sock, &efds);

    for(const auto clientFd : this->clientSockets) {
        FD_SET(clientFd, &rfds);
        FD_SET(clientFd, &efds);

        if(clientFd > highest) {
            highest = clientFd;
        }
    }

    // wait (up to 5 seconds)
    err = select((highest+1), &rfds, nullptr, &efds, nullptr);
    if(err == -1) {
        // a signal interrupted the call, likely we'll go exit now
        if(errno == EINTR) {
            return;
        }
        // everything else is an actual error
        throw std::system_error(errno, std::generic_category(), "rpc select");
    }

    // handle event on main connection (new client)
    if(FD_ISSET(this->sock, &rfds) || FD_ISSET(this->sock, &efds)) {
        int fd = accept(this->sock, nullptr, nullptr);
        if(fd == -1) {
            throw std::system_error(errno, std::generic_category(), "rpc accept");
        }

        // store it for later, on the next iteration we'll listen to it
        this->clientSockets.insert(fd);
    }

    // handle client connections
    for(const auto clientFd : this->clientSockets) {
        // bail if no events on this fd
        if(!FD_ISSET(clientFd, &rfds) && !FD_ISSET(clientFd, &efds)) {
            continue;
        }

        // for exceptional, close the socket
        if(FD_ISSET(clientFd, &efds)) {
            close(clientFd);
            deadClients.insert(clientFd);
            continue;
        }

        // we must have received a message, try to handle it
        try {
            if(!this->handleClientMessage(d, clientFd)) {
                // client closed connection
                deadClients.insert(clientFd);
            }
        }
        // if message processing fails, close the connection
        catch(const std::exception &e) {
            std::cerr << "Error on client fd " << clientFd << ": " << e.what() << std::endl;

            close(clientFd);
            deadClients.insert(clientFd);
        }
    }

    // remove any dead clients
    for(const auto deadClient : deadClients) {
        this->clientSockets.erase(deadClient);
    }
}

/**
 * @brief Read a message from a client
 *
 * Reads an RPC message from the client and acts upon it.
 *
 * @param d Drawer instance to receive status updates
 * @param fd Client file descriptor
 *
 * @return Whether the client connection is still valid
 */
bool RpcListener::handleClientMessage(Drawer &d, const int fd) {
    int err;

    // read a message
    splash_rpc_message_t msg;
    err = read(fd, &msg, sizeof(msg));

    if(!err) {
        return false;
    } else if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "client read");
    } else if(static_cast<size_t>(err) < sizeof(splash_rpc_message_t)) {
        throw std::runtime_error("insufficient client read size");
    }

    // handle the message
    switch(msg.type) {
        // no-op
        case kSplashRpcTypeNone:
            break;
        // update progress
        case kSplashRpcTypeSetProgress:
            d.setProgress(msg.progress);
            break;
        // update message
        case kSplashRpcTypeSetMessage:
            d.setProgressString(msg.message);
            break;
        // exit
        case kSplashRpcTypeTerminate:
            gRun = false;
            break;
        // unhandled
        default:
            std::cerr << "unhandled rpc message type: " << msg.type << std::endl;
            throw std::runtime_error("unhandled rpc message type");
    }

    return true;
}
