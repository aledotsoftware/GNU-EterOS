/**
 * =============================================================================
 * éterOS - STM32F103 Register Definitions (CMSIS-like)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 * =============================================================================
 *
 * Defines memory map and register structures for STM32F103 (Cortex-M3).
 * Follows CMSIS conventions for compatibility.
 *
 * Target: STM32F103C8T6 (Blue Pill)
 */

#ifndef STM32F10X_H
#define STM32F10X_H

#include <types.h>

/* ========================================================================= */
/* Memory Map                                                                */
/* ========================================================================= */

#define FLASH_BASE          0x08000000UL
#define SRAM_BASE           0x20000000UL
#define PERIPH_BASE         0x40000000UL

/* APB1 Peripherals (0x4000 0000 - 0x4000 77FF) */
#define TIM2_BASE           (PERIPH_BASE + 0x0000)
#define TIM3_BASE           (PERIPH_BASE + 0x0400)
#define USART2_BASE         (PERIPH_BASE + 0x4400)
#define USART3_BASE         (PERIPH_BASE + 0x4800)
#define I2C1_BASE           (PERIPH_BASE + 0x5400)
#define I2C2_BASE           (PERIPH_BASE + 0x5800)
#define CAN1_BASE           (PERIPH_BASE + 0x6400)
#define BKP_BASE            (PERIPH_BASE + 0x6C00)
#define PWR_BASE            (PERIPH_BASE + 0x7000)

/* APB2 Peripherals (0x4001 0000 - 0x4001 3FFF) */
#define APB2PERIPH_BASE     (PERIPH_BASE + 0x10000)
#define AFIO_BASE           (APB2PERIPH_BASE + 0x0000)
#define EXTI_BASE           (APB2PERIPH_BASE + 0x0400)
#define GPIOA_BASE          (APB2PERIPH_BASE + 0x0800)
#define GPIOB_BASE          (APB2PERIPH_BASE + 0x0C00)
#define GPIOC_BASE          (APB2PERIPH_BASE + 0x1000)
#define GPIOD_BASE          (APB2PERIPH_BASE + 0x1400)
#define GPIOE_BASE          (APB2PERIPH_BASE + 0x1800)
#define ADC1_BASE           (APB2PERIPH_BASE + 0x2400)
#define ADC2_BASE           (APB2PERIPH_BASE + 0x2800)
#define TIM1_BASE           (APB2PERIPH_BASE + 0x2C00)
#define SPI1_BASE           (APB2PERIPH_BASE + 0x3000)
#define USART1_BASE         (APB2PERIPH_BASE + 0x3800)

/* AHB Peripherals (0x4001 8000 - 0x4002 3FFF) */
#define AHBPERIPH_BASE      (PERIPH_BASE + 0x20000)
#define DMA1_BASE           (AHBPERIPH_BASE + 0x0000)
#define DMA2_BASE           (AHBPERIPH_BASE + 0x0400)
#define RCC_BASE            (AHBPERIPH_BASE + 0x1000)
#define FLASH_R_BASE        (AHBPERIPH_BASE + 0x2000) /* Flash Interface */
#define CRC_BASE            (AHBPERIPH_BASE + 0x3000)

/* Cortex-M3 Core Peripherals */
#define SCS_BASE            0xE000E000UL
#define SYSTICK_BASE        (SCS_BASE + 0x0010)
#define NVIC_BASE           (SCS_BASE + 0x0100)
#define SCB_BASE            (SCS_BASE + 0x0D00)

/* ========================================================================= */
/* Register Structures                                                       */
/* ========================================================================= */

#define __IO volatile

/* ---- RCC (Reset and Clock Control) ---- */
typedef struct {
    __IO uint32_t CR;
    __IO uint32_t CFGR;
    __IO uint32_t CIR;
    __IO uint32_t APB2RSTR;
    __IO uint32_t APB1RSTR;
    __IO uint32_t AHBENR;
    __IO uint32_t APB2ENR;
    __IO uint32_t APB1ENR;
    __IO uint32_t BDCR;
    __IO uint32_t CSR;
} RCC_TypeDef;

#define RCC                 ((RCC_TypeDef *) RCC_BASE)

/* RCC Bit Definitions */
#define RCC_CR_HSION        (1 << 0)
#define RCC_CR_HSIRDY       (1 << 1)
#define RCC_CR_HSEON        (1 << 16)
#define RCC_CR_HSERDY       (1 << 17)
#define RCC_CR_PLLON        (1 << 24)
#define RCC_CR_PLLRDY       (1 << 25)

#define RCC_CFGR_SW_HSE     (1 << 0)
#define RCC_CFGR_SWS_HSE    (1 << 2)
#define RCC_CFGR_HPRE_DIV1  (0)
#define RCC_CFGR_PPRE1_DIV2 (4 << 8)
#define RCC_CFGR_PPRE2_DIV1 (0 << 11)
#define RCC_CFGR_PLLSRC     (1 << 16)
#define RCC_CFGR_PLLXTPRE   (1 << 17)
#define RCC_CFGR_PLLMUL9    (7 << 18)

#define RCC_APB2ENR_IOPAEN  (1 << 2)
#define RCC_APB2ENR_IOPBEN  (1 << 3)
#define RCC_APB2ENR_IOPCEN  (1 << 4)
#define RCC_APB2ENR_USART1EN (1 << 14)

