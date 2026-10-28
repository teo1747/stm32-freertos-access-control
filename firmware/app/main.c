/*
 * Access control terminal: an MFRC522 card reader and a keypad PIN in front
 * of a servo lock, users and an access log in an I2C EEPROM, the time from
 * a DS3231, an SSD1306 screen and an admin shell on the console.
 */
#include "board.h"
#include "console.h"
#include "fault.h"
#include "i2c.h"
#include "supervisor.h"
#include "system.h"
#include "FreeRTOS.h"
#include "task.h"

int main(void)
{
    const char *clock = system_clock_init();
    enum reset_cause cause = system_reset_cause();
    const struct fault_record *fault = fault_last();

    gpio_output(PIN_LED_HEARTBEAT, GPIO_PUSHPULL, 0);
    console_init();

    console_printf("\r\naccess-terminal %s: STM32F446RE at %lu MHz (%s), reset: %s\r\n",
                   FIRMWARE_VERSION, (unsigned long)(system_hclk() / 1000000u), clock,
                   system_reset_name(cause));
    if (fault) {
        char line[160];
        fault_describe(fault, line, sizeof line);
        console_printf("last fault: %s\r\n", line);
    }

    i2c_init();
    supervisor_start();

    vTaskStartScheduler();
    fault_raise(FAULT_OUT_OF_MEMORY, "scheduler", 0);
}
