#ifndef SPLASH_RPCLISTENER_H
#define SPLASH_RPCLISTENER_H

#include <cstddef>
#include <unordered_set>

class Drawer;

class RpcListener {
    public:
        RpcListener();
        ~RpcListener();

        void handleEvents(Drawer &);

    private:
        bool handleClientMessage(Drawer &, const int);

    private:
        /// Maximum number of clients to keep waiting in the listen backlog
        constexpr static const size_t kListenBacklog{5};

        /// Listening socket
        int sock{-1};
        /// Active client sockets
        std::unordered_set<int> clientSockets;
};

#endif
