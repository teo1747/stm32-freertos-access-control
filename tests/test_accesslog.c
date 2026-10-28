#include "test.h"
#include "accesslog.h"
#include "fake_store.h"

int main(void)
{
    static struct fake_store fs;
    struct store st;
    struct accesslog log;
    struct log_record r;
    struct card_uid card = { 4, { 0x04, 0xA1, 0xB2, 0xC3 } };

    /* Blank: empty */
    fake_store_init(&fs, &st);
    CHECK_INT(accesslog_open(&log, &st), 0);
    CHECK_INT(accesslog_count(&log), 0);
    CHECK_INT(accesslog_get(&log, 0, &r), -1);

    /* Appends, newest first */
    CHECK_INT(accesslog_append(&log, 1000, LOG_BOOT, -1, NULL), 0);
    CHECK_INT(accesslog_append(&log, 1001, LOG_GRANTED, 3, &card), 0);
    CHECK_INT(accesslog_count(&log), 2);
    CHECK_INT(accesslog_get(&log, 0, &r), 0);
    CHECK_INT(r.event, LOG_GRANTED);
    CHECK_INT(r.user, 3);
    CHECK_INT(r.uid[1], 0xA1);
    CHECK_INT(accesslog_get(&log, 1, &r), 0);
    CHECK_INT(r.event, LOG_BOOT);
    CHECK_INT(r.user, LOG_NO_USER);

    /* Reopen (a power cycle) finds the newest */
    CHECK_INT(accesslog_open(&log, &st), 0);
    CHECK_INT(accesslog_count(&log), 2);
    CHECK_INT(log.next_seq, 3);

    /* Wrap around the ring several times */
    for (uint32_t i = 0; i < 3 * LOG_CAPACITY + 10; i++)
        accesslog_append(&log, 2000 + i, LOG_DENIED_UNKNOWN, -1, &card);
    CHECK_INT(accesslog_count(&log), LOG_CAPACITY);
    CHECK_INT(accesslog_get(&log, 0, &r), 0);
    CHECK_INT(r.time, 2000 + 3 * LOG_CAPACITY + 9);
    CHECK_INT(accesslog_get(&log, LOG_CAPACITY - 1, &r), 0);
    CHECK_INT(r.time, 2000 + 2 * LOG_CAPACITY + 10);
    CHECK_INT(accesslog_get(&log, LOG_CAPACITY, &r), -1);
    uint32_t next = log.next_seq;
    CHECK_INT(accesslog_open(&log, &st), 0);
    CHECK_INT(log.next_seq, next);

    /* A record torn by a power cut (the newest) is skipped at open, and its
     * sequence number is reused by the next append */
    uint32_t newest_addr = STORE_LOG_ADDR + ((next - 2) % LOG_CAPACITY) * 16;
    fs.mem[newest_addr + 6] ^= 0x40;
    CHECK_INT(accesslog_open(&log, &st), 0);
    CHECK_INT(log.next_seq, next - 1);
    CHECK_INT(log.dropped, 1);
    CHECK_INT(accesslog_append(&log, 9999, LOG_EXIT, -1, NULL), 0);
    CHECK_INT(accesslog_get(&log, 0, &r), 0);
    CHECK_INT(r.time, 9999);

    /* A damaged older record reads as gone, its neighbours don't */
    fs.mem[STORE_LOG_ADDR + ((next - 6) % LOG_CAPACITY) * 16 + 4] ^= 0x01;
    CHECK_INT(accesslog_get(&log, 4, &r), -1);
    CHECK_INT(accesslog_get(&log, 3, &r), 0);
    CHECK_INT(accesslog_get(&log, 5, &r), 0);

    /* Clear */
    CHECK_INT(accesslog_clear(&log), 0);
    CHECK_INT(accesslog_count(&log), 0);
    CHECK_INT(accesslog_open(&log, &st), 0);
    CHECK_INT(accesslog_count(&log), 0);

    /* A failed write isn't counted */
    fs.fail_writes = 1;
    CHECK_INT(accesslog_append(&log, 1, LOG_BOOT, -1, NULL), -1);
    CHECK_INT(accesslog_count(&log), 0);

    CHECK_STR(log_event_name(LOG_DOOR_FORCED), "ALARM: door forced");
    CHECK_STR(log_event_name(200), "?");
    return test_report("accesslog");
}
