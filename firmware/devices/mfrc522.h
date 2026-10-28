/*
 * MFRC522 13.56 MHz reader on SPI, with what an access terminal needs from
 * ISO 14443A: wake a card, read its UID through anticollision, select it,
 * halt it.
 */
#ifndef MFRC522_H
#define MFRC522_H

#include <stdint.h>
#include "card.h"

enum {
    MFRC522_OK = 0,
    MFRC522_NO_CARD = -1,           /* nothing answered */
    MFRC522_ERROR = -2,             /* a collision, a CRC or parity error */
    MFRC522_UNSUPPORTED = -3,       /* a double or triple size UID */
    MFRC522_NOT_FOUND = -4,         /* the chip itself doesn't answer */
};

/* Resets the chip and turns the antenna on; returns VersionReg (0x91 or
 * 0x92 for an MFRC522) or MFRC522_NOT_FOUND */
int mfrc522_init(void);
/* WUPA, anticollision, SELECT, HLTA: a card in the field, halted or not */
int mfrc522_read_uid(struct card_uid *uid);

#endif
