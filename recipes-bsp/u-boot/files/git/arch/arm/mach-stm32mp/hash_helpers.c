#include <common.h>
#include <stddef.h>
#include <stdint.h>

#include "hash_helpers.h"

//-----------------------------------------------------------------------------
// Platform-specific functions and macros

#define	FORCE_INLINE __attribute__((always_inline)) inline

static inline uint32_t rotl32 ( uint32_t x, int8_t r ) {
    return (x << r) | (x >> (32 - r));
}

#define	ROTL32(x,y)	rotl32(x,y)


//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

FORCE_INLINE uint32_t read_little_endian32(const uint8_t * array, uint64_t pos) {
    return *((uint32_t*)(array + pos));
}

FORCE_INLINE uint32_t getblock32 ( const uint32_t * p, int i ) {
    return read_little_endian32((uint8_t *) p, 4*i);
}

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

FORCE_INLINE uint32_t fmix32 ( uint32_t h ) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

/**
 * @brief Hash the given data with MurmurHash3
 *
 * This is a hashing algorithm written by Austin Appleby, placed in the public domain.
 */
uint32_t do_hash_murmurhash3(const void *inData, const size_t inDataBytes, const uint32_t seed) {
    const uint8_t * data = (const uint8_t*) inData;
    const int nblocks = inDataBytes / 4;

    uint32_t h1 = seed;

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    //----------
    // body

    const uint32_t * blocks = (const uint32_t *)(data + nblocks*4);

    for(int i = -nblocks; i; i++) {
        uint32_t k1 = getblock32(blocks,i);

        k1 *= c1;
        k1 = ROTL32(k1,15);
        k1 *= c2;

        h1 ^= k1;
        h1 = ROTL32(h1,13);
        h1 = h1*5+0xe6546b64;
    }

    //----------
    // tail

    const uint8_t * tail = (const uint8_t*)(data + nblocks*4);

    uint32_t k1 = 0;

    switch(inDataBytes & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
            k1 *= c1; k1 = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
    };

    //----------
    // finalization

    h1 ^= inDataBytes;

    h1 = fmix32(h1);

    return h1;
}

