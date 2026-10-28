/*
 * SHA-256 (FIPS 180-4), for the PIN hashes.
 */
#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>
#include <stdint.h>

#define SHA256_SIZE 32

struct sha256 {
    uint32_t h[8];
    uint64_t length;        /* bytes hashed so far */
    uint8_t block[64];
    size_t used;
};

void sha256_init(struct sha256 *s);
void sha256_update(struct sha256 *s, const void *data, size_t n);
void sha256_final(struct sha256 *s, uint8_t out[SHA256_SIZE]);
void sha256(const void *data, size_t n, uint8_t out[SHA256_SIZE]);

#endif
