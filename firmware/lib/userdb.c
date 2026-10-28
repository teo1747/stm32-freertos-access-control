#include <string.h>
#include "userdb.h"
#include "crc16.h"
#include "sha256.h"

#define MAGIC           0x31544341u     /* "ACT1" */
#define VERSION         1
#define RECORD_USED     0x5A
#define HASH_ROUNDS     1024

_Static_assert(sizeof(struct user_record) == 64, "a user is one EEPROM page");
_Static_assert(sizeof(struct header_record) == 64, "the header is one EEPROM page");

static const struct settings defaults = {
    .unlock_s = 5,
    .lockout_s = 30,
    .door_held_s = 20,
    .pin_timeout_s = 15,
    .max_attempts = 3,
};

bool pin_valid(const char *pin)
{
    size_t n = strlen(pin);
    if (n < PIN_MIN || n > PIN_MAX)
        return false;
    for (size_t i = 0; i < n; i++)
        if (pin[i] < '0' || pin[i] > '9')
            return false;
    return true;
}

/* H = SHA-256(salt || pin), then H = SHA-256(H || salt) 1023 more times */
static void hash_pin(const uint8_t salt[4], const char *pin, uint8_t out[16])
{
    uint8_t h[SHA256_SIZE];
    struct sha256 s;

    sha256_init(&s);
    sha256_update(&s, salt, 4);
    sha256_update(&s, pin, strlen(pin));
    sha256_final(&s, h);
    for (int i = 1; i < HASH_ROUNDS; i++) {
        sha256_init(&s);
        sha256_update(&s, h, sizeof h);
        sha256_update(&s, salt, 4);
        sha256_final(&s, h);
    }
    memcpy(out, h, 16);
}

/* Compares without an early exit, so the time taken says nothing */
static bool same(const uint8_t *a, const uint8_t *b, size_t n)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < n; i++)
        diff |= a[i] ^ b[i];
    return diff == 0;
}

static void new_salt(struct userdb *db, uint8_t salt[4])
{
    uint32_t r = db->random();
    memcpy(salt, &r, 4);
}

static int save_header(struct userdb *db)
{
    db->hdr.crc = crc16(&db->hdr, offsetof(struct header_record, crc));
    return db->st->write(db->st->ctx, STORE_HEADER_ADDR, &db->hdr, sizeof db->hdr) ? USERDB_IO : USERDB_OK;
}

static int save_record(struct userdb *db, int slot)
{
    struct user_record *r = &db->rec[slot];
    r->crc = crc16(r, offsetof(struct user_record, crc));
    uint32_t addr = STORE_USERS_ADDR + (uint32_t)slot * sizeof *r;
    return db->st->write(db->st->ctx, addr, r, sizeof *r) ? USERDB_IO : USERDB_OK;
}

static void default_header(struct userdb *db)
{
    memset(&db->hdr, 0, sizeof db->hdr);
    db->hdr.magic = MAGIC;
    db->hdr.version = VERSION;
    db->hdr.unlock_s = defaults.unlock_s;
    db->hdr.lockout_s = defaults.lockout_s;
    db->hdr.door_held_s = defaults.door_held_s;
    db->hdr.pin_timeout_s = defaults.pin_timeout_s;
    db->hdr.max_attempts = defaults.max_attempts;
    new_salt(db, db->hdr.admin_salt);
    hash_pin(db->hdr.admin_salt, ADMIN_DEFAULT_PIN, db->hdr.admin_hash);
}

static bool used(const struct user_record *r)
{
    return r->used == RECORD_USED;
}

int userdb_open(struct userdb *db, const struct store *st, uint32_t (*random)(void))
{
    memset(db, 0, sizeof *db);
    db->st = st;
    db->random = random;

    if (st->read(st->ctx, STORE_HEADER_ADDR, &db->hdr, sizeof db->hdr))
        return USERDB_IO;
    if (db->hdr.magic != MAGIC || db->hdr.version != VERSION
        || db->hdr.crc != crc16(&db->hdr, offsetof(struct header_record, crc))) {
        /* Blank, foreign or damaged: start over. The user records of
         * another layout must not be taken for ours. */
        db->formatted = true;
        default_header(db);
        if (st->read(st->ctx, STORE_USERS_ADDR, db->rec, sizeof db->rec))
            return USERDB_IO;
        for (int i = 0; i < USERS_MAX; i++) {
            bool was_used = used(&db->rec[i]);
            memset(&db->rec[i], 0, sizeof db->rec[i]);
            /* A blank chip reads 0xFF: only what looks used is cleared */
            if (was_used && save_record(db, i))
                return USERDB_IO;
        }
        return save_header(db);
    }

    if (st->read(st->ctx, STORE_USERS_ADDR, db->rec, sizeof db->rec))
        return USERDB_IO;
    for (int i = 0; i < USERS_MAX; i++) {
        struct user_record *r = &db->rec[i];
        if (!used(r))
            continue;
        if (r->crc != crc16(r, offsetof(struct user_record, crc)) || r->uid_len > CARD_UID_MAX) {
            memset(r, 0, sizeof *r);
            db->dropped++;
        }
    }
    return USERDB_OK;
}

