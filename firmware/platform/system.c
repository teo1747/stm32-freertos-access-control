/*
 * Clock tree, reset cause and cycle counter.
 *
 * The clock sequence is RM0390's (6.3 and 5.1.4): voltage scale 1, the PLL
 * at VCO 360 MHz, over-drive on before SYSCLK goes over 168 MHz, five flash
 * wait states, then the switch to the PLL.
 */
#include "system.h"
#include "FreeRTOSConfig.h"
#include "board.h"
#include "stm32f4xx.h"

uint32_t SystemCoreClock = 16000000u;      /* HSI out of reset */

#define TIMEOUT_LOOPS 200000u

static int wait_set(volatile uint32_t *reg, uint32_t mask)
{
    for (uint32_t n = 0; n < TIMEOUT_LOOPS; n++)
        if (*reg & mask)
            return 1;
    return 0;
}

const char *system_clock_init(void)
{
    const char *source = "HSE";

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_VOS;                  /* scale 1 */

    /* HSE in bypass: the ST-LINK drives OSC_IN with 8 MHz. A board whose
     * MCO isn't wired (solder bridges SB50/SB54) never gets HSERDY. */
    RCC->CR |= RCC_CR_HSEBYP | RCC_CR_HSEON;
    uint32_t pllm;
    uint32_t pllsrc;
    if (wait_set(&RCC->CR, RCC_CR_HSERDY)) {
        pllm = BOARD_HSE_HZ / 1000000u;     /* 1 MHz into the PLL */
        pllsrc = RCC_PLLCFGR_PLLSRC_HSE;
    } else {
        RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_HSEBYP);
        pllm = 16;
        pllsrc = RCC_PLLCFGR_PLLSRC_HSI;
        source = "HSI";
    }

    /* 1 MHz x 360 = 360 MHz VCO; /2 = 180 MHz SYSCLK; /8 = 45 MHz on Q */
    RCC->PLLCFGR = pllsrc
                 | (pllm << RCC_PLLCFGR_PLLM_Pos)
                 | (360u << RCC_PLLCFGR_PLLN_Pos)
                 | (0u << RCC_PLLCFGR_PLLP_Pos)     /* /2 */
                 | (8u << RCC_PLLCFGR_PLLQ_Pos)
                 | (2u << RCC_PLLCFGR_PLLR_Pos);
    RCC->CR |= RCC_CR_PLLON;
    wait_set(&RCC->CR, RCC_CR_PLLRDY);

    PWR->CR |= PWR_CR_ODEN;
    wait_set(&PWR->CSR, PWR_CSR_ODRDY);
    PWR->CR |= PWR_CR_ODSWEN;
    wait_set(&PWR->CSR, PWR_CSR_ODSWRDY);

    FLASH->ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    /* AHB 180 MHz, APB1 45 MHz, APB2 90 MHz */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_SW))
              | RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2
              | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }

    SystemCoreClock = 180000000u;

    /* The cycle counter: delays and the run-time statistics */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    return source;
}

uint32_t system_hclk(void)
{
    return SystemCoreClock;
}

static uint32_t apb_divider(uint32_t ppre)
{
    /* PPREx: 0xx is /1, then 100 /2, 101 /4, 110 /8, 111 /16 */
    return (ppre & 4u) ? 2u << (ppre & 3u) : 1u;
}

uint32_t system_pclk1(void)
{
    return SystemCoreClock / apb_divider((RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos);
}

uint32_t system_pclk2(void)
{
    return SystemCoreClock / apb_divider((RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos);
}

uint32_t system_timclk1(void)
{
    /* The timers run at twice PCLK1 when APB1 is divided (TIMPRE clear) */
    uint32_t pclk = system_pclk1();
    return pclk == SystemCoreClock ? pclk : 2u * pclk;
}

enum reset_cause system_reset_cause(void)
{
    static int read;
    static enum reset_cause cause;

    if (!read) {
        uint32_t csr = RCC->CSR;
        /* A power-on also sets PINRSTF: test the specific flags first */
        if (csr & RCC_CSR_IWDGRSTF)
            cause = RESET_WATCHDOG;
        else if (csr & RCC_CSR_WWDGRSTF)
            cause = RESET_WINDOW_WATCHDOG;
        else if (csr & RCC_CSR_LPWRRSTF)
            cause = RESET_LOW_POWER;
        else if (csr & RCC_CSR_SFTRSTF)
            cause = RESET_SOFTWARE;
        else if (csr & RCC_CSR_PORRSTF)
            cause = RESET_POWER_ON;
        else if (csr & RCC_CSR_BORRSTF)
            cause = RESET_BROWNOUT;
        else
            cause = RESET_PIN;
        RCC->CSR |= RCC_CSR_RMVF;
        read = 1;
    }
    return cause;
}

const char *system_reset_name(enum reset_cause cause)
{
    static const char *const names[] = {
        [RESET_POWER_ON] = "power-on",
        [RESET_PIN] = "reset pin",
        [RESET_SOFTWARE] = "software",
        [RESET_WATCHDOG] = "watchdog",
        [RESET_WINDOW_WATCHDOG] = "window watchdog",
        [RESET_LOW_POWER] = "low-power",
        [RESET_BROWNOUT] = "brown-out",
    };
    return names[cause];
}

uint32_t system_cycles(void)
{
    return DWT->CYCCNT;
}

void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000u);
    while (DWT->CYCCNT - start < cycles) { }
}

void system_reboot(void)
{
    NVIC_SystemReset();
}

uint32_t system_uid32(void)
{
    const volatile uint32_t *uid = (const volatile uint32_t *)UID_BASE;
    return uid[0] ^ uid[1] ^ uid[2];
}

/* FreeRTOS's run-time statistics count CPU cycles. They wrap every 23 s at
 * 180 MHz, so "top" measures over a window and uses differences. */
void stats_timer_init(void)
{
}

uint32_t stats_timer_read(void)
{
    return DWT->CYCCNT;
}
