/*
 * The user database and the access log on the 24LC256, shared by the
 * terminal task and the admin shell under one lock.
 */
#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include "accesslog.h"
#include "userdb.h"

/* From a task: the I2C bus must be up */
bool storage_init(void);
bool storage_ready(void);

/* Hold the lock around any use of these two */
void storage_take(void);
void storage_give(void);
struct userdb *storage_users(void);
struct accesslog *storage_log(void);

/* Appends to the log (taking the lock), and echoes it on the console */
void storage_log_event(enum log_event e, int slot, const struct card_uid *uid);

#endif
