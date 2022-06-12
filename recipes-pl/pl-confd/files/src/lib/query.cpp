#include <unistd.h>
#include <cbor.h>

#include <system_error>
#include <vector>

#include <rpc/types.h>
#include "confd.h"

extern int gSocket;
extern uint8_t gNextTag;

/**
 * @brief Send a reuqest for the specified key.
 *
 * @return Tag associated with the message
 */
static uint8_t SendKeyRequest(const char *keyName) {
    int err;

    // serialize the request
    // TODO

    // stick a header on it
    uint8_t tag{++gNextTag};

    // transmit it
    // TODO

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

