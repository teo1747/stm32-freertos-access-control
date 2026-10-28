#include "inputs.h"
#include "board.h"
#include "console.h"
#include "keypad.h"
#include "mfrc522.h"
#include "spi.h"
#include "supervisor.h"
#include "terminal.h"
#include "FreeRTOS.h"
#include "task.h"

#define SCAN_MS         10
#define STABLE_SCANS    3       /* 30 ms the same: past a membrane's bounce */
#define READER_MS       100
#define CARD_GONE_POLLS 3       /* a card is gone after 3 silent polls */

static const char *reader_status = "not started";

/* A card is reported once when it arrives; holding it on the reader
 * doesn't repeat it, taking it away and back does */
static void reader_task(void *arg)
{
    struct card_uid present = { 0 };
    int misses = 0;

    spi_init();
    int version = mfrc522_init();
    if (version < 0) {
        reader_status = "MFRC522 not answering";
        console_log("reader", "%s", reader_status);
    } else {
        reader_status = version == 0x92 ? "MFRC522 v2.0" : version == 0x91 ? "MFRC522 v1.0" : "MFRC522 clone";
        console_log("reader", "%s (version 0x%02x)", reader_status, version);
    }

    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(READER_MS));
        supervisor_checkin(CHECKIN_READER);
        if (version < 0)
            continue;

        struct card_uid uid;
        int r = mfrc522_read_uid(&uid);
        if (r == MFRC522_OK) {
            misses = 0;
            if (!present.len || !card_uid_equal(&uid, &present)) {
                present = uid;
                terminal_card(&uid);
            }
        } else if (r == MFRC522_UNSUPPORTED) {
            if (!present.len)
                console_log("reader", "a card with a 7 or 10 byte UID: not supported");
            present.len = 1;    /* "something" is there: don't repeat */
            misses = 0;
        } else if (present.len && ++misses >= CARD_GONE_POLLS) {
            present.len = 0;
        }
    }
}

struct debounce {
    bool stable;
    bool last;
    uint8_t count;
};

/* True when the input has just settled at a new value */
static bool settle(struct debounce *d, bool raw)
{
    if (raw != d->last) {
        d->last = raw;
        d->count = 0;
        return false;
    }
    if (d->count < STABLE_SCANS && ++d->count == STABLE_SCANS && raw != d->stable) {
        d->stable = raw;
        return true;
    }
    return false;
}

static void inputs_task(void *arg)
{
    struct debounce exit_btn = { 0 }, door = { 0 };
    uint16_t keys_down = 0;
    uint8_t key_count = 0;

    /* Start from what the door is now, so boot isn't a "door opened" */
    door.stable = door.last = gpio_read(PIN_DOOR_CONTACT);
    door.count = STABLE_SCANS;

    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(SCAN_MS));

        /* The keypad: one key at a time, reported when pressed */
        uint16_t scan = keypad_scan();
        if (scan != keys_down) {
            keys_down = scan;
            key_count = 0;
        } else if (key_count < STABLE_SCANS && ++key_count == STABLE_SCANS && scan) {
            if ((scan & (scan - 1)) == 0) {             /* exactly one key */
                int key = __builtin_ctz(scan);
                terminal_key(keypad_char(key));
            }
        }

        /* Exit button: low when pressed */
        if (settle(&exit_btn, !gpio_read(PIN_EXIT_BUTTON)) && exit_btn.stable)
            terminal_exit();

        /* Door contact: closed pulls low; high (released) is open */
        if (settle(&door, gpio_read(PIN_DOOR_CONTACT)))
            terminal_door(door.stable);

        supervisor_checkin(CHECKIN_INPUTS);
    }
}

bool inputs_init(void)
{
    keypad_init();
    gpio_input(PIN_EXIT_BUTTON, GPIO_PULLUP);
    gpio_input(PIN_DOOR_CONTACT, GPIO_PULLUP);
    gpio_output(PIN_RFID_CS, GPIO_PUSHPULL, 1);
    /* The pull-ups against the wiring's capacitance */
    vTaskDelay(pdMS_TO_TICKS(2));
    return gpio_read(PIN_DOOR_CONTACT);
}

void inputs_start(void)
{
    xTaskCreate(reader_task, "reader", 384, NULL, 2, NULL);
    xTaskCreate(inputs_task, "inputs", 256, NULL, 4, NULL);
}

const char *inputs_reader_status(void)
{
    return reader_status;
}