/* ---- GPIO (General Purpose I/O) ---- */
typedef struct {
    __IO uint32_t CRL;
    __IO uint32_t CRH;
    __IO uint32_t IDR;
    __IO uint32_t ODR;
    __IO uint32_t BSRR;
    __IO uint32_t BRR;
    __IO uint32_t LCKR;
} GPIO_TypeDef;

#define GPIOA               ((GPIO_TypeDef *) GPIOA_BASE)
#define GPIOB               ((GPIO_TypeDef *) GPIOB_BASE)
#define GPIOC               ((GPIO_TypeDef *) GPIOC_BASE)

/* GPIO Mode & Config */
#define GPIO_CR_MODE_INPUT      0x0
#define GPIO_CR_MODE_OUTPUT_10  0x1
#define GPIO_CR_MODE_OUTPUT_2   0x2
#define GPIO_CR_MODE_OUTPUT_50  0x3

#define GPIO_CR_CNF_ANALOG      (0x0 << 2)
#define GPIO_CR_CNF_INPUT_FLOAT (0x1 << 2)
#define GPIO_CR_CNF_INPUT_PU_PD (0x2 << 2)

#define GPIO_CR_CNF_GP_PP       (0x0 << 2)
#define GPIO_CR_CNF_GP_OD       (0x1 << 2)
#define GPIO_CR_CNF_ALT_PP      (0x2 << 2)
#define GPIO_CR_CNF_ALT_OD      (0x3 << 2)

/* ---- USART (Universal Synchronous Asynchronous Receiver Transmitter) ---- */
typedef struct {
    __IO uint32_t SR;
    __IO uint32_t DR;
    __IO uint32_t BRR;
    __IO uint32_t CR1;
    __IO uint32_t CR2;
    __IO uint32_t CR3;
    __IO uint32_t GTPR;
} USART_TypeDef;

#define USART1              ((USART_TypeDef *) USART1_BASE)

/* USART Bit Definitions */
#define USART_SR_RXNE       (1 << 5)
#define USART_SR_TC         (1 << 6)
#define USART_SR_TXE        (1 << 7)

#define USART_CR1_RE        (1 << 2)
#define USART_CR1_TE        (1 << 3)
#define USART_CR1_RXNEIE    (1 << 5)
#define USART_CR1_UE        (1 << 13)

/* ---- FLASH (Flash Interface) ---- */
typedef struct {
    __IO uint32_t ACR;
    __IO uint32_t KEYR;
    __IO uint32_t OPTKEYR;
    __IO uint32_t SR;
    __IO uint32_t CR;
    __IO uint32_t AR;
    __IO uint32_t OBR;
    __IO uint32_t WRPR;
} FLASH_TypeDef;

#define FLASH               ((FLASH_TypeDef *) FLASH_R_BASE)

#define FLASH_ACR_LATENCY_2 (2 << 0)
#define FLASH_ACR_PRFTBE    (1 << 4)

/* ---- SysTick (System Timer) ---- */
typedef struct {
    __IO uint32_t CTRL;
    __IO uint32_t LOAD;
    __IO uint32_t VAL;
    __IO uint32_t CALIB;
} SysTick_TypeDef;

#define SysTick             ((SysTick_TypeDef *) SYSTICK_BASE)

#define SysTick_CTRL_ENABLE (1 << 0)
#define SysTick_CTRL_TICKINT (1 << 1)
#define SysTick_CTRL_CLKSOURCE (1 << 2)

/* ---- NVIC (Nested Vectored Interrupt Controller) ---- */
typedef struct {
    __IO uint32_t ISER[8];                 /* Interrupt Set Enable Register */
    uint32_t RESERVED0[24];
    __IO uint32_t ICER[8];                 /* Interrupt Clear Enable Register */
    uint32_t RESERVED1[24];
    __IO uint32_t ISPR[8];                 /* Interrupt Set Pending Register */
    uint32_t RESERVED2[24];
    __IO uint32_t ICPR[8];                 /* Interrupt Clear Pending Register */
    uint32_t RESERVED3[24];
    __IO uint32_t IABR[8];                 /* Interrupt Active bit Register */
    uint32_t RESERVED4[56];
    __IO uint8_t  IP[240];                 /* Interrupt Priority Register (8 Bit wide) */
    uint32_t RESERVED5[644];
    __IO uint32_t STIR;                    /* Software Trigger Interrupt Register */
} NVIC_TypeDef;

#define NVIC                ((NVIC_TypeDef *) NVIC_BASE)

/* ---- SCB (System Control Block) ---- */
typedef struct {
    __IO uint32_t CPUID;
    __IO uint32_t ICSR;
    __IO uint32_t VTOR;
    __IO uint32_t AIRCR;
    __IO uint32_t SCR;
    __IO uint32_t CCR;
    __IO uint32_t SHP[12];
    __IO uint32_t SHCSR;
    __IO uint32_t CFSR;
    __IO uint32_t HFSR;
    __IO uint32_t DFSR;
    __IO uint32_t MMAR;
    __IO uint32_t BFAR;
    __IO uint32_t AFSR;
} SCB_TypeDef;

#define SCB                 ((SCB_TypeDef *) SCB_BASE)

#define SCB_AIRCR_VECTKEY   (0x05FA << 16)
#define SCB_AIRCR_SYSRESETREQ (1 << 2)

#endif /* STM32F10X_H */
