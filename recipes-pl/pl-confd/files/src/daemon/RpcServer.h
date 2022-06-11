#ifndef RPCSERVER_H
#define RPCSERVER_H

#include <cstddef>

/**
 * @brief Remote access interface
 *
 * This class opens the listening socket for the RPC interface and handles requests there. It
 * hides the management and interfacing with clients behind a single run method, which takes
 * advantage of system facilities to wait on multiple file descriptors at once.
 */
class RpcServer {
    public:
        RpcServer();
        ~RpcServer();

        void run();

    private:
        /// Maximum amount of clients that may be waiting to be accepted at once
        constexpr static const size_t kListenBacklog{5};

        /// Main RPC listening socket
        int listenSock{-1};
};

#endif
