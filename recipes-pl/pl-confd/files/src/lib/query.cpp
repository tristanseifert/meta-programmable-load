#include <unistd.h>
#include <cbor.h>

#include <system_error>
#include <vector>

#include <rpc/types.h>
#include "confd.h"
#include "rpc-helpers.h"

extern int gSocket;
extern uint8_t gNextTag;

/**
 * @brief Send a reuqest for the specified key.
 *
 * @return Tag associated with the message
 */
static uint8_t SendKeyRequest(const char *keyName) {
    std::vector<std::byte> msgBuf;

    // serialize the request
    cbor_item_t *root = cbor_new_definite_map(1);
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("key")),
        .value = cbor_move(cbor_build_string(keyName))
    });

    size_t rootBufLen;
    unsigned char *rootBuf{nullptr};
    const size_t serializedBytes = cbor_serialize_alloc(root, &rootBuf, &rootBufLen);

    // copy the data to message buffer
    msgBuf.resize(sizeof(struct rpc_header) + serializedBytes);
    auto hdr = reinterpret_cast<struct rpc_header *>(msgBuf.data());

    memcpy(hdr->payload, rootBuf, serializedBytes);
    free(rootBuf);
    cbor_decref(&root);

    // stick a header on it
    uint8_t tag{++gNextTag};

    hdr->version = kRpcVersionLatest;
    hdr->length = sizeof(struct rpc_header) + serializedBytes;
    hdr->endpoint = kConfigQuery;
    hdr->tag = tag;

    // transmit it
    SendPacket(msgBuf);
    return tag;
}



int confd_get_string(const char *key, char *outStr, const size_t outStrLen) {
    try {
        // send request
        const auto tag = SendKeyRequest(key);

        // TODO: read a response

        // TODO: decode response
    } catch(const std::system_error &e) {
        return -e.code().value();
    } catch(const std::exception &) {
        return -1;
    }

    // TODO: implement
    // return the appropriate value based on response
    return 1;
}