static void record_uid(const struct user_record *r, struct card_uid *uid)
{
    uid->len = r->uid_len;
    memcpy(uid->bytes, r->uid, CARD_UID_MAX);
}

int userdb_find(const struct userdb *db, const struct card_uid *uid)
{
    for (int i = 0; i < USERS_MAX; i++) {
        struct card_uid u;
        if (!used(&db->rec[i]))
            continue;
        record_uid(&db->rec[i], &u);
        if (card_uid_equal(&u, uid))
            return i;
    }
    return -1;
}

int userdb_get(const struct userdb *db, int slot, struct user *u)
{
    if (slot < 0 || slot >= USERS_MAX || !used(&db->rec[slot]))
        return USERDB_INVALID;
    const struct user_record *r = &db->rec[slot];
    memset(u, 0, sizeof *u);
    record_uid(r, &u->uid);
    memcpy(u->name, r->name, USER_NAME_MAX);
    u->name[USER_NAME_MAX] = '\0';
    u->flags = r->flags;
    u->from_hour = r->from_hour;
    u->to_hour = r->to_hour;
    return USERDB_OK;
}

unsigned userdb_count(const struct userdb *db)
{
    unsigned n = 0;
    for (int i = 0; i < USERS_MAX; i++)
        n += used(&db->rec[i]);
    return n;
}

static bool user_valid(const struct user *u)
{
    return (u->uid.len == 4 || u->uid.len == 7 || u->uid.len == 10)
        && u->name[0] != '\0' && strlen(u->name) <= USER_NAME_MAX
        && u->from_hour <= 24 && u->to_hour <= 24;
}

static void fill(struct user_record *r, const struct user *u)
{
    memset(r->name, 0, sizeof r->name);
    strncpy(r->name, u->name, USER_NAME_MAX);
    r->flags = u->flags;
    r->from_hour = u->from_hour;
    r->to_hour = u->to_hour;
}

int userdb_add(struct userdb *db, const struct user *u, const char *pin)
{
    if (!user_valid(u) || (pin && !pin_valid(pin)) || (!pin && (u->flags & USER_PIN)))
        return USERDB_INVALID;
    if (userdb_find(db, &u->uid) >= 0)
        return USERDB_EXISTS;

    for (int i = 0; i < USERS_MAX; i++) {
        struct user_record *r = &db->rec[i];
        if (used(r))
            continue;
        memset(r, 0, sizeof *r);
        r->used = RECORD_USED;
        r->uid_len = u->uid.len;
        memcpy(r->uid, u->uid.bytes, u->uid.len);
        fill(r, u);
        new_salt(db, r->salt);
        if (pin)
            hash_pin(r->salt, pin, r->pin_hash);
        if (save_record(db, i)) {
            memset(r, 0, sizeof *r);
            return USERDB_IO;
        }
        return i;
    }
    return USERDB_FULL;
}

static bool has_pin(const struct user_record *r)
{
    static const uint8_t none[sizeof r->pin_hash];
    return memcmp(r->pin_hash, none, sizeof none) != 0;
}

int userdb_update(struct userdb *db, int slot, const struct user *u)
{
    if (slot < 0 || slot >= USERS_MAX || !used(&db->rec[slot]))
        return USERDB_INVALID;
    struct user check = *u;
    record_uid(&db->rec[slot], &check.uid);
    /* Requiring a PIN the user doesn't have would lock them out */
    if (!user_valid(&check) || ((u->flags & USER_PIN) && !has_pin(&db->rec[slot])))
        return USERDB_INVALID;
    struct user_record before = db->rec[slot];
    fill(&db->rec[slot], u);
    if (save_record(db, slot)) {
        db->rec[slot] = before;
        return USERDB_IO;
    }
    return USERDB_OK;
}

