#include "test.h"
#include "fake_store.h"
#include "userdb.h"

static struct user make(const char *uid, const char *name, uint8_t flags)
{
    struct user u = { .flags = flags, .from_hour = 0, .to_hour = 24 };
    CHECK(card_uid_parse(uid, &u.uid));
    snprintf(u.name, sizeof u.name, "%s", name);
    return u;
}

int main(void)
{
    static struct fake_store fs;
    static struct userdb db;
    struct store st;
    struct user u;
    struct settings cfg;

    /* A blank chip: formatted, default settings, the default admin PIN, no
     * user pages written (only the header) */
    fake_store_init(&fs, &st);
    CHECK_INT(userdb_open(&db, &st, fake_random), USERDB_OK);
    CHECK(db.formatted);
    CHECK_INT(fs.writes, 1);
    CHECK_INT(userdb_count(&db), 0);
    CHECK(userdb_admin_is_default(&db));
    CHECK(userdb_check_admin(&db, "1234"));
    CHECK(!userdb_check_admin(&db, "4321"));
    userdb_settings(&db, &cfg);
    CHECK_INT(cfg.unlock_s, 5);
    CHECK_INT(cfg.max_attempts, 3);

    /* Adding users */
    struct user ada = make("04:A1:B2:C3", "Ada Lovelace", USER_PIN | USER_ADMIN);
    struct user alan = make("04112233", "Alan Turing", 0);
    int s_ada = userdb_add(&db, &ada, "2468");
    int s_alan = userdb_add(&db, &alan, NULL);
    CHECK(s_ada >= 0 && s_alan >= 0 && s_ada != s_alan);
    CHECK_INT(userdb_add(&db, &ada, "1111"), USERDB_EXISTS);
    CHECK_INT(userdb_count(&db), 2);

    /* Invalid input */
    struct user bad = make("04112244", "", 0);
    CHECK_INT(userdb_add(&db, &bad, NULL), USERDB_INVALID);         /* no name */
    bad = make("04112244", "Bad PIN", USER_PIN);
    CHECK_INT(userdb_add(&db, &bad, "12"), USERDB_INVALID);         /* too short */
    CHECK_INT(userdb_add(&db, &bad, "12a4"), USERDB_INVALID);
    CHECK_INT(userdb_add(&db, &bad, "1234567"), USERDB_INVALID);    /* too long */
    CHECK_INT(userdb_add(&db, &bad, NULL), USERDB_INVALID);         /* PIN flag, no PIN */

    /* Lookups and PINs */
    struct card_uid card;
    CHECK(card_uid_parse("04 A1 B2 C3", &card));
    CHECK_INT(userdb_find(&db, &card), s_ada);
    CHECK_INT(userdb_get(&db, s_ada, &u), USERDB_OK);
    CHECK_STR(u.name, "Ada Lovelace");
    CHECK(userdb_check_pin(&db, s_ada, "2468"));
    CHECK(!userdb_check_pin(&db, s_ada, "2469"));
    CHECK(!userdb_check_pin(&db, s_alan, "2468"));
    CHECK(card_uid_parse("DEADBEEF", &card));
    CHECK_INT(userdb_find(&db, &card), -1);

    /* The same PIN hashes differently for two users: salted */
    struct user grace = make("04556677", "Grace Hopper", USER_PIN);
    int s_grace = userdb_add(&db, &grace, "2468");
    CHECK(memcmp(db.rec[s_grace].pin_hash, db.rec[s_ada].pin_hash, 16) != 0);

    /* Changes survive a reopen (a power cycle) */
    CHECK_INT(userdb_set_pin(&db, s_alan, "1357"), USERDB_OK);
    u = alan;
    u.flags = USER_PIN;                 /* flags are as given: keep the PIN */
    strcpy(u.name, "A. Turing");
    u.from_hour = 8;
    u.to_hour = 18;
    CHECK_INT(userdb_update(&db, s_alan, &u), USERDB_OK);
    struct user no_pin = make("04112233", "Grace", USER_PIN);
    CHECK_INT(userdb_update(&db, s_grace, &no_pin), USERDB_OK);    /* Grace has a PIN */
    no_pin.flags = 0;
    CHECK_INT(userdb_update(&db, s_grace, &no_pin), USERDB_OK);    /* PIN off */
    cfg.unlock_s = 7;
    CHECK_INT(userdb_set_settings(&db, &cfg), USERDB_OK);
    CHECK_INT(userdb_set_admin(&db, "9876"), USERDB_OK);
    CHECK_INT(userdb_remove(&db, s_grace), USERDB_OK);

    CHECK_INT(userdb_open(&db, &st, fake_random), USERDB_OK);
    CHECK(!db.formatted);
    CHECK_INT(userdb_count(&db), 2);
    CHECK_INT(userdb_get(&db, s_alan, &u), USERDB_OK);
    CHECK_STR(u.name, "A. Turing");
    CHECK(u.flags & USER_PIN);
    CHECK_INT(u.from_hour, 8);
    CHECK(userdb_check_pin(&db, s_alan, "1357"));
    CHECK(userdb_check_admin(&db, "9876"));
    CHECK(!userdb_admin_is_default(&db));
    userdb_settings(&db, &cfg);
    CHECK_INT(cfg.unlock_s, 7);
    CHECK_INT(userdb_get(&db, s_grace, &u), USERDB_INVALID);

    /* A damaged record is dropped at open: fails closed */
    fs.mem[STORE_USERS_ADDR + s_alan * 64 + 20] ^= 0x01;
    CHECK_INT(userdb_open(&db, &st, fake_random), USERDB_OK);
    CHECK_INT(db.dropped, 1);
    CHECK_INT(userdb_count(&db), 1);

    /* Asking for a PIN from a user who has none is refused */
    struct user nopin = make("04998877", "No PIN", 0);
    int s_nopin = userdb_add(&db, &nopin, NULL);
    nopin.flags = USER_PIN;
    CHECK_INT(userdb_update(&db, s_nopin, &nopin), USERDB_INVALID);
    userdb_remove(&db, s_nopin);

    /* A failed write leaves the cache as it was */
    fs.fail_writes = 1;
    CHECK_INT(userdb_remove(&db, s_ada), USERDB_IO);
    CHECK_INT(userdb_count(&db), 1);
    CHECK_INT(userdb_add(&db, &grace, "1111"), USERDB_IO);
    CHECK_INT(userdb_count(&db), 1);
    fs.fail_writes = 0;

    /* Full */
    for (int i = 0; i < USERS_MAX; i++) {
        char uid[9];
        snprintf(uid, sizeof uid, "1000%04X", i);
        struct user x = make(uid, "Filler", 0);
        userdb_add(&db, &x, NULL);
    }
    CHECK_INT(userdb_count(&db), USERS_MAX);
    struct user extra = make("20000000", "One too many", 0);
    CHECK_INT(userdb_add(&db, &extra, NULL), USERDB_FULL);

    /* Wipe: no users, defaults back */
    CHECK_INT(userdb_wipe(&db), USERDB_OK);
    CHECK_INT(userdb_open(&db, &st, fake_random), USERDB_OK);
    CHECK_INT(userdb_count(&db), 0);
    CHECK(userdb_admin_is_default(&db));

    /* A foreign layout (wrong magic) is formatted, its "users" cleared */
    fs.mem[0] ^= 0xFF;
    CHECK_INT(userdb_open(&db, &st, fake_random), USERDB_OK);
    CHECK(db.formatted);

    /* Hours */
    struct user h = { .from_hour = 8, .to_hour = 18 };
    CHECK(user_allowed_at(&h, 8) && user_allowed_at(&h, 17));
    CHECK(!user_allowed_at(&h, 18) && !user_allowed_at(&h, 7));
    h.from_hour = 22;
    h.to_hour = 6;                      /* the night shift */
    CHECK(user_allowed_at(&h, 23) && user_allowed_at(&h, 0) && user_allowed_at(&h, 5));
    CHECK(!user_allowed_at(&h, 6) && !user_allowed_at(&h, 12));

    /* Card UIDs */
    CHECK(card_uid_parse("04:A1:B2:C3:D4:E5:F6", &card));
    CHECK_INT(card.len, 7);
    char text[32];
    card_uid_format(&card, text, sizeof text);
    CHECK_STR(text, "04:A1:B2:C3:D4:E5:F6");
    CHECK(!card_uid_parse("04A1B2", &card));        /* 3 bytes */
    CHECK(!card_uid_parse("04A1B2C", &card));       /* odd digit */
    CHECK(!card_uid_parse("04A1B2CG", &card));

    return test_report("userdb");
}
