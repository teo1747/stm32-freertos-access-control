/*
 * Calendar time and Unix time (seconds since 1970-01-01 00:00:00), for
 * 2000-01-01 to 2099-12-31, the DS3231's range. The access log stores Unix
 * time: 4 bytes an entry, easy to compare and subtract.
 */
#ifndef DATETIME_H
#define DATETIME_H

#include <stdbool.h>
#include <stdint.h>

struct datetime {
    uint16_t year;      /* 2000..2099 */
    uint8_t month;      /* 1..12 */
    uint8_t day;        /* 1..31 */
    uint8_t hour;       /* 0..23 */
    uint8_t minute;
    uint8_t second;
    uint8_t weekday;    /* 1 Monday .. 7 Sunday (ISO 8601) */
};

bool datetime_valid(const struct datetime *dt);
uint32_t datetime_to_unix(const struct datetime *dt);
void datetime_from_unix(uint32_t t, struct datetime *dt);

/* "YYYY-MM-DD hh:mm:ss" (or with a T): true if valid */
bool datetime_parse(const char *s, struct datetime *dt);
/* "YYYY-MM-DD hh:mm:ss", 20 bytes with the NUL */
void datetime_format(const struct datetime *dt, char *buf, unsigned size);

const char *datetime_weekday_name(uint8_t weekday);

#endif
