/**
 * éterOS — ARM Cortex-M HAL Implementation
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Target: STM32F103 (Cortex-M3)
 * Reference: CMSIS & STM32F10x Reference Manual
 */

#include <hal.h>
#include "stm32f10x.h"

/* ========================================================================= */
/* Global State                                                              */
/* ========================================================================= */

static volatile uint64_t sys_ticks = 0;
static uint32_t system_core_clock = 8000000; /* Default HSI 8MHz */

/* ========================================================================= */
/* Internal Helper Functions                                                 */
/* ========================================================================= */

/**
 * Configure System Clock to 72MHz using HSE (8MHz Crystal) + PLL.
 * If HSE fails, falls back to HSI (8MHz).
 */
static void system_clock_config(void) {
    /* 1. Enable HSE */
    RCC->CR |= RCC_CR_HSEON;

    /* Wait for HSE Ready (simplistic timeout) */
    uint32_t timeout = 0x500; /* Arbitrary small timeout for "best effort" */
    while ((RCC->CR & RCC_CR_HSERDY) == 0 && timeout--);

    if (RCC->CR & RCC_CR_HSERDY) {
        /* HSE Ready - Configure PLL for 72MHz */
        /* Flash Latency = 2 wait states for 72MHz */
        FLASH->ACR |= FLASH_ACR_LATENCY_2;

        /* PLL: HSE * 9 = 8MHz * 9 = 72MHz */
        RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMUL9);
        RCC->CFGR |= (RCC_CFGR_PLLSRC | RCC_CFGR_PLLMUL9);

        /* Enable PLL */
        RCC->CR |= RCC_CR_PLLON;
        while ((RCC->CR & RCC_CR_PLLRDY) == 0);

        /* Select PLL as System Clock */
        RCC->CFGR &= ~(RCC_CFGR_SW_HSE); /* Clear SW bits */
        RCC->CFGR |= (1 << 1); /* SW = 10 (PLL) */

        /* Wait for switch */
        while ((RCC->CFGR & RCC_CFGR_SWS_HSE) != (1 << 3)); /* Wait for SWS = 10 */

        system_core_clock = 72000000;
    } else {
        /* HSE Failed, stick to HSI (8MHz) */
        system_core_clock = 8000000;
    }
}

/* ========================================================================= */
/* HAL Implementation                                                        */
/* ========================================================================= */

void hal_init(void) {
    /* 1. Configure System Clock */
    system_clock_config();

    /* 2. Configure SysTick for 100Hz (10ms) default or 1000Hz (1ms) */
    /* Let's assume 100Hz as per x86 default */
    hal_timer_init(100);

    /* 3. Initialize Console (UART1) */
    hal_console_init();

    /* 4. Enable Interrupts globally */
    hal_interrupts_enable();
}

void hal_interrupts_enable(void) {
    __asm__ volatile ("cpsie i" : : : "memory");
}

void hal_interrupts_disable(void) {
    __asm__ volatile ("cpsid i" : : : "memory");
}

void hal_irq_install(uint8_t vector, irq_handler_t handler) {
    /*
     * In Cortex-M, the Vector Table is usually in Flash at 0x00000000 or 0x08000000.
     * To change handlers at runtime, we need to move VTOR to SRAM.
     * For this HAL placeholder, we will just enable the IRQ in NVIC.
     * 'vector' here corresponds to the IRQ number (0 = WWDG, ... 37 = USART1, etc)
     * NOT the exception number (Reset=1, etc).
     */

    if (vector >= 240) return;

    /* Enable IRQ in ISER */
    NVIC->ISER[vector >> 5] = (1 << (vector & 0x1F));

    /* We cannot easily change the function pointer without RAM vector table remapping */
    (void)handler;
}

void hal_cpu_halt(void) {
    __asm__ volatile ("wfi");
}

void hal_cpu_enable_interrupts_and_halt(void) {
    hal_interrupts_enable();
    hal_cpu_halt();
}

void hal_cpu_reset(void) {
    /* System Reset Request */
    SCB->AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ;
    while(1);
}

/* ---- Console (UART1) ---- */

void hal_console_init(void) {
    /* 1. Enable Clocks: GPIOA (for TX/RX) + USART1 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    /* 2. Configure GPIOA Pin 9 (TX) as Alternate Function Push-Pull (50MHz) */
    /* CRL: CNF=10 (Alt PP), MODE=11 (50MHz) -> 0xB */
    /* Pin 9 is in CRH (bits 4-7) */
    GPIOA->CRH &= ~(0xF << 4);
    GPIOA->CRH |= (0xB << 4);

    /* 3. Configure GPIOA Pin 10 (RX) as Input Floating */
    /* CRL: CNF=01 (Float), MODE=00 (Input) -> 0x4 */
    /* Pin 10 is in CRH (bits 8-11) */
    GPIOA->CRH &= ~(0xF << 8);
    GPIOA->CRH |= (0x4 << 8);

    /* 4. Configure USART1 Baud Rate (115200) */
    /* BRR = PCLK2 / Baud. PCLK2 = SystemCoreClock (assuming no prescaler) */
    uint32_t baud = 115200;
    uint32_t divider = system_core_clock / baud;
    USART1->BRR = divider;

    /* 5. Enable UART, TX, RX */
    USART1->CR1 |= USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void hal_console_putchar(char c) {
    /* Wait for TXE (Transmit Data Register Empty) */
    while (!(USART1->SR & USART_SR_TXE));

    /* Write data */
    USART1->DR = (c & 0xFF);
}

void hal_console_write(const char* str) {
    while (*str) {
        hal_console_putchar(*str++);
    }
}

void hal_console_clear(void) {
    /* VT100 Clear Screen command */
    hal_console_write("\033[2J\033[H");
}

/* ---- Input ---- */

void hal_input_init(void) {
    /* UART1 RX already enabled in console_init */
}

char hal_input_getchar(void) {
    /* Wait for RXNE (Read Data Register Not Empty) */
    while (!(USART1->SR & USART_SR_RXNE));

    return (char)(USART1->DR & 0xFF);
}

bool hal_input_available(void) {
    return (USART1->SR & USART_SR_RXNE) ? true : false;
}

/* ---- Timer (SysTick) ---- */

void hal_timer_init(uint32_t hz) {
    if (hz == 0) hz = 100;

    /* Reload Value = Clock / Hz - 1 */
    uint32_t reload = (system_core_clock / hz) - 1;

    /* SysTick uses 24-bit counter */
    if (reload > 0x00FFFFFF) reload = 0x00FFFFFF;

    SysTick->LOAD = reload;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE | SysTick_CTRL_TICKINT | SysTick_CTRL_ENABLE;
}

uint64_t hal_timer_ticks(void) {
    return sys_ticks;
}

/* ---- SysTick Handler ---- */
/* This function matches the CMSIS/Startup standard name */
void SysTick_Handler(void) {
    sys_ticks++;
}

/* ---- Debug ---- */

void hal_debug_putchar(char c) {
    /* Send to same console for now */
    hal_console_putchar(c);
}

void hal_debug_write(const char* str) {
    hal_console_write(str);
}

#if ETEROS_HAS_MMU
void hal_mmu_init(void) {
    /* Cortex-M3 (STM32F103) has MPU, not MMU. */
    /* We could config MPU here, but for now do nothing as Tier 1 doesn't require MMU */
}
#endif
