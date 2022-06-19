#ifndef BOARD_ST_STM32MP1_HASH_HELPERS_H
#define BOARD_ST_STM32MP1_HASH_HELPERS_H

#include <common.h>
#include <stddef.h>
#include <stdint.h>

uint32_t do_hash_murmurhash3(const void *inData, const size_t inDataBytes, const uint32_t seed);

#endif
