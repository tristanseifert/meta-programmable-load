#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cerrno>
#include <cstring>
#include <system_error>

#include <plog/Log.h>

#include "Config.h"
#include "RpcServer.h"
#include "watchdog.h"

/**
 * @brief Initialize the RPC server
 *
 * Create the listening socket and bind to it, then permit accepting connections.
 */
RpcServer::RpcServer() {
    int err;

    const auto path = Config::GetRpcSocketPath().c_str();

    // create the socket
    err = socket(AF_UNIX, SOCK_STREAM, 0);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "create rpc socket");
    }

    this->listenSock = err;

    // delete previous file, if any, then bind to that path
    PLOG_DEBUG << "RPC socket path: '" << path << "'";

    err = unlink(path);
    if(err == -1 && errno != ENOENT) {
        throw std::system_error(errno, std::generic_category(), "unlink rpc socket");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    err = bind(this->listenSock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "bind rpc socket");
    }

    // make listening socket non-blocking (to allow accept calls)
    err = fcntl(this->listenSock, F_GETFL);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "get rpc socket flags");
    }

    err = fcntl(this->listenSock, F_SETFL, err | O_NONBLOCK);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "set rpc socket flags");
    }

    // allow clients to connect
    err = listen(this->listenSock, kListenBacklog);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "listen rpc socket");
    }
}

/**
 * @brief Shut down the RPC server
 *
 * Terminate any remaining client connections, as well as close the main listening socket. We'll
 * also delete the socket file at this time.
 */
RpcServer::~RpcServer() {
    int err;

    // close and unlink listening socket
    PLOG_DEBUG << "Closing RPC server socket";

    close(this->listenSock);

    err = unlink(Config::GetRpcSocketPath().c_str());
    if(err == -1) {
        PLOG_ERROR << "failed to unlink socket: " << strerror(errno);
    }

    // close all clients
    PLOG_DEBUG << "Closing client connections";

}

/**
 * @brief Wait for a server event
 *
 * Block on the listening socket, and all active client sockets, to wait for data to be received or
 * some other type of event.
 *
 * @remark This waits up to a maximum number of microseconds so that we may kick the watchdog, if
 *         it is enabled. The interval is set to half of the watchdog notification interval.
 */
void RpcServer::run() {
    // get maximum block time

    // TODO: implement
}
