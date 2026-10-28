#include <string.h>
#include "card.h"
#include "fmt.h"

bool card_uid_equal(const struct card_uid *a, const struct card_uid *b)
{
    return a->len == b->len && memcmp(a->bytes, b->bytes, a->len) == 0;
}

void card_uid_format(const struct card_uid *uid, char *buf, unsigned size)
{
    unsigned pos = 0;
    buf[0] = '\0';
    for (unsigned i = 0; i < uid->len && pos + 3 < size; i++)
        pos += (unsigned)fmt_snprintf(buf + pos, size - pos, i ? ":%02X" : "%02X", uid->bytes[i]);
}

static int hex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool card_uid_parse(const char *s, struct card_uid *uid)
{
    struct card_uid u = { 0 };

    while (*s) {
        if (*s == ':' || *s == ' ' || *s == '-') {
            s++;
            continue;
        }
        int hi = hex(s[0]), lo = hi < 0 ? -1 : hex(s[1]);
        if (hi < 0 || lo < 0 || u.len == CARD_UID_MAX)
            return false;
        u.bytes[u.len++] = (uint8_t)(hi << 4 | lo);
        s += 2;
    }
    if (u.len != 4 && u.len != 7 && u.len != 10)
        return false;
    *uid = u;
    return true;
}
