/*
 * Timer PWM: the servo on TIM2_CH3 (50 Hz, 1 us steps) and the passive
 * buzzer on TIM3_CH2 (a square wave at the tone's frequency).
 */
#ifndef PWM_H
#define PWM_H

#include <stdint.h>

void pwm_servo_init(void);
/* The pulse width, 500 to 2500 us; 0 stops the pulses (the servo holds) */
void pwm_servo_us(uint32_t us);

void pwm_tone_init(void);
/* A 50% square wave; 0 is silence */
void pwm_tone(uint32_t hz);

#endif
