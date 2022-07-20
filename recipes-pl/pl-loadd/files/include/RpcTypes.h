/**
 * @file
 *
 * @brief Load interface deamon RPC type definitions
 */
#ifndef LOADD_RPCTYPES_H
#define LOADD_RPCTYPES_H

#include <stddef.h>
#include <stdint.h>

#define kRpcVersionLatest 0x0100

/**
 * @brief RPC header flags
 *
 * This enum defines allowable values for the `rpc_header.flags` member.
 *
 * @seealso struct rpc_header
 */
enum rpc_flags {
    kRpcFlagReply                       = (1 << 0),
    kRpcFlagBroadcast                   = (1 << 1),
};

/**
 * @brief RPC message header
 *
 * This is sent in the native byte order -- we only use local domain sockets -- and contains an
 * optional CBOR-encoded payload section. The same header is used for requests to the server, and
 * the replies it sends.
 *
 * @remark The same packet header is used when sending messages to the remote endpoint on the M4
 *         but it uses the same byte ordering.
 */
struct rpc_header {
    /// protocol version: use kRpcVersionLatest
    uint16_t version;
    /// total length of message, in bytes (including this header)
    uint16_t length;

    /**
     * @brief Message endpoint
     *
     * @remark This is different from endpoints on the M4 firmware -- these are arbitrary
     *         constructs wholly defined by loadd for purposes of segmenting the RPC handler.
     */
    uint8_t endpoint;
    /// message tag: used to identify its response
    uint8_t tag;
    /**
     * @brief Flags bit field
     *
     * The following flags are defined:
     *
     * - (1 << 0): Set for replies to a previous request
     * - (1 << 7): Indicates the message is a broadcast message
     */
    uint8_t flags;

    /// reserved, not currently used: set to 0
    uint8_t reserved;

    /// optional message payload (dependant upon tag)
    uint8_t payload[];
} __attribute__((packed));

/**
 * @brief RPC message endpoints
 */
enum rpc_endpoint {
    // TODO: define message types
};

#endif
