#include <unistd.h>
#include <cbor.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include "rpc/types.h"
#include "confd.h"
#include "RpcConnection.h"

/**
 * @brief Serialize a query for the given key name
 */
static std::vector<std::byte> SerializeKeyRequest(const char *keyName) {
    std::vector<std::byte> msgBuf;

    // build the request object
    cbor_item_t *root = cbor_new_definite_map(1);
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("key")),
        .value = cbor_move(cbor_build_string(keyName))
    });

    // serialize it
    size_t rootBufLen;
    unsigned char *rootBuf{nullptr};
    const size_t serializedBytes = cbor_serialize_alloc(root, &rootBuf, &rootBufLen);

    // copy it into a vector
    msgBuf.resize(serializedBytes);
    std::copy(reinterpret_cast<const std::byte *>(rootBuf),
            reinterpret_cast<const std::byte *>(rootBuf + serializedBytes), msgBuf.begin());

    // clean up
    free(rootBuf);
    cbor_decref(&root);

    return msgBuf;
}


int confd_get_string(const char *key, char *outStr, const size_t outStrLen) {
    try {
        std::span<const std::byte> replyPayload;

        // serialize request
        auto req = SerializeKeyRequest(key);

        // send the request and await the response
        std::lock_guard lg(RpcConnection::The()->lock);
        RpcConnection::The()->sendPacketWithReply(kConfigQuery, req, replyPayload);

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

