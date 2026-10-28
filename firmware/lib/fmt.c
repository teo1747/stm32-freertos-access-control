#include <stdbool.h>
#include "fmt.h"

struct out {
    char *buf;
    size_t size;
    size_t len;
};

static void put(struct out *o, char c)
{
    if (o->len + 1 < o->size)
        o->buf[o->len] = c;
    o->len++;
}

static void repeat(struct out *o, char c, int n)
{
    while (n-- > 0)
        put(o, c);
}

/* Digits of v in base, least significant first; returns how many */
static int digits(char *tmp, unsigned long v, unsigned base, bool upper)
{
    const char *set = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int n = 0;
    do {
        tmp[n++] = set[v % base];
        v /= base;
    } while (v);
    return n;
}

int fmt_vsnprintf(char *buf, size_t size, const char *f, va_list ap)
{
    struct out o = { buf, size, 0 };

    for (; *f; f++) {
        if (*f != '%') {
            put(&o, *f);
            continue;
        }
        f++;

        bool left = false, zero = false;
        for (;; f++) {
            if (*f == '-')
                left = true;
            else if (*f == '0')
                zero = true;
            else
                break;
        }

        int width = 0;
        if (*f == '*') {
            width = va_arg(ap, int);
            if (width < 0) {
                left = true;
                width = -width;
            }
            f++;
        } else {
            while (*f >= '0' && *f <= '9')
                width = width * 10 + (*f++ - '0');
        }

        int prec = -1;
        if (*f == '.') {
            f++;
            prec = 0;
            if (*f == '*') {
                prec = va_arg(ap, int);
                f++;
            } else {
                while (*f >= '0' && *f <= '9')
                    prec = prec * 10 + (*f++ - '0');
            }
        }

        char size_mod = 0;
        if (*f == 'l' || *f == 'z')
            size_mod = *f++;

        char tmp[24];
        int n = 0;
        bool neg = false;
        bool reversed = true;
        const char *s = tmp;

        switch (*f) {
        case 'd':
        case 'i': {
            long v = size_mod == 'l' ? va_arg(ap, long)
                   : size_mod == 'z' ? (long)va_arg(ap, size_t)
                   : va_arg(ap, int);
            neg = v < 0;
            n = digits(tmp, neg ? 0ul - (unsigned long)v : (unsigned long)v, 10, false);
            break;
        }
        case 'u':
        case 'x':
        case 'X': {
            unsigned long v = size_mod == 'l' ? va_arg(ap, unsigned long)
                            : size_mod == 'z' ? va_arg(ap, size_t)
                            : va_arg(ap, unsigned);
            n = digits(tmp, v, *f == 'u' ? 10 : 16, *f == 'X');
            break;
        }
        case 'c':
            tmp[0] = (char)va_arg(ap, int);
            n = 1;
            break;
        case 's':
            s = va_arg(ap, const char *);
            if (!s)
                s = "(null)";
            while (s[n] && (prec < 0 || n < prec))
                n++;
            reversed = false;
            zero = false;
            break;
        case '%':
            put(&o, '%');
            continue;
        case '\0':
            f--;        /* a lone '%' at the end: stop at the NUL */
            continue;
        default:
            /* Unknown: print it as it was written */
            put(&o, '%');
            put(&o, *f);
            continue;
        }

        int pad = width - n - (neg ? 1 : 0);
        if (!left && !zero)
            repeat(&o, ' ', pad);
        if (neg)
            put(&o, '-');
        if (!left && zero)
            repeat(&o, '0', pad);
        for (int i = 0; i < n; i++)
            put(&o, reversed ? s[n - 1 - i] : s[i]);
        if (left)
            repeat(&o, ' ', pad);
    }

    if (size)
        buf[o.len < size ? o.len : size - 1] = '\0';
    return (int)o.len;
}

int fmt_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = fmt_vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}
