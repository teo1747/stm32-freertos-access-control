/*
 * The fault record: what stopped the firmware, kept in .noinit across the
 * reset that follows, and reported at the next boot.
 */
#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>

enum fault_kind {
    FAULT_NONE,
    FAULT_HARD,             /* HardFault, with the stacked registers */
    FAULT_ASSERT,           /* configASSERT or fault_assert */
    FAULT_STACK_OVERFLOW,   /* FreeRTOS's stack check, with the task */
    FAULT_OUT_OF_MEMORY,    /* pvPortMalloc failed */
    FAULT_WATCHDOG,         /* the supervisor saw a task stop checking in */
};

struct fault_record {
    uint32_t magic;
    uint32_t kind;
    uint32_t pc, lr, psr;
    uint32_t cfsr, hfsr, mmfar, bfar;
    uint32_t line;
    char where[24];         /* a file or a task's name */
    uint32_t count;         /* faults since power-on */
};

/* The record of the fault before this reset, or 0 */
const struct fault_record *fault_last(void);
const char *fault_kind_name(uint32_t kind);
void fault_clear(void);
/* One line: "hard fault in rfid at pc 0x08001234 (CFSR 0x00008200, BFAR ...)" */
void fault_describe(const struct fault_record *r, char *buf, unsigned size);

/* Record and reset */
void fault_assert(const char *file, int line) __attribute__((noreturn));
void fault_raise(enum fault_kind kind, const char *where, uint32_t detail) __attribute__((noreturn));

#endif
