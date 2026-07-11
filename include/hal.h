/**
 * =============================================================================
 * éterOS - Hardware Abstraction Layer (HAL)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 * =============================================================================
 *
 * La HAL es la capa que hace a éterOS universal.
 * Define la interfaz que TODA arquitectura debe implementar.
 *
 * Cada plataforma provee su implementación en:
 *   kernel/arch/<arch>/hal_impl.c
 *
 * Arquitecturas soportadas (actuales y planificadas):
 *
 *   TIER 1 - Microcontroladores (éterOS-Micro)
 *   ─────────────────────────────────────────
 *   arm-cortex-m    ARM Cortex-M0/M3/M4/M7   (STM32, Pico, Arduino Due)
 *   avr             AVR 8-bit                 (Arduino Uno/Mega, ATtiny)
 *   riscv32         RISC-V 32-bit             (ESP32-C3, GD32V)
 *   xtensa          Xtensa LX6/LX7            (ESP32, ESP32-S3)
 *
 *   TIER 2 - Procesadores de Aplicación (éterOS-Core)
 *   ─────────────────────────────────────────────────
 *   aarch64         ARM Cortex-A53/A72/A76    (RPi, phones, tablets, satélites)
 *   riscv64         RISC-V 64-bit             (SiFive, StarFive)
 *
 *   TIER 3 - Escritorio / Servidor (éterOS-Full)
 *   ─────────────────────────────────────────────
 *   x86_64          Intel/AMD 64-bit          (PCs, servidores, data centers)
 *   x86             Intel/AMD 32-bit          (legacy)
 *
 * CÓMO PORTAR éterOS A UNA NUEVA ARQUITECTURA:
 *   1. Crear directorio kernel/arch/<tu_arch>/
 *   2. Implementar TODAS las funciones declaradas en este header
 *   3. Agregar tu arch al Makefile/build system
 *   4. ¡Compilar y correr!
 *
 *   Todo lo demás (shell, string, lógica del kernel) se reutiliza tal cual.
 *
 * =============================================================================
 */

#ifndef ETEROS_HAL_H
#define ETEROS_HAL_H

#include "types.h"

/* ========================================================================= */
/* Identificación de arquitectura (se define en compilación: -DARCH_xxx)      */
/* ========================================================================= */
#if defined(ARCH_X86_64)
    #define ETEROS_ARCH_NAME    "x86_64"
    #define ETEROS_TIER         3
    #define ETEROS_HAS_MMU      1
    #define ETEROS_HAS_FPU      1
    #define ETEROS_WORD_SIZE    64
#elif defined(ARCH_AARCH64)
    #define ETEROS_ARCH_NAME    "aarch64"
    #define ETEROS_TIER         2
    #define ETEROS_HAS_MMU      1
    #define ETEROS_HAS_FPU      1
    #define ETEROS_WORD_SIZE    64
#elif defined(ARCH_RISCV64)
    #define ETEROS_ARCH_NAME    "riscv64"
    #define ETEROS_TIER         2
    #define ETEROS_HAS_MMU      1
    #define ETEROS_HAS_FPU      1
    #define ETEROS_WORD_SIZE    64
#elif defined(ARCH_ARM_CORTEX_M)
    #define ETEROS_ARCH_NAME    "arm-cortex-m"
    #define ETEROS_TIER         1
    #define ETEROS_HAS_MMU      0
    #define ETEROS_HAS_FPU      0
    #define ETEROS_WORD_SIZE    32
#elif defined(ARCH_XTENSA)
    #define ETEROS_ARCH_NAME    "xtensa"
    #define ETEROS_TIER         1
    #define ETEROS_HAS_MMU      0
    #define ETEROS_HAS_FPU      0
    #define ETEROS_WORD_SIZE    32
#elif defined(ARCH_RISCV32)
    #define ETEROS_ARCH_NAME    "riscv32"
    #define ETEROS_TIER         1
    #define ETEROS_HAS_MMU      0
    #define ETEROS_HAS_FPU      0
    #define ETEROS_WORD_SIZE    32
