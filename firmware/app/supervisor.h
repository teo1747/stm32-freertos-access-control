/*
 * The first task: brings the devices up in order and starts the others.
 * Then it keeps the watchdog: the IWDG is fed only while every task checks
 * in. A task that stops is named in the fault record before the reset; if
 * the supervisor itself stops, the IWDG resets the chip.
 */
#ifndef SUPERVISOR_H
#define SUPERVISOR_H

enum checkin {
    CHECKIN_TERMINAL,
    CHECKIN_READER,
    CHECKIN_INPUTS,
    CHECKIN_UI,
    CHECKIN_COUNT
};

void supervisor_start(void);
/* From each watched task's loop, at least once a second */
void supervisor_checkin(enum checkin who);

#endif
