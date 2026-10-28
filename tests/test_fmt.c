#include <stdint.h>
#include <limits.h>
#include "test.h"
#include "fmt.h"

/* The host's snprintf is the reference */
#define SAME(...) do { \
    char ours[64], ref[64]; \
    int n1 = fmt_snprintf(ours, sizeof ours, __VA_ARGS__); \
    int n2 = snprintf(ref, sizeof ref, __VA_ARGS__); \
    CHECK_STR(ours, ref); \
    CHECK_INT(n1, n2); \
} while (0)

int main(void)
{
    SAME("plain text");
    SAME("%d %d %d", 0, 42, -42);
    SAME("%i", INT_MIN);
    SAME("%d", INT_MAX);
    SAME("%u", UINT_MAX);
    SAME("%x %X", 0xbeefu, 0xbeefu);
    SAME("%08x", 0x1234u);
    SAME("%5d|%-5d|%05d", 42, 42, -42);
    SAME("%ld %lu %lx", -123456789L, 4000000000UL, 0xdeadbeefUL);
    SAME("%zu", (size_t)12345);
    SAME("%c%c%c", 'a', 'b', 'c');
    SAME("%s|%10s|%-10s|", "abc", "right", "left");
    SAME("%.3s|%.*s", "abcdef", 2, "xyz");
    SAME("%*d|%-*d", 6, 7, 6, 7);
    SAME("100%%");
    SAME("%02u:%02u:%02u", 7u, 5u, 0u);

    /* Truncation: always terminated, returns the full length */
    char small[6];
    CHECK_INT(fmt_snprintf(small, sizeof small, "%s", "truncated"), 9);
    CHECK_STR(small, "trunc");
    CHECK_INT(fmt_snprintf(small, 0, "%d", 12345), 5);
    CHECK_INT(fmt_snprintf(NULL, 0, "%s", "x"), 1);

    /* A NULL string, and a lone % at the end */
    char buf[16];
    fmt_snprintf(buf, sizeof buf, "%s", (const char *)NULL);
    CHECK_STR(buf, "(null)");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
    fmt_snprintf(buf, sizeof buf, "abc%");
#pragma GCC diagnostic pop
    CHECK_STR(buf, "abc");

    return test_report("fmt");
}
