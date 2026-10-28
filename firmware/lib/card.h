/*
 * A card's UID, as ISO 14443-3 type A gives it: 4, 7 or 10 bytes.
 */
#ifndef CARD_H
#define CARD_H

#include <stdbool.h>
#include <stdint.h>

#define CARD_UID_MAX 10

struct card_uid {
    uint8_t len;
    uint8_t bytes[CARD_UID_MAX];
};

bool card_uid_equal(const struct card_uid *a, const struct card_uid *b);
/* "04:A1:B2:C3" */
void card_uid_format(const struct card_uid *uid, char *buf, unsigned size);
/* Hex bytes, with or without ':' or ' ' between them; true if 4, 7 or 10 */
bool card_uid_parse(const char *s, struct card_uid *uid);

#endif
