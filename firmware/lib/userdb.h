/*
 * Users and settings in non-volatile storage, cached in RAM so a card is
 * looked up without a bus transfer. Each user is one 64-byte record (one
 * EEPROM page, written in one go) with a CRC; a record that fails its CRC
 * at load is dropped, so damage fails closed.
 *
 * PINs are kept as salted, stretched hashes: SHA-256 iterated 1024 times
 * over a random salt and the PIN. A dumped EEPROM doesn't give the PINs
 * away for free, and each guess costs the attacker 1024 hashes. (A 4 to 6
 * digit PIN can still be brute-forced offline: the lockout and the
 * terminal's physical security are the real defence.)
 */
#ifndef USERDB_H
#define USERDB_H

#include <stdbool.h>
#include <stdint.h>
#include "card.h"
#include "store.h"

#define USERS_MAX       32
#define USER_NAME_MAX   16
#define PIN_MIN         4
#define PIN_MAX         6

/* User flags */
#define USER_ADMIN      0x01    /* clears alarms with a card + PIN */
#define USER_PIN        0x02    /* card and PIN; without it, the card alone */
#define USER_DISABLED   0x04

struct user {
    struct card_uid uid;
    char name[USER_NAME_MAX + 1];
    uint8_t flags;
    uint8_t from_hour;          /* allowed from from_hour:00 to to_hour:00; */
    uint8_t to_hour;            /* 0 and 24: any time; from > to wraps midnight */
};

struct settings {
    uint16_t unlock_s;          /* the lock stays open this long without the door opening */
    uint16_t lockout_s;         /* after max_attempts wrong PINs; doubles each time */
    uint16_t door_held_s;       /* open longer: alarm */
    uint8_t pin_timeout_s;
    uint8_t max_attempts;
};

/* The records as stored, here so the cache's size is known */
struct user_record {
    uint8_t used;
    uint8_t uid_len;
    uint8_t uid[CARD_UID_MAX];
    char name[USER_NAME_MAX + 1];
    uint8_t flags;
    uint8_t from_hour;
    uint8_t to_hour;
    uint8_t salt[4];
    uint8_t pin_hash[16];
    uint8_t reserved[10];
    uint16_t crc;
};

struct header_record {
    uint32_t magic;
    uint16_t version;
    uint16_t unlock_s;
    uint16_t lockout_s;
    uint16_t door_held_s;
    uint8_t pin_timeout_s;
    uint8_t max_attempts;
    uint8_t admin_salt[4];
    uint8_t admin_hash[16];
    uint8_t reserved[28];
    uint16_t crc;
};

struct userdb {
    const struct store *st;
    uint32_t (*random)(void);
    struct header_record hdr;
    struct user_record rec[USERS_MAX];
    bool formatted;             /* the storage was blank (or foreign) at open */
    unsigned dropped;           /* records that failed their CRC at open */
};

enum {
    USERDB_OK = 0,
    USERDB_IO = -1,             /* the storage failed */
    USERDB_FULL = -2,
    USERDB_EXISTS = -3,         /* that card is already enrolled */
    USERDB_INVALID = -4,        /* a bad PIN, name, hour or slot */
};

#define ADMIN_DEFAULT_PIN "1234"

/* Loads the header and the users; a blank storage gets the default
 * settings and the default admin PIN. random seeds the salts. */
int userdb_open(struct userdb *db, const struct store *st, uint32_t (*random)(void));

int userdb_find(const struct userdb *db, const struct card_uid *uid);
int userdb_get(const struct userdb *db, int slot, struct user *u);
unsigned userdb_count(const struct userdb *db);
/* Returns the new user's slot */
int userdb_add(struct userdb *db, const struct user *u, const char *pin);
/* Name, flags and hours; the card and the PIN stay */
int userdb_update(struct userdb *db, int slot, const struct user *u);
int userdb_set_pin(struct userdb *db, int slot, const char *pin);
int userdb_remove(struct userdb *db, int slot);
bool userdb_check_pin(const struct userdb *db, int slot, const char *pin);
/* May the user come in at this hour (0..23)? */
bool user_allowed_at(const struct user *u, unsigned hour);

void userdb_settings(const struct userdb *db, struct settings *s);
int userdb_set_settings(struct userdb *db, const struct settings *s);
bool userdb_check_admin(const struct userdb *db, const char *pin);
int userdb_set_admin(struct userdb *db, const char *pin);
bool userdb_admin_is_default(const struct userdb *db);
/* Every user gone, the settings and the admin PIN back to their defaults */
int userdb_wipe(struct userdb *db);

bool pin_valid(const char *pin);
const char *userdb_strerror(int err);

#endif
