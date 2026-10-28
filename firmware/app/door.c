#include "door.h"
#include "board.h"
#include "pwm.h"
#include "FreeRTOS.h"
#include "timers.h"

/* The bolt: 0 degrees locked, 90 open (1 ms and 1.5 ms pulses) */
#define SERVO_LOCKED_US     1000u
#define SERVO_OPEN_US       1500u

struct note {
    uint16_t hz;            /* 0: a rest */
    uint16_t ms;
};

static const struct note key_tune[] = { { 2000, 30 }, { 0, 0 } };
static const struct note ok_tune[] = { { 1568, 90 }, { 0, 30 }, { 2093, 140 }, { 0, 0 } };
static const struct note denied_tune[] = { { 330, 180 }, { 0, 60 }, { 262, 320 }, { 0, 0 } };
static const struct note warn_tune[] = { { 2637, 60 }, { 0, 0 } };
static const struct note alarm_tune[] = { { 2800, 250 }, { 1900, 250 }, { 0, 0 } };

static TimerHandle_t timer;
static const struct note *tune;
static const struct note *step;
static bool repeat;

static void next_note(TimerHandle_t t)
{
    if (step && step->ms == 0 && repeat)
        step = tune;
    if (!step || step->ms == 0) {
        pwm_tone(0);
        step = NULL;
        return;
    }
    pwm_tone(step->hz);
    xTimerChangePeriod(timer, pdMS_TO_TICKS(step->ms), 0);
    step++;
}

void door_init(void)
{
    pwm_servo_init();
    pwm_servo_us(SERVO_LOCKED_US);
    pwm_tone_init();
    gpio_output(PIN_LED_GREEN, GPIO_PUSHPULL, 0);
    gpio_output(PIN_LED_RED, GPIO_PUSHPULL, 0);
    timer = xTimerCreate("sound", 1, pdFALSE, NULL, next_note);
    configASSERT(timer);
}

void door_lock(bool open)
{
    pwm_servo_us(open ? SERVO_OPEN_US : SERVO_LOCKED_US);
}

void door_leds(bool green, bool red)
{
    gpio_write(PIN_LED_GREEN, green);
    gpio_write(PIN_LED_RED, red);
}

void door_sound(enum sound s)
{
    static const struct note *const tunes[] = {
        [SOUND_OFF] = NULL,
        [SOUND_KEY] = key_tune,
        [SOUND_OK] = ok_tune,
        [SOUND_DENIED] = denied_tune,
        [SOUND_WARN] = warn_tune,
        [SOUND_ALARM] = alarm_tune,
    };
    /* The alarm isn't cut short by a key's click */
    if (repeat && s != SOUND_OFF && s != SOUND_ALARM && s != SOUND_OK)
        return;
    /* The timer task reads these: change them all at once */
    taskENTER_CRITICAL();
    tune = tunes[s];
    step = tune;
    repeat = s == SOUND_ALARM;
    taskEXIT_CRITICAL();
    if (tune) {
        /* The timer task plays the notes; the first one right away */
        xTimerChangePeriod(timer, 1, portMAX_DELAY);
    } else {
        xTimerStop(timer, portMAX_DELAY);
        pwm_tone(0);
    }
}
