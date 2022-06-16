#ifndef RPC_HELPER_TYPES_H
#define RPC_HELPER_TYPES_H

#include <stdint.h>

#define kRpcVersionLatest 0x0100

/**
 * @brief RPC message header
 *
 * This is sent in the native byte order -- we only use local domain sockets -- and contains an
 * optional CBOR-encoded payload section. The same header is used for requests to the server, and
 * the replies it sends.
 */
struct rpc_header {
    /// protocol version: use kRpcVersionLatest
    uint16_t version;
    /// total length of message, in bytes (including this header)
    uint16_t length;

    /// message endpoint
    uint8_t endpoint;
    /// message tag: used to identify its response
    uint8_t tag;
    /// flags: currently only 0x01 is defined, which is set for replies to requests
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
    /// Access the configuration database (read)
    kConfigQuery                        = 0x01,
    /// Update the configuration database (write)
    kConfigUpdate                       = 0x02,
};

#endif
