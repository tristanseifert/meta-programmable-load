#ifndef RPC_HELPERS_H
#define RPC_HELPERS_H

#include <unistd.h>

#include <cstddef>
#include <span>
#include <system_error>

// RPC socket (defined in connection.cpp)
extern int gSocket;

/**
 * @brief Transmit the given packet
 *
 * Sends the packet data over the RPC connection. It's assumed the packet already has a header
 * attached to it.
 */
static void SendPacket(std::span<std::byte> packet) {
    int err;
    err = write(gSocket, packet.data(), packet.size());

    // IO failed
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "write rpc message");
    }
    // partial write
    else if(err != packet.size()) {
        throw std::runtime_error("insufficient write");
    }
}


#endif
