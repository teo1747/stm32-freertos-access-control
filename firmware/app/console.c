#include <stdarg.h>
#include "console.h"
#include "board.h"
#include "fmt.h"
#include "uart.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

static SemaphoreHandle_t lock;

void console_init(void)
{
    lock = xSemaphoreCreateMutex();
    configASSERT(lock);
    uart_init(&uart2, PIN_CONSOLE_TX, PIN_CONSOLE_RX, CONSOLE_BAUD, 512, 128);
}

static int running(void)
{
    return xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
}

void console_write(const char *s, size_t n)
{
    if (running())
        xSemaphoreTake(lock, portMAX_DELAY);
    uart_write(&uart2, s, n);
    if (running())
        xSemaphoreGive(lock);
}

void console_printf(const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    int n = fmt_vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    console_write(buf, n < (int)sizeof buf ? (size_t)n : sizeof buf - 1);
}

void console_log(const char *tag, const char *fmt, ...)
{
    char buf[160];
    uint32_t ms = running() ? xTaskGetTickCount() : 0;
    int n = fmt_snprintf(buf, sizeof buf, "[%5lu.%03lu] %s: ",
                         (unsigned long)(ms / 1000), (unsigned long)(ms % 1000), tag);
    va_list ap;
    va_start(ap, fmt);
    n += fmt_vsnprintf(buf + n, sizeof buf - (size_t)n, fmt, ap);
    va_end(ap);
    /* Room for the line end even when the message was cut */
    if (n > (int)sizeof buf - 3)
        n = sizeof buf - 3;
    buf[n++] = '\r';
    buf[n++] = '\n';
    console_write(buf, (size_t)n);
}
