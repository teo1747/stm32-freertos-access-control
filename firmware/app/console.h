/*
 * The console on the ST-LINK's virtual COM port: log lines from any task,
 * one at a time, and the admin shell's input.
 */
#ifndef CONSOLE_H
#define CONSOLE_H

#include <stddef.h>

void console_init(void);
void console_write(const char *s, size_t n);
void console_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* A log line: "[  12.345] tag: message\r\n" */
void console_log(const char *tag, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#endif
