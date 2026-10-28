/*
 * Non-volatile storage as the user database and the access log see it: the
 * 24LC256 on the board, a byte array in the unit tests.
 *
 * Layout (32 KB):
 *   0x0000  header: settings and the admin PIN    64 B, one page
 *   0x0040  users: 32 records                     64 B each, one page each
 *   0x0840  access log: a ring of 256 records     16 B each
 *   0x1840  free
 */
#ifndef STORE_H
#define STORE_H

#include <stddef.h>
#include <stdint.h>

#define STORE_HEADER_ADDR   0x0000u
#define STORE_USERS_ADDR    0x0040u
#define STORE_LOG_ADDR      0x0840u

struct store {
    /* 0 or a negative error */
    int (*read)(void *ctx, uint32_t addr, void *buf, size_t n);
    int (*write)(void *ctx, uint32_t addr, const void *buf, size_t n);
    void *ctx;
};

#endif
