/*
 * Startup code for the STM32F446RE: the vector table, the reset handler that
 * sets up .data, .bss and the FPU before main, and a default handler for
 * every interrupt nobody handles.
 */
#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;

int main(void);
void Reset_Handler(void);
void default_handler(void);

/* The HardFault handler is in fault.c; it is weak here so a missing one
 * still links. */
void NMI_Handler(void) __attribute__((weak, alias("default_handler")));
void HardFault_Handler(void) __attribute__((weak, alias("default_handler")));
void MemManage_Handler(void) __attribute__((weak, alias("default_handler")));
void BusFault_Handler(void) __attribute__((weak, alias("default_handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("default_handler")));
void SVC_Handler(void) __attribute__((weak, alias("default_handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("default_handler")));
void PendSV_Handler(void) __attribute__((weak, alias("default_handler")));
void SysTick_Handler(void) __attribute__((weak, alias("default_handler")));
void WWDG_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void PVD_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TAMP_STAMP_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void RTC_WKUP_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void FLASH_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void RCC_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void EXTI0_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void EXTI1_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void EXTI2_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void EXTI3_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void EXTI4_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA1_Stream0_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA1_Stream1_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA1_Stream2_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA1_Stream3_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA1_Stream4_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA1_Stream5_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA1_Stream6_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void ADC_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void CAN1_TX_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void CAN1_RX0_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void CAN1_RX1_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void CAN1_SCE_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void EXTI9_5_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TIM1_BRK_TIM9_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TIM1_UP_TIM10_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TIM1_TRG_COM_TIM11_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TIM1_CC_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TIM2_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TIM3_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TIM4_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void I2C1_EV_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void I2C1_ER_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void I2C2_EV_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void I2C2_ER_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void SPI1_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void SPI2_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void USART1_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void USART2_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void USART3_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void EXTI15_10_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void RTC_Alarm_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void OTG_FS_WKUP_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TIM8_BRK_TIM12_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TIM8_UP_TIM13_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TIM8_TRG_COM_TIM14_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TIM8_CC_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA1_Stream7_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void FMC_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void SDIO_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TIM5_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void SPI3_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void UART4_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void UART5_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TIM6_DAC_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void TIM7_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA2_Stream0_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA2_Stream1_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA2_Stream2_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA2_Stream3_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA2_Stream4_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void CAN2_TX_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void CAN2_RX0_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void CAN2_RX1_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void CAN2_SCE_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void OTG_FS_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA2_Stream5_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA2_Stream6_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DMA2_Stream7_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void USART6_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void I2C3_EV_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void I2C3_ER_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void OTG_HS_EP1_OUT_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void OTG_HS_EP1_IN_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void OTG_HS_WKUP_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void OTG_HS_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void DCMI_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void FPU_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void SPI4_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void SAI1_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void SAI2_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void QUADSPI_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void CEC_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void SPDIF_RX_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void FMPI2C1_EV_IRQHandler(void) __attribute__((weak, alias("default_handler")));
void FMPI2C1_ER_IRQHandler(void) __attribute__((weak, alias("default_handler")));

typedef void (*vector_t)(void);

__attribute__((section(".isr_vector"), used))
const vector_t vector_table[] = {
    (vector_t)&_estack,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0,
    0,
    0,
    0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler,
    WWDG_IRQHandler,
    PVD_IRQHandler,
    TAMP_STAMP_IRQHandler,
    RTC_WKUP_IRQHandler,
    FLASH_IRQHandler,
    RCC_IRQHandler,
    EXTI0_IRQHandler,
    EXTI1_IRQHandler,
    EXTI2_IRQHandler,
    EXTI3_IRQHandler,
    EXTI4_IRQHandler,
    DMA1_Stream0_IRQHandler,
    DMA1_Stream1_IRQHandler,
    DMA1_Stream2_IRQHandler,
    DMA1_Stream3_IRQHandler,
    DMA1_Stream4_IRQHandler,
    DMA1_Stream5_IRQHandler,
    DMA1_Stream6_IRQHandler,
    ADC_IRQHandler,
    CAN1_TX_IRQHandler,
    CAN1_RX0_IRQHandler,
    CAN1_RX1_IRQHandler,
    CAN1_SCE_IRQHandler,
    EXTI9_5_IRQHandler,
    TIM1_BRK_TIM9_IRQHandler,
    TIM1_UP_TIM10_IRQHandler,
    TIM1_TRG_COM_TIM11_IRQHandler,
    TIM1_CC_IRQHandler,
    TIM2_IRQHandler,
    TIM3_IRQHandler,
    TIM4_IRQHandler,
    I2C1_EV_IRQHandler,
    I2C1_ER_IRQHandler,
    I2C2_EV_IRQHandler,
    I2C2_ER_IRQHandler,
    SPI1_IRQHandler,
    SPI2_IRQHandler,
    USART1_IRQHandler,
    USART2_IRQHandler,
    USART3_IRQHandler,
    EXTI15_10_IRQHandler,
    RTC_Alarm_IRQHandler,
    OTG_FS_WKUP_IRQHandler,
    TIM8_BRK_TIM12_IRQHandler,
    TIM8_UP_TIM13_IRQHandler,
    TIM8_TRG_COM_TIM14_IRQHandler,
    TIM8_CC_IRQHandler,
    DMA1_Stream7_IRQHandler,
    FMC_IRQHandler,
    SDIO_IRQHandler,
    TIM5_IRQHandler,
    SPI3_IRQHandler,
    UART4_IRQHandler,
    UART5_IRQHandler,
    TIM6_DAC_IRQHandler,
    TIM7_IRQHandler,
    DMA2_Stream0_IRQHandler,
    DMA2_Stream1_IRQHandler,
    DMA2_Stream2_IRQHandler,
    DMA2_Stream3_IRQHandler,
    DMA2_Stream4_IRQHandler,
    0,                          /* 61: reserved */
    0,                          /* 62: reserved */
    CAN2_TX_IRQHandler,
    CAN2_RX0_IRQHandler,
    CAN2_RX1_IRQHandler,
    CAN2_SCE_IRQHandler,
    OTG_FS_IRQHandler,
    DMA2_Stream5_IRQHandler,
    DMA2_Stream6_IRQHandler,
    DMA2_Stream7_IRQHandler,
    USART6_IRQHandler,
    I2C3_EV_IRQHandler,
    I2C3_ER_IRQHandler,
    OTG_HS_EP1_OUT_IRQHandler,
    OTG_HS_EP1_IN_IRQHandler,
    OTG_HS_WKUP_IRQHandler,
    OTG_HS_IRQHandler,
    DCMI_IRQHandler,
    0,                          /* 79: reserved */
    0,                          /* 80: reserved */
    FPU_IRQHandler,
    0,                          /* 82: reserved */
    0,                          /* 83: reserved */
    SPI4_IRQHandler,
    0,                          /* 85: reserved */
    0,                          /* 86: reserved */
    SAI1_IRQHandler,
    0,                          /* 88: reserved */
    0,                          /* 89: reserved */
    0,                          /* 90: reserved */
    SAI2_IRQHandler,
    QUADSPI_IRQHandler,
    CEC_IRQHandler,
    SPDIF_RX_IRQHandler,
    FMPI2C1_EV_IRQHandler,
    FMPI2C1_ER_IRQHandler,
};

void Reset_Handler(void)
{
    /* Initialised data from flash, then zeroed .bss */
    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata; )
        *dst++ = *src++;
    for (uint32_t *dst = &_sbss; dst < &_ebss; )
        *dst++ = 0;

    /* Full access to CP10 and CP11, the FPU, before any float instruction */
    *(volatile uint32_t *)0xE000ED88 |= (0xFu << 20);
    __asm volatile ("dsb\n isb");

    main();
    for (;;) { }
}

void default_handler(void)
{
    /* An interrupt without a handler: stop here, the watchdog resets */
    for (;;) { }
}
