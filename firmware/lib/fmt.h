/*
 * A small snprintf: reentrant, no heap, no floating point, a few hundred
 * bytes of code instead of newlib's printf.
 *
 * Conversions: %d %i %u %x %X %c %s %%, with the flags '-' and '0', a width
 * (or *), a precision for %s (or .*), and the length modifiers l and z.
 */
#ifndef FMT_H
#define FMT_H

#include <stdarg.h>
#include <stddef.h>

/* Like vsnprintf: writes at most size - 1 characters and a NUL, returns the
 * length the whole output would have */
int fmt_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int fmt_snprintf(char *buf, size_t size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#endif
