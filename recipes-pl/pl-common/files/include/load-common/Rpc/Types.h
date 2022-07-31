/**
 * @file
 *
 * @brief RPC type definitions
 */
#ifndef PLCOMMON_RPC_TYPES_H
#define PLCOMMON_RPC_TYPES_H

#include <cstddef>
#include <cstdint>

namespace PlCommon::Rpc {
/**
 * @brief Current RPC version
 */
constexpr static const uint16_t kRpcVersionLatest{0x0100};

/**
 * @brief RPC header flags
 *
 * This enum defines allowable values for the `rpc_header.flags` member.
 *
 * @seealso struct RpcHeader
 */
enum RpcFlags {
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
struct RpcHeader {
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
     * @seeAlso enum RpcFlags
     */
    uint8_t flags;

    /// reserved, not currently used: set to 0
    uint8_t reserved;

    /// optional message payload (dependant upon endpoint)
    uint8_t payload[];
} __attribute__((packed));

}

#endif
