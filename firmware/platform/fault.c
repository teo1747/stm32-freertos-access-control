#include <string.h>
#include "fault.h"
#include "fmt.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"

#define FAULT_MAGIC 0xFA017ED5u

/* Not zeroed by the startup code: survives every reset but a power-on */
static struct fault_record record __attribute__((section(".noinit")));
static struct fault_record previous;

/* main calls this first, before anything can fault again */
const struct fault_record *fault_last(void)
{
    static int taken;

    if (!taken) {
        if (record.magic == FAULT_MAGIC && record.kind != FAULT_NONE)
            previous = record;
        else
            memset(&record, 0, sizeof record);   /* power-on: garbage */
        record.magic = FAULT_MAGIC;
        record.kind = FAULT_NONE;
        taken = 1;
    }
    return previous.kind != FAULT_NONE ? &previous : 0;
}

void fault_clear(void)
{
    previous.kind = FAULT_NONE;
    record.count = 0;
}

const char *fault_kind_name(uint32_t kind)
{
    switch (kind) {
    case FAULT_HARD: return "hard fault";
    case FAULT_ASSERT: return "assertion";
    case FAULT_STACK_OVERFLOW: return "stack overflow";
    case FAULT_OUT_OF_MEMORY: return "out of memory";
    case FAULT_WATCHDOG: return "task stalled";
    default: return "none";
    }
}

void fault_describe(const struct fault_record *r, char *buf, unsigned size)
{
    switch (r->kind) {
    case FAULT_HARD:
        fmt_snprintf(buf, size, "hard fault in %s at pc 0x%08lx, lr 0x%08lx (CFSR 0x%08lx, HFSR 0x%08lx, BFAR 0x%08lx)",
                     r->where, (unsigned long)r->pc, (unsigned long)r->lr, (unsigned long)r->cfsr,
                     (unsigned long)r->hfsr, (unsigned long)r->bfar);
        break;
    case FAULT_ASSERT:
        fmt_snprintf(buf, size, "assertion failed at %s:%lu", r->where, (unsigned long)r->line);
        break;
    case FAULT_OUT_OF_MEMORY:
        fmt_snprintf(buf, size, "out of memory in %s (%lu bytes free)", r->where, (unsigned long)r->line);
        break;
    default:
        fmt_snprintf(buf, size, "%s in %s", fault_kind_name(r->kind), r->where);
        break;
    }
}

/* Before the first task exists there is no current task to name */
static const char *current_task(void)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
        return "main";
    return pcTaskGetName(NULL);
}

static void copy_where(const char *where)
{
    /* Keep the end of a path: the file's name is what tells */
    size_t n = strlen(where);
    if (n >= sizeof record.where)
        where += n - (sizeof record.where - 1);
    strncpy(record.where, where, sizeof record.where - 1);
    record.where[sizeof record.where - 1] = '\0';
}

static void __attribute__((noreturn)) save_and_reset(void)
{
    record.magic = FAULT_MAGIC;
    record.count++;
    record.cfsr = SCB->CFSR;
    record.hfsr = SCB->HFSR;
    record.mmfar = SCB->MMFAR;
    record.bfar = SCB->BFAR;
    __DSB();
    NVIC_SystemReset();
}

void fault_raise(enum fault_kind kind, const char *where, uint32_t detail)
{
    __disable_irq();
    record.kind = kind;
    record.line = detail;
    record.pc = 0;
    record.lr = (uint32_t)__builtin_return_address(0);
    record.psr = 0;
    copy_where(where ? where : "");
    save_and_reset();
}

void fault_assert(const char *file, int line)
{
    fault_raise(FAULT_ASSERT, file, (uint32_t)line);
}

/* The registers the core stacked on exception entry: r0-r3, r12, lr, pc,
 * xPSR. Called from HardFault_Handler with the right stack's pointer. */
void fault_hard(const uint32_t *frame) __attribute__((used, noreturn));
void fault_hard(const uint32_t *frame)
{
    record.kind = FAULT_HARD;
    record.lr = frame[5];
    record.pc = frame[6];
    record.psr = frame[7];
    record.line = 0;
    copy_where(current_task());
    save_and_reset();
}

void HardFault_Handler(void) __attribute__((naked));
void HardFault_Handler(void)
{
    /* EXC_RETURN's bit 2: the frame is on the process stack (a task) or on
     * the main stack (an interrupt, or before the scheduler) */
    __asm volatile(
        "tst lr, #4      \n"
        "ite eq          \n"
        "mrseq r0, msp   \n"
        "mrsne r0, psp   \n"
        "b fault_hard    \n");
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    fault_raise(FAULT_STACK_OVERFLOW, name, 0);
}

void vApplicationMallocFailedHook(void)
{
    fault_raise(FAULT_OUT_OF_MEMORY, current_task(), xPortGetFreeHeapSize());
}
