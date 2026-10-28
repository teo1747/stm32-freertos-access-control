#include "iwdg.h"
#include "stm32f4xx.h"

#define KEY_RELOAD  0xAAAAu
#define KEY_ENABLE  0xCCCCu
#define KEY_ACCESS  0x5555u

void iwdg_start(uint32_t timeout_ms)
{
    /* LSI 32 kHz / 32 = 1 kHz: one count a millisecond, 4095 at most */
    if (timeout_ms > 4095u)
        timeout_ms = 4095u;
    IWDG->KR = KEY_ENABLE;
    IWDG->KR = KEY_ACCESS;
    IWDG->PR = 3;                   /* /32 */
    IWDG->RLR = timeout_ms;
    while (IWDG->SR) { }            /* PVU, RVU: the values reach the LSI domain */
    IWDG->KR = KEY_RELOAD;
}

void iwdg_feed(void)
{
    IWDG->KR = KEY_RELOAD;
}
