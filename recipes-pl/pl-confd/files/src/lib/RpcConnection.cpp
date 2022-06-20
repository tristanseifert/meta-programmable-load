#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstring>
#include <system_error>

#include "rpc/types.h"
#include "RpcConnection.h"

RpcConnection *RpcConnection::gShared{nullptr};

/**
 * @brief Establish RPC connection
 *
 * Create the RPC socket and dial the path specified.
 */
RpcConnection::RpcConnection(const std::string_view &socketPath) {
    int err;

    // create the socket
    this->socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if(this->socket == -1) {
        throw std::system_error(errno, std::generic_category(), "create rpc socket");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, socketPath.data(), sizeof(addr.sun_path) - 1);

    // dial it
    err = connect(this->socket, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "dial rpc socket");
    }
}

/**
 * @brief Close RPC connection
 *
 * We'll close the socket and release all allocated memory.
 */
RpcConnection::~RpcConnection() {
    if(this->socket != -1) {
        close(this->socket);
    }
}



/**
 * @brief Create and send packet
 *
 * Given the endpoint and optional payload, format a packet and then send it to the RPC connection.
 *
 * @return Tag of the sent packet
 */
uint8_t RpcConnection::sendPacket(const uint8_t ep, std::span<const std::byte> payload) {
    // allocate buffer
    const size_t msgSize = sizeof(struct rpc_header) + payload.size();
    this->transmitBuf.resize(msgSize, std::byte(0));
    std::fill(this->transmitBuf.begin(), this->transmitBuf.begin() + sizeof(struct rpc_header),
            std::byte(0));

    // copy payload
    if(!payload.empty()) {
        std::copy(payload.begin(), payload.end(),
                (this->transmitBuf.begin() + offsetof(struct rpc_header, payload)));
    }

    // fill in the header
    uint8_t tag{++this->nextTag};
    auto hdr = reinterpret_cast<struct rpc_header *>(this->transmitBuf.data());

    hdr->version = kRpcVersionLatest;
    hdr->length = msgSize;
    hdr->endpoint = ep;
    hdr->tag = tag;

    // send the packet
    this->sendPacket(this->transmitBuf);

    return tag;
}

/**
 * @brief Send a packet, then await its reply
 *
 * Transmit a packet (with the given payload) to the specified endpoint; then wait for the response
 * to whatever request we just sent, based on the combination of ep+tag.
 *
 * @param ep Endpoint to send the packet to
 * @param payload Payload to include in the packet
 * @param outReplyPayload Buffer containing the payload of the received packet
 *
 * @note The reply packet buffer is valid only until the next packet reception.
 */
void RpcConnection::sendPacketWithReply(const uint8_t ep, std::span<const std::byte> payload,
        std::span<const std::byte> &outReplyPayload) {
    std::span<const std::byte> replyPacket;

    // send the request
    const auto tag = this->sendPacket(ep, payload);

    // receive the response and validate the header
    this->receivePacket(replyPacket);
    if(replyPacket.empty()) {
        throw std::logic_error("received empty reply packet?");
    }

    auto &replyHdr = *reinterpret_cast<const struct rpc_header *>(replyPacket.data());

    if(replyHdr.endpoint != ep) {
#ifndef NDEBUG
        fprintf(stderr, "got ep %02x, expected %02x\n", replyHdr.endpoint, ep);
#endif
        throw std::runtime_error("ep mismatch");
    } else if(replyHdr.tag != tag) {
#ifndef NDEBUG
        fprintf(stderr, "got tag %02x, expected %02x\n", replyHdr.tag, tag);
#endif
        throw std::runtime_error("tag mismatch");
    }

    // get the payload
    outReplyPayload = replyPacket.subspan(offsetof(struct rpc_header, payload));
}


/**
 * @brief Receive a raw packet
 *
 * Attempt to read a complete packet from the socket, into our internal receive buffer, which will
 * be grown as needed.
 *
 * @param outPacket Region in the receive buffer encompassing this packet
 *
 * @note The buffer that's output from this method is only valid until the next packet is to be
 *       received on the connection.
 *
 * @remark If this call succeeds, the caller is guaranteed that the packet's header has passed
 *         validation and basic sanity checks, and that the packet contents are likely valid.
 */
void RpcConnection::receivePacket(std::span<const std::byte> &outPacket) {
    int err;

    // ensure we have space for _at least_ a header
    if(this->receiveBuf.size() < sizeof(struct rpc_header)) {
        this->receiveBuf.resize(sizeof(struct rpc_header));
    }

    // read the header first
    err = read(this->socket, this->receiveBuf.data(), sizeof(struct rpc_header));
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "read rpc message (header)");
    } else if(err != sizeof(struct rpc_header)) {
        // TODO: can we recover from this?
        throw std::runtime_error("insufficient read (header)");
    }

    // validate the header and get how much payload to read
    size_t payloadToRead{0};

    {
        auto &hdr = *reinterpret_cast<const struct rpc_header *>(this->receiveBuf.data());

        if(hdr.version != kRpcVersionLatest) {
            throw std::runtime_error("invalid rpc version");
        } else if(hdr.length < sizeof(struct rpc_header)) {
            throw std::runtime_error("invalid rpc message length");
        }

        payloadToRead = hdr.length - sizeof(hdr);
    }

    // read payload, if any
    if(payloadToRead) {
        this->receiveBuf.resize(sizeof(struct rpc_header) + payloadToRead);

        err = read(this->socket, this->receiveBuf.data() + offsetof(struct rpc_header, payload),
                payloadToRead);
        if(err == -1) {
            throw std::system_error(errno, std::generic_category(), "read rpc message (payload)");
        } else if(err != payloadToRead) {
            // TODO: can we recover from this?
            throw std::runtime_error("insufficient read (payload)");
        }
    }

    // output it
    outPacket = this->receiveBuf;
}

/**
 * @brief Send a raw packet
 *
 * Transmit the given buffer over the socket connection.
 *
 * @param packet Raw packet (including header) to send
 */
void RpcConnection::sendPacket(std::span<const std::byte> packet) {
    int err;
    err = write(this->socket, packet.data(), packet.size());

    // IO failed
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "write rpc message");
    }
    // partial write (TODO: handle this)
    else if(err != packet.size()) {
        throw std::runtime_error("insufficient write");
    }
}