int userdb_set_pin(struct userdb *db, int slot, const char *pin)
{
    if (slot < 0 || slot >= USERS_MAX || !used(&db->rec[slot]) || !pin_valid(pin))
        return USERDB_INVALID;
    struct user_record *r = &db->rec[slot];
    struct user_record before = *r;
    new_salt(db, r->salt);
    hash_pin(r->salt, pin, r->pin_hash);
    r->flags |= USER_PIN;
    if (save_record(db, slot)) {
        *r = before;
        return USERDB_IO;
    }
    return USERDB_OK;
}

int userdb_remove(struct userdb *db, int slot)
{
    if (slot < 0 || slot >= USERS_MAX || !used(&db->rec[slot]))
        return USERDB_INVALID;
    struct user_record before = db->rec[slot];
    memset(&db->rec[slot], 0, sizeof db->rec[slot]);
    if (save_record(db, slot)) {
        db->rec[slot] = before;
        return USERDB_IO;
    }
    return USERDB_OK;
}

bool userdb_check_pin(const struct userdb *db, int slot, const char *pin)
{
    if (slot < 0 || slot >= USERS_MAX || !used(&db->rec[slot]) || !pin_valid(pin))
        return false;
    uint8_t h[16];
    hash_pin(db->rec[slot].salt, pin, h);
    return same(h, db->rec[slot].pin_hash, sizeof h);
}

bool user_allowed_at(const struct user *u, unsigned hour)
{
    unsigned from = u->from_hour, to = u->to_hour;
    if (from == to || (from == 0 && to == 24))
        return true;
    if (from < to)
        return hour >= from && hour < to;
    return hour >= from || hour < to;       /* across midnight: 22 to 6 */
}

void userdb_settings(const struct userdb *db, struct settings *s)
{
    s->unlock_s = db->hdr.unlock_s;
    s->lockout_s = db->hdr.lockout_s;
    s->door_held_s = db->hdr.door_held_s;
    s->pin_timeout_s = db->hdr.pin_timeout_s;
    s->max_attempts = db->hdr.max_attempts;
}

int userdb_set_settings(struct userdb *db, const struct settings *s)
{
    if (s->unlock_s < 1 || s->unlock_s > 60 || s->lockout_s < 1 || s->lockout_s > 3600
        || s->door_held_s < 5 || s->door_held_s > 600 || s->pin_timeout_s < 5
        || s->pin_timeout_s > 60 || s->max_attempts < 1 || s->max_attempts > 10)
        return USERDB_INVALID;
    struct header_record before = db->hdr;
    db->hdr.unlock_s = s->unlock_s;
    db->hdr.lockout_s = s->lockout_s;
    db->hdr.door_held_s = s->door_held_s;
    db->hdr.pin_timeout_s = s->pin_timeout_s;
    db->hdr.max_attempts = s->max_attempts;
    if (save_header(db)) {
        db->hdr = before;
        return USERDB_IO;
    }
    return USERDB_OK;
}

bool userdb_check_admin(const struct userdb *db, const char *pin)
{
    if (!pin_valid(pin))
        return false;
    uint8_t h[16];
    hash_pin(db->hdr.admin_salt, pin, h);
    return same(h, db->hdr.admin_hash, sizeof h);
}

int userdb_set_admin(struct userdb *db, const char *pin)
{
    if (!pin_valid(pin))
        return USERDB_INVALID;
    struct header_record before = db->hdr;
    new_salt(db, db->hdr.admin_salt);
    hash_pin(db->hdr.admin_salt, pin, db->hdr.admin_hash);
    if (save_header(db)) {
        db->hdr = before;
        return USERDB_IO;
    }
    return USERDB_OK;
}

bool userdb_admin_is_default(const struct userdb *db)
{
    return userdb_check_admin(db, ADMIN_DEFAULT_PIN);
}

int userdb_wipe(struct userdb *db)
{
    for (int i = 0; i < USERS_MAX; i++) {
        if (!used(&db->rec[i]))
            continue;
        memset(&db->rec[i], 0, sizeof db->rec[i]);
        if (save_record(db, i))
            return USERDB_IO;
    }
    default_header(db);
    return save_header(db);
}

const char *userdb_strerror(int err)
{
    switch (err) {
    case USERDB_OK: return "ok";
    case USERDB_IO: return "storage error";
    case USERDB_FULL: return "no free slot";
    case USERDB_EXISTS: return "card already enrolled";
    case USERDB_INVALID: return "invalid";
    default: return "?";
    }
}
