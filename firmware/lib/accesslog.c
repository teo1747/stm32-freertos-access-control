#include <stddef.h>
#include <string.h>
#include "accesslog.h"
#include "crc16.h"

_Static_assert(sizeof(struct log_record) == 16, "four records a page");

static uint32_t slot_addr(uint32_t seq)
{
    return STORE_LOG_ADDR + ((seq - 1u) % LOG_CAPACITY) * (uint32_t)sizeof(struct log_record);
}

static int valid(const struct log_record *r)
{
    return r->seq != 0 && r->seq != 0xFFFFFFFFu
        && r->crc == crc16(r, offsetof(struct log_record, crc));
}

int accesslog_open(struct accesslog *l, const struct store *st)
{
    /* A page (4 records) at a time: 64 reads rather than 256 */
    struct log_record page[4];
    uint32_t newest = 0;

    memset(l, 0, sizeof *l);
    l->st = st;
    for (unsigned i = 0; i < LOG_CAPACITY; i += 4) {
        if (st->read(st->ctx, STORE_LOG_ADDR + i * sizeof page[0], page, sizeof page))
            return -1;
        for (unsigned j = 0; j < 4; j++) {
            if (valid(&page[j])) {
                /* A record in the wrong slot is someone else's data */
                if ((page[j].seq - 1u) % LOG_CAPACITY == i + j && page[j].seq > newest)
                    newest = page[j].seq;
            } else if (page[j].seq != 0xFFFFFFFFu && page[j].seq != 0) {
                l->dropped++;
            }
        }
    }
    l->next_seq = newest + 1;
    return 0;
}

int accesslog_append(struct accesslog *l, uint32_t time, enum log_event event, int user,
                     const struct card_uid *uid)
{
    struct log_record r = {
        .seq = l->next_seq,
        .time = time,
        .event = (uint8_t)event,
        .user = user >= 0 && user < LOG_NO_USER ? (uint8_t)user : LOG_NO_USER,
    };
    if (uid)
        memcpy(r.uid, uid->bytes, uid->len < 4 ? uid->len : 4);
    r.crc = crc16(&r, offsetof(struct log_record, crc));

    if (l->st->write(l->st->ctx, slot_addr(r.seq), &r, sizeof r))
        return -1;
    l->next_seq++;
    return 0;
}

unsigned accesslog_count(const struct accesslog *l)
{
    uint32_t written = l->next_seq - 1;
    return written < LOG_CAPACITY ? written : LOG_CAPACITY;
}

int accesslog_get(const struct accesslog *l, unsigned back, struct log_record *r)
{
    if (back >= accesslog_count(l))
        return -1;
    uint32_t seq = l->next_seq - 1 - back;
    if (l->st->read(l->st->ctx, slot_addr(seq), r, sizeof *r))
        return -1;
    return valid(r) && r->seq == seq ? 0 : -1;
}

int accesslog_clear(struct accesslog *l)
{
    /* Erased records: a page of 0xFF at a time */
    uint8_t blank[64];
    memset(blank, 0xFF, sizeof blank);
    for (unsigned off = 0; off < LOG_CAPACITY * sizeof(struct log_record); off += sizeof blank)
        if (l->st->write(l->st->ctx, STORE_LOG_ADDR + off, blank, sizeof blank))
            return -1;
    l->next_seq = 1;
    return 0;
}

const char *log_event_name(unsigned event)
{
    static const char *const names[LOG_EVENT_COUNT] = {
        [LOG_BOOT] = "boot",
        [LOG_GRANTED] = "granted",
        [LOG_DENIED_UNKNOWN] = "denied: unknown card",
        [LOG_DENIED_PIN] = "denied: wrong PIN",
        [LOG_DENIED_DISABLED] = "denied: disabled",
        [LOG_DENIED_SCHEDULE] = "denied: outside hours",
        [LOG_LOCKOUT] = "lockout",
        [LOG_PIN_TIMEOUT] = "PIN timeout",
        [LOG_EXIT] = "exit button",
        [LOG_REMOTE_UNLOCK] = "remote unlock",
        [LOG_DOOR_FORCED] = "ALARM: door forced",
        [LOG_DOOR_HELD] = "ALARM: door held open",
        [LOG_ALARM_CLEARED] = "alarm cleared",
        [LOG_USER_ADDED] = "user added",
        [LOG_USER_REMOVED] = "user removed",
        [LOG_ADMIN_LOGIN] = "admin login",
        [LOG_ADMIN_FAILED] = "admin login failed",
        [LOG_TIME_SET] = "time set",
        [LOG_DENIED_ALARM] = "denied: alarm active",
    };
    return event < LOG_EVENT_COUNT && names[event] ? names[event] : "?";
}