#elif defined(ARCH_AVR)
    #define ETEROS_ARCH_NAME    "avr"
    #define ETEROS_TIER         1
    #define ETEROS_HAS_MMU      0
    #define ETEROS_HAS_FPU      0
    #define ETEROS_WORD_SIZE    8
#else
    /* Default: asumir x86_64 para compatibilidad actual */
    #define ARCH_X86_64
    #define ETEROS_ARCH_NAME    "x86_64"
    #define ETEROS_TIER         3
    #define ETEROS_HAS_MMU      1
    #define ETEROS_HAS_FPU      1
    #define ETEROS_WORD_SIZE    64
#endif

/* ========================================================================= */
/* Tier features (qué incluir según la capacidad del dispositivo)            */
/* ========================================================================= */
#define ETEROS_EDITION_MICRO    1   /* Shell + UART, sin display, sin MMU     */
#define ETEROS_EDITION_CORE     2   /* + Display, + memory management, + IPC  */
#define ETEROS_EDITION_FULL     3   /* + Filesystem, + networking, + GUI      */

/* ========================================================================= */
/* API de la HAL — Cada arquitectura DEBE implementar estas funciones        */
/* ========================================================================= */

/* ---- Inicialización ---- */

/**
 * Inicializa el hardware de la plataforma.
 * Llamado como primera cosa en kmain().
 */
void hal_init(void);

/* ---- Interrupciones ---- */

/**
 * Habilita las interrupciones globales.
 * x86: STI | ARM: CPSIE I | RISC-V: csrs mstatus, MIE
 */
void hal_interrupts_enable(void);

/**
 * Deshabilita las interrupciones globales.
 * x86: CLI | ARM: CPSID I | RISC-V: csrc mstatus, MIE
 */
void hal_interrupts_disable(void);

/**
 * Instala un handler para un vector de interrupción.
 * @param vector  Número de interrupción/IRQ.
 * @param handler Puntero a la función handler.
 */
typedef void (*irq_handler_t)(void);
void hal_irq_install(uint8_t vector, irq_handler_t handler);

/* ---- CPU ---- */

/**
 * Detiene la CPU hasta la próxima interrupción.
 * x86: HLT | ARM: WFI | RISC-V: WFI
 */
void hal_cpu_halt(void);

/**
 * Habilita interrupciones y detiene la CPU de forma atómica.
 */
void hal_cpu_enable_interrupts_and_halt(void);

/**
 * Reinicia el sistema.
 * x86: triple fault | ARM: NVIC_SystemReset | RISC-V: write to reset reg
 */
void hal_cpu_reset(void);

/* ---- Consola (independiente del medio físico) ---- */

/**
 * Inicializa la consola de salida.
 * Puede ser VGA, UART, LCD, OLED, o cualquier display.
 */
void hal_console_init(void);

/**
 * Escribe un carácter a la consola.
 */
void hal_console_putchar(char c);

/**
 * Escribe una cadena a la consola.
 */
void hal_console_write(const char* str);

/**
 * Limpia la consola.
 */
void hal_console_clear(void);

/* ---- Input (independiente del medio físico) ---- */

/**
 * Inicializa el dispositivo de entrada.
 * Puede ser teclado PS/2, UART, GPIO buttons, touch, etc.
 */
void hal_input_init(void);

/**
 * Lee un carácter de la entrada. Bloquea hasta que haya input.
 */
char hal_input_getchar(void);

/**
 * Verifica si hay entrada disponible.
 */
bool hal_input_available(void);

/* ---- Timer ---- */

/**
 * Inicializa un timer del sistema con la frecuencia dada.
 * @param hz Frecuencia en hertz.
 */
void hal_timer_init(uint32_t hz);

/**
 * Retorna los ticks del sistema desde el arranque.
 */
uint64_t hal_timer_ticks(void);

/* ---- Memoria (solo Tier 2+) ---- */

#include "hal/mm.h"

/* ---- Debug / Diagnóstico ---- */

/**
 * Envía un carácter al puerto de debug (UART, JTAG, serial).
 */
void hal_debug_putchar(char c);

/**
 * Envía una cadena al puerto de debug.
 */
void hal_debug_write(const char* str);

#endif /* ETEROS_HAL_H */
