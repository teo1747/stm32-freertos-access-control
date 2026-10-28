/*
 * A RAM-backed store for the tests: blank (0xFF) like a new EEPROM, with a
 * switch to make writes fail.
 */
#ifndef FAKE_STORE_H
#define FAKE_STORE_H

#include <stdint.h>
#include <string.h>
#include "store.h"

struct fake_store {
    uint8_t mem[32768];
    int fail_writes;
    unsigned writes;
};

static inline int fake_read(void *ctx, uint32_t addr, void *buf, size_t n)
{
    struct fake_store *f = ctx;
    if (addr + n > sizeof f->mem)
        return -1;
    memcpy(buf, f->mem + addr, n);
    return 0;
}

static inline int fake_write(void *ctx, uint32_t addr, const void *buf, size_t n)
{
    struct fake_store *f = ctx;
    if (f->fail_writes || addr + n > sizeof f->mem)
        return -1;
    memcpy(f->mem + addr, buf, n);
    f->writes++;
    return 0;
}

static inline void fake_store_init(struct fake_store *f, struct store *st)
{
    memset(f->mem, 0xFF, sizeof f->mem);
    f->fail_writes = 0;
    f->writes = 0;
    st->read = fake_read;
    st->write = fake_write;
    st->ctx = f;
}

static inline uint32_t fake_random(void)
{
    static uint32_t x = 2463534242u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

#endif
