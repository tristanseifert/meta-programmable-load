#ifndef BOARD_ST_STM32MP1_ENCODING_HELPERS_H
#define BOARD_ST_STM32MP1_ENCODING_HELPERS_H

#include <common.h>
#include <stddef.h>
#include <stdint.h>

int do_encode_base32(const void *inData, const size_t inDataBytes, char *output,
        const size_t outputBytes);

#endif
