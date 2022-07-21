#ifndef UTIL_CBOR_H
#define UTIL_CBOR_H

#include <cbor.h>

namespace Util {
/**
 * @brief Read a CBOR integer value
 *
 * @param item CBOR item to read
 *
 * @remark The item must be an unsigned integer or the results are undefined.
 */
constexpr inline static uint64_t CborReadUint(const cbor_item_t *item) {
    switch(cbor_int_get_width(item)) {
        case CBOR_INT_8:
            return cbor_get_uint8(item);
        case CBOR_INT_16:
            return cbor_get_uint16(item);
        case CBOR_INT_32:
            return cbor_get_uint32(item);
        case CBOR_INT_64:
            return cbor_get_uint64(item);
    }
}
}

#endif
