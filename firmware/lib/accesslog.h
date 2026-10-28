/*
 * The access log: a ring of 256 records of 16 bytes in non-volatile
 * storage. Each record carries a sequence number and a CRC, and record n
 * lives at slot (n - 1) % 256, so there is no head pointer to keep: at
 * open, the highest valid sequence number is the newest record. A record
 * torn by a power cut fails its CRC and is skipped.
 */
#ifndef ACCESSLOG_H
#define ACCESSLOG_H

#include <stdint.h>
#include "card.h"
#include "store.h"

#define LOG_CAPACITY 256

enum log_event {
    LOG_BOOT = 1,
    LOG_GRANTED,
    LOG_DENIED_UNKNOWN,
    LOG_DENIED_PIN,
    LOG_DENIED_DISABLED,
    LOG_DENIED_SCHEDULE,
    LOG_LOCKOUT,
    LOG_PIN_TIMEOUT,
    LOG_EXIT,
    LOG_REMOTE_UNLOCK,
    LOG_DOOR_FORCED,
    LOG_DOOR_HELD,
    LOG_ALARM_CLEARED,
    LOG_USER_ADDED,
    LOG_USER_REMOVED,
    LOG_ADMIN_LOGIN,
    LOG_ADMIN_FAILED,
    LOG_TIME_SET,
    LOG_EVENT_COUNT
};

#define LOG_NO_USER 0xFF

struct log_record {
    uint32_t seq;               /* 1, 2, ...; 0 and 0xFFFFFFFF: empty */
    uint32_t time;              /* Unix time, 0 if the clock wasn't set */
    uint8_t event;
    uint8_t user;               /* the slot, or LOG_NO_USER */
    uint8_t uid[4];             /* the card's first 4 bytes */
    uint16_t crc;
};

struct accesslog {
    const struct store *st;
    uint32_t next_seq;
    unsigned dropped;           /* records that failed their CRC at open */
};

int accesslog_open(struct accesslog *l, const struct store *st);
int accesslog_append(struct accesslog *l, uint32_t time, enum log_event event, int user,
                     const struct card_uid *uid);
/* back = 0 is the newest; returns 0, or -1 if it's gone (overwritten,
 * damaged or never written) */
int accesslog_get(const struct accesslog *l, unsigned back, struct log_record *r);
unsigned accesslog_count(const struct accesslog *l);
int accesslog_clear(struct accesslog *l);
const char *log_event_name(unsigned event);

#endif
