#include <common.h>
#include <stddef.h>
#include <stdint.h>

#include "encoding_helpers.h"

/**
 * @brief Base32 encode binary data
 *
 * Transform the provided binary data into a base32-encoded string.
 *
 * @return Number of characters written
 */
int do_encode_base32(const void *inData, const size_t inDataBytes, char *output,
        const size_t outputBytes) {
    static const char *kChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

    if(inDataBytes < 0 || inDataBytes > (1 << 28)) {
        return -1;
    }

    const uint8_t *input = (const uint8_t *) inData;

    int count = 0;
    if(inDataBytes > 0) {
        uint32_t buffer = input[0];
        size_t next = 1;
        size_t bitsLeft = 8;

        while(count < outputBytes && (bitsLeft > 0 || next < inDataBytes)) {
            if(bitsLeft < 5) {
                if(next < inDataBytes) {
                    buffer <<= 8;
                    buffer |= input[next++] & 0xFF;
                    bitsLeft += 8;
                } else {
                    int pad = 5 - bitsLeft;
                    buffer <<= pad;
                    bitsLeft += pad;
                }
            }
            size_t index = 0x1F & (buffer >> (bitsLeft - 5));
            bitsLeft -= 5;
            output[count++] = kChars[index];
        }
    }
    if(count < outputBytes) {
        output[count] = '\000';
    }
    return count;
}

