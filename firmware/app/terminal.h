/*
 * The task that owns the access rules (lib/access.c). Everything that
 * happens reaches it through its queue, so the rules run in one task and
 * need no locking of their own.
 */
#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdbool.h>
#include <stdint.h>
#include "access.h"
#include "card.h"

void terminal_start(bool door_open);

void terminal_card(const struct card_uid *uid);
void terminal_key(char key);
void terminal_door(bool open);
void terminal_exit(void);
void terminal_remote_unlock(void);
void terminal_alarm_off(void);
void terminal_settings_changed(void);
/* Puts the terminal in enrol mode and waits for a card: true with its UID,
 * false after 20 s or if the terminal is busy */
bool terminal_enroll(struct card_uid *uid);

enum access_state terminal_state(void);
enum alarm_kind terminal_alarm(void);

#endif
