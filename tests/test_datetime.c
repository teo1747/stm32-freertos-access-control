#include "test.h"
#include "datetime.h"

static void known(const char *text, uint32_t unix_time, int weekday)
{
    struct datetime dt;
    CHECK(datetime_parse(text, &dt));
    CHECK_INT(datetime_to_unix(&dt), unix_time);
    CHECK_INT(dt.weekday, weekday);

    char back[20];
    datetime_from_unix(unix_time, &dt);
    datetime_format(&dt, back, sizeof back);
    CHECK_STR(back, text);
}

int main(void)
{
    /* Reference values from Python's calendar.timegm and isoweekday */
    known("2000-01-01 00:00:00", 946684800u, 6);
    known("2026-10-28 17:42:05", 1793209325u, 3);
    known("2024-02-29 23:59:59", 1709251199u, 4);
    known("2038-01-19 03:14:08", 2147483648u, 2);
    known("2099-12-31 23:59:59", 4102444799u, 4);

    /* Every day of the range survives the round trip, and days follow */
    struct datetime dt;
    uint32_t start = 946684800u;
    for (uint32_t day = 0; day < 36525; day++) {
        uint32_t t = start + day * 86400u + 43200u;
        datetime_from_unix(t, &dt);
        if (!datetime_valid(&dt) || datetime_to_unix(&dt) != t) {
            CHECK(0 && "round trip");
            break;
        }
    }
    datetime_from_unix(4102444799u, &dt);
    CHECK_INT(dt.year, 2099);

    /* Rejected input */
    CHECK(!datetime_parse("2026-02-29 00:00:00", &dt));     /* not a leap year */
    CHECK(!datetime_parse("2026-13-01 00:00:00", &dt));
    CHECK(!datetime_parse("2026-10-28 24:00:00", &dt));
    CHECK(!datetime_parse("1999-12-31 23:59:59", &dt));
    CHECK(!datetime_parse("2026-10-28 17:42", &dt));
    CHECK(!datetime_parse("2026-10-28 17:42:05x", &dt));
    CHECK(datetime_parse("2026-10-28T17:42:05", &dt));

    CHECK_STR(datetime_weekday_name(3), "Wed");
    return test_report("datetime");
}
