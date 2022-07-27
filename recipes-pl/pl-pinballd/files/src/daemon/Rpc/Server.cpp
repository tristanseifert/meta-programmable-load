#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cbor.h>

#include <cerrno>
#include <cstring>
#include <system_error>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <fmt/format.h>
#include <plog/Log.h>

#include "EventLoop.h"
#include "Probulator.h"
#include "RpcTypes.h"
#include "Client.h"
#include "Server.h"

using namespace Rpc;

/**
 * @brief Initialize the listening socket
 *
 * Create and bind the domain socket used for RPC requests.
 *
 * @param path Filesystem path to create the listening socket at
 */
void Server::initSocket(const std::filesystem::path &path) {
    int err;

    // create the socket
    err = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "create rpc socket");
    }

    this->listenSock = err;

    // delete previous file, if any, then bind to that path
    PLOG_INFO << "RPC socket path: '" << path.native() << "'";

    err = unlink(path.native().c_str());
    if(err == -1 && errno != ENOENT) {
        throw std::system_error(errno, std::generic_category(), "unlink rpc socket");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.native().c_str(), sizeof(addr.sun_path) - 1);

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

    this->socketPath = path;
}

/**
 * @brief Initialize the event for the listening socket
 *
 * This event fires any time a client connects. It serves as the primary event.
 */
void Server::initSocketEvent(EventLoop *ev) {
    this->listenEvent = event_new(ev->getEvBase(), this->listenSock,
            (EV_READ | EV_PERSIST), [](auto fd, auto what, auto ctx) {
        try {
            reinterpret_cast<Server *>(ctx)->acceptClient();
        } catch(const std::exception &e) {
            PLOG_ERROR << "failed to accept client: " << e.what();
        }
    }, this);
    if(!this->listenEvent) {
        throw std::runtime_error("failed to allocate listen event");
    }

    event_add(this->listenEvent, nullptr);
}

/**
 * @brief Shut down the RPC server
 *
 * Terminate any remaining client connections, as well as close the main listening socket. We'll
 * also delete the socket file at this time.
 */
Server::~Server() {
    int err;

    // close and unlink listening socket
    PLOG_DEBUG << "Closing RPC server socket";

    event_free(this->listenEvent);
    close(this->listenSock);

    err = unlink(this->socketPath.native().c_str());
    if(err == -1) {
        PLOG_ERROR << "failed to unlink socket: " << strerror(errno);
    }

    // close all clients
    PLOG_DEBUG << "Closing client connections";
    this->clients.clear();
}



/**
 * @brief Set probulator instance to affect
 */
void Server::setProbulator(const std::shared_ptr<Probulator> &probulator) {
    this->ledManager = probulator->getLedManager();
}



/**
 * @brief Accept a single waiting client
 *
 * Accepts one client on the listening socket, and sets up a connection struct for it.
 */
void Server::acceptClient() {
    // accept client
    int fd = accept(this->listenSock, nullptr, nullptr);
    if(fd == -1) {
        throw std::system_error(errno, std::generic_category(), "accept");
    }

    // convert socket to non-blocking
    int err = evutil_make_socket_nonblocking(fd);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "evutil_make_socket_nonblocking");
    }

    // set up our bookkeeping for it and add it to event loop
    auto cl = std::make_shared<Client>(this->shared_from_this(), fd);
    this->clients.emplace(cl->getId(), std::move(cl));

    PLOG_DEBUG << "Accepted client " << fd << " (" << this->clients.size() << " total)";
}

/**
 * @brief Terminate a client connection
 *
 * Releases a particular client connection, closing it if not already closed by the remote end.
 * All of its associated resources will be released as well.
 */
void Server::releaseClient(const int clientId) {
    const auto removed = this->clients.erase(clientId);
    if(!removed) {
        throw std::invalid_argument(fmt::format("cannot remove nonexistent client {}", clientId));
    }
}



#if 0
    int err;

    for(const auto &[bev, client] : this->clients) {
        // skip if client doesn't want this broadcast
        if(!client->wantsBroadcastOfType(type)) {
            continue;
        }

        err = bufferevent_write(bev, packet.data(), packet.size());

        if(err != 0) {
            PLOG_WARNING << "failed to broadcast packet to client " << client->socket << ": "
                << err;
        }
    }
#endif

/**
 * @brief Broadcast a packet given a raw payload
 *
 * Format the packet by prepending a `struct rpc_header` to it, filling it out appropriately and
 * then copying the payload in before sending it to be broadcast.
 */
void Server::broadcastRaw(const BroadcastType type, const uint8_t endpoint,
        std::span<const std::byte> payload) {
    // set up buffer and copy the payload in
    std::vector<std::byte> buffer;
    buffer.resize(sizeof(struct rpc_header) + payload.size());
    std::fill(buffer.begin(), buffer.begin() + sizeof(struct rpc_header), std::byte{0});

    std::copy(payload.begin(), payload.end(), buffer.begin() + sizeof(struct rpc_header));

    // initialize the header
    auto hdr = reinterpret_cast<struct rpc_header *>(buffer.data());
    hdr->version = kRpcVersionLatest;
    hdr->endpoint = endpoint;
    hdr->length = buffer.size();

    // send it
    this->broadcastPacket(type, buffer);
}
