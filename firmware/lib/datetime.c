#include "datetime.h"
#include "fmt.h"

static bool leap(unsigned year)
{
    /* 2000 is a leap year and 2100 is out of range: every fourth year */
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static unsigned days_in_month(unsigned year, unsigned month)
{
    static const uint8_t days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    return days[month - 1] + (month == 2 && leap(year));
}

bool datetime_valid(const struct datetime *dt)
{
    return dt->year >= 2000 && dt->year <= 2099
        && dt->month >= 1 && dt->month <= 12
        && dt->day >= 1 && dt->day <= days_in_month(dt->year, dt->month)
        && dt->hour < 24 && dt->minute < 60 && dt->second < 60;
}

/* Days from 1970-01-01 to the civil date (Howard Hinnant's algorithm) */
static int32_t days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    int era = y / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int32_t)doe - 719468;
}

uint32_t datetime_to_unix(const struct datetime *dt)
{
    int32_t days = days_from_civil(dt->year, dt->month, dt->day);
    return (uint32_t)days * 86400u + dt->hour * 3600u + dt->minute * 60u + dt->second;
}

void datetime_from_unix(uint32_t t, struct datetime *dt)
{
    uint32_t days = t / 86400u;
    uint32_t secs = t % 86400u;

    dt->hour = (uint8_t)(secs / 3600u);
    dt->minute = (uint8_t)(secs / 60u % 60u);
    dt->second = (uint8_t)(secs % 60u);
    /* 1970-01-01 was a Thursday */
    dt->weekday = (uint8_t)((days + 3u) % 7u + 1u);

    int32_t z = (int32_t)days + 719468;
    int32_t era = z / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = mp < 10 ? mp + 3 : mp - 9;
    dt->year = (uint16_t)((int32_t)yoe + era * 400 + (m <= 2));
    dt->month = (uint8_t)m;
    dt->day = (uint8_t)d;
}

static bool number(const char **s, unsigned digits, unsigned *out)
{
    unsigned v = 0;
    for (unsigned i = 0; i < digits; i++) {
        char c = (*s)[i];
        if (c < '0' || c > '9')
            return false;
        v = v * 10 + (unsigned)(c - '0');
    }
    *s += digits;
    *out = v;
    return true;
}

static bool expect(const char **s, char c)
{
    if (**s != c)
        return false;
    (*s)++;
    return true;
}

bool datetime_parse(const char *s, struct datetime *dt)
{
    unsigned y, mo, d, h, mi, se;

    if (!number(&s, 4, &y) || !expect(&s, '-') || !number(&s, 2, &mo) || !expect(&s, '-')
        || !number(&s, 2, &d))
        return false;
    if (*s != ' ' && *s != 'T')
        return false;
    s++;
    if (!number(&s, 2, &h) || !expect(&s, ':') || !number(&s, 2, &mi) || !expect(&s, ':')
        || !number(&s, 2, &se) || *s)
        return false;

    struct datetime t = { (uint16_t)y, (uint8_t)mo, (uint8_t)d, (uint8_t)h, (uint8_t)mi, (uint8_t)se, 0 };
    if (!datetime_valid(&t))
        return false;
    datetime_from_unix(datetime_to_unix(&t), &t);     /* fills the weekday */
    *dt = t;
    return true;
}

void datetime_format(const struct datetime *dt, char *buf, unsigned size)
{
    fmt_snprintf(buf, size, "%04u-%02u-%02u %02u:%02u:%02u", dt->year, dt->month, dt->day,
                 dt->hour, dt->minute, dt->second);
}

const char *datetime_weekday_name(uint8_t weekday)
{
    static const char *const names[] = { "?", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
    return names[weekday <= 7 ? weekday : 0];
}
