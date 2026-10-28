#include "pwm.h"
#include "board.h"
#include "system.h"

#define TICK_HZ 1000000u        /* both timers count microseconds */

void pwm_servo_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;
    gpio_alternate(PIN_SERVO, 1, GPIO_PUSHPULL, GPIO_NOPULL);

    TIM2->PSC = system_timclk1() / TICK_HZ - 1u;
    TIM2->ARR = 20000u - 1u;                                /* 20 ms, 50 Hz */
    TIM2->CCR3 = 0;
    /* PWM mode 1 with preload: a new width starts at the next period */
    TIM2->CCMR2 = (6u << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE;
    TIM2->CCER = TIM_CCER_CC3E;
    TIM2->CR1 = TIM_CR1_ARPE;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CR1 |= TIM_CR1_CEN;
}

void pwm_servo_us(uint32_t us)
{
    if (us && (us < 500u || us > 2500u))
        return;
    TIM2->CCR3 = us;
}

void pwm_tone_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    (void)RCC->APB1ENR;
    gpio_alternate(PIN_BUZZER, 2, GPIO_PUSHPULL, GPIO_NOPULL);

    TIM3->PSC = system_timclk1() / TICK_HZ - 1u;
    TIM3->ARR = 1000u - 1u;
    TIM3->CCR2 = 0;
    TIM3->CCMR1 = (6u << TIM_CCMR1_OC2M_Pos) | TIM_CCMR1_OC2PE;
    TIM3->CCER = TIM_CCER_CC2E;
    TIM3->CR1 = TIM_CR1_ARPE;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->CR1 |= TIM_CR1_CEN;
}

void pwm_tone(uint32_t hz)
{
    if (hz == 0 || hz > TICK_HZ / 4u) {
        TIM3->CCR2 = 0;
        return;
    }
    uint32_t period = TICK_HZ / hz;
    TIM3->ARR = period - 1u;
    TIM3->CCR2 = period / 2u;
}
