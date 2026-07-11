/**
 * =============================================================================
 * éterOS - Xtensa HAL Implementation (ESP32)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 * =============================================================================
 *
 * Implementación de la HAL para la arquitectura Xtensa LX6/LX7 (ESP32/S2/S3).
 *
 * Funcionalidades:
 *   - UART (Serial Console)
 *   - Timer (CCOUNT/CCOMPARE)
 *   - Interrupciones (INTENABLE)
 *   - Stubs de Red (WiFi/BLE/MQTT)
 *
 * Referencia:
 *   ESP32 Technical Reference Manual (Espressif Systems)
 *
 * =============================================================================
 */

#include "../../../include/hal.h"
#include "../../../include/types.h"

/* ========================================================================= */
/* Definiciones de Registros (ESP32)                                         */
/* ========================================================================= */

/* ---- UART0 (Consola por defecto) ---- */
#ifndef ESP32_UART0_BASE
#define ESP32_UART0_BASE        0x3FF40000
#endif
#define UART_FIFO(base)         ((volatile uint32_t*)(base + 0x00))
#define UART_INT_ENA(base)      ((volatile uint32_t*)(base + 0x0C))
#define UART_INT_CLR(base)      ((volatile uint32_t*)(base + 0x10))
#define UART_CLKDIV(base)       ((volatile uint32_t*)(base + 0x14))
#define UART_STATUS(base)       ((volatile uint32_t*)(base + 0x1C))
#define UART_CONF0(base)        ((volatile uint32_t*)(base + 0x20))
#define UART_CONF1(base)        ((volatile uint32_t*)(base + 0x24))

/* Status Bits */
#define UART_RXFIFO_CNT_MASK    0x000000FF
#define UART_RXFIFO_CNT_SHIFT   0
#define UART_TXFIFO_CNT_MASK    0x00FF0000
#define UART_TXFIFO_CNT_SHIFT   16
#define UART_TXFIFO_FULL_THR    126  /* FIFO size is 128 usually */

/* Interrupt Enable Bits */
#define UART_RXFIFO_FULL_INT_ENA      (1 << 0)
#define UART_RXFIFO_FULL_THRHD_SHIFT  0
#define UART_RXFIFO_FULL_THRHD_MASK   0x7F

/* ---- Timer & Interrupts (Xtensa Special Registers) ---- */
/* Se acceden via asm("rsr"/"wsr") */

/* ========================================================================= */
/* Variables Globales                                                        */
/* ========================================================================= */

static uint32_t system_ticks = 0;

/* ========================================================================= */
/* Inicialización y Control de Hardware                                      */
/* ========================================================================= */

void hal_init(void) {
    /*
     * En un entorno bare-metal real, aquí configuraríamos:
     * - Watchdog Timer (WDT) disable (para evitar reset por timeout)
     * - PLL / Clock setup
     * - Cache mapping
     *
     * Por ahora, confiamos en que el bootloader de 2da etapa
     * (o el entorno de simulación) nos deja en un estado saneado.
     */
}

void hal_cpu_halt(void) {
    /* Instrucción WAITI de Xtensa: espera interrupciones (bajo consumo) */
#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile ("waiti 0");
#endif
}

void hal_cpu_enable_interrupts_and_halt(void) {
    hal_interrupts_enable();
    hal_cpu_halt();
}

void hal_cpu_reset(void) {
    /*
     * Reset por software via RTC controller o Watchdog.
     * Stub simple: loop infinito.
     */
    while(1);
}

/* ========================================================================= */
/* Consola (UART0)                                                           */
/* ========================================================================= */

void hal_console_init(void) {
    /*
     * Configuración básica UART0:
     * - 115200 baudios (asumiendo reloj base 80MHz)
     * - 8 bits, No parity, 1 stop bit
     *
     * Nota: En muchos casos el bootloader ya inicializa UART0.
     * Aquí solo nos aseguramos de limpiar interrupciones pendientes.
     */

    /* Deshabilitar interrupciones UART por ahora */
    *UART_INT_ENA(ESP32_UART0_BASE) = 0;

    /* Limpiar estado */
    *UART_INT_CLR(ESP32_UART0_BASE) = 0xFFFFFFFF;
}

void hal_console_putchar(char c) {
    /* Esperar si el FIFO de transmisión está lleno */
    while (1) {
        uint32_t status = *UART_STATUS(ESP32_UART0_BASE);
        uint32_t tx_cnt = (status & UART_TXFIFO_CNT_MASK) >> UART_TXFIFO_CNT_SHIFT;

        if (tx_cnt < UART_TXFIFO_FULL_THR) {
            break;
        }
    }

    /* Escribir carácter al FIFO */
    *UART_FIFO(ESP32_UART0_BASE) = (uint32_t)c;
}

void hal_console_write(const char* str) {
    while (*str) {
        hal_console_putchar(*str++);
    }
}

void hal_console_clear(void) {
    /* Secuencia ANSI para limpiar pantalla */
    hal_console_write("\033[2J\033[H");
}

/* ---- Input (Polling) ---- */

void hal_input_init(void) {
    /*
     * Configure RX interrupt
     * 1. Set RX FIFO Full Threshold to 1 (generate interrupt on every char)
     * 2. Clear pending interrupts
     * 3. Enable RX interrupt
     */

    /* Set RX Full Threshold to 1 */
    uint32_t conf1 = *UART_CONF1(ESP32_UART0_BASE);
    conf1 &= ~UART_RXFIFO_FULL_THRHD_MASK;
    conf1 |= (1 << UART_RXFIFO_FULL_THRHD_SHIFT);
    *UART_CONF1(ESP32_UART0_BASE) = conf1;

    /* Clear any pending RX interrupt */
    *UART_INT_CLR(ESP32_UART0_BASE) = UART_RXFIFO_FULL_INT_ENA;

    /* Enable RX interrupt */
    *UART_INT_ENA(ESP32_UART0_BASE) |= UART_RXFIFO_FULL_INT_ENA;
}

char hal_input_getchar(void) {
    /* Esperar hasta que haya un byte disponible */
    while (!hal_input_available()) {
        /*
         * Busy wait. En un sistema multitarea real, esto debería
         * llamar a yield() o poner la tarea en sleep.
         */
    }

    /* Leer del FIFO (misma dirección base, lectura = pop RX) */
    return (char)(*UART_FIFO(ESP32_UART0_BASE) & 0xFF);
}

bool hal_input_available(void) {
    uint32_t status = *UART_STATUS(ESP32_UART0_BASE);
    uint32_t rx_cnt = (status & UART_RXFIFO_CNT_MASK) >> UART_RXFIFO_CNT_SHIFT;
    return rx_cnt > 0;
}

/* ========================================================================= */
/* Interrupciones (Xtensa)                                                   */
/* ========================================================================= */

void hal_interrupts_enable(void) {
    /* RSIL (Read and Set Interrupt Level) con nivel 0 habilita todo */
#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile ("rsil %0, 0" : : "r"(0));
#endif
}

void hal_interrupts_disable(void) {
    /* RSIL con nivel 15 (o max) deshabilita todo */
#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile ("rsil %0, 15" : : "r"(0));
#endif
}

void hal_irq_install(uint8_t vector, irq_handler_t handler) {
    /*
     * Xtensa maneja interrupciones por niveles (Levels 1-7).
     * Mapear un vector específico requiere configurar la tabla de vectores
     * (VECBASE) o usar el manejador de interrupciones del SDK si existiera.
     *
     * Stub: Registrar el handler en una tabla interna.
     */
    (void)vector;
    (void)handler;
}

/* ========================================================================= */
/* Timer (CCOUNT/CCOMPARE)                                                   */
/* ========================================================================= */

void hal_timer_init(uint32_t hz) {
    /*
     * Xtensa tiene un registro CCOUNT que cuenta ciclos de reloj.
     * CCOMPARE0 puede generar una interrupción cuando CCOUNT == CCOMPARE0.
     */
    uint32_t cycles_per_tick = 80000000 / hz; /* Asumiendo 80MHz CPU clock */
    uint32_t current_ccount;

#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile ("rsr %0, ccount" : "=r"(current_ccount));

    /* Programar primera interrupción */
    __asm__ volatile ("wsr %0, ccompare0" : : "r"(current_ccount + cycles_per_tick));
#else
    (void)cycles_per_tick;
    (void)current_ccount;
#endif

    /* Habilitar interrupción de Timer (bit 6 en INTENABLE, ejemplo) */
    /* __asm__ volatile ("wsr %0, intenable" ...); */
}

uint64_t hal_timer_ticks(void) {
    /*
     * Leer CCOUNT directamente.
     * Nota: CCOUNT es de 32 bits y da la vuelta cada ~53 segundos a 80MHz.
     * Para 64 bits reales, necesitamos un contador software en la ISR.
     */
    uint32_t ccount = 0;
#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile ("rsr %0, ccount" : "=r"(ccount));
#endif
    return (uint64_t)ccount; /* Retorna ciclos crudos por ahora */
}

/* ========================================================================= */
/* Memory Management (Stub para No-MMU)                                      */
/* ========================================================================= */

#if ETEROS_HAS_MMU
void hal_mmu_init(void) {
    /* Xtensa (ESP32) tiene una MMU limitada pero generalmente se usa flat map */
}
#endif

void hal_debug_putchar(char c) {
    hal_console_putchar(c);
}

void hal_debug_write(const char* str) {
    hal_console_write(str);
}

/* ========================================================================= */
/* Subsistemas de Red (WiFi / BLE / MQTT Stubs)                              */
/* ========================================================================= */

/* ---- Estructuras de Datos (Hooks) ---- */

typedef enum {
    WIFI_MODE_STA,
    WIFI_MODE_AP,
    WIFI_MODE_APSTA
} wifi_mode_t;

typedef struct {
    char ssid[32];
    char password[64];
    uint8_t channel;
    wifi_mode_t mode;
} wifi_config_t;

typedef enum {
    BLE_MODE_ADVERTISING,
    BLE_MODE_SCANNING,
    BLE_MODE_CONNECTED
} ble_mode_t;

/* ---- Funciones Stub ---- */

/* Declaro estas funciones globalmente para ser usadas en main.c */
void wifi_init_stub(void) {
    hal_console_write("[HAL-WIFI] Inicializando hardware de radio 2.4GHz...\n");
    /* Simulando delay y configuración de registros PHY */
    hal_console_write("[HAL-WIFI] MAC Address: AA:BB:CC:DD:EE:FF\n");
}

void wifi_connect_stub(const char* ssid, const char* pass) {
    hal_console_write("[HAL-WIFI] Conectando a AP: ");
    hal_console_write(ssid);
    hal_console_write("...\n");
    /* Simular éxito */
    hal_console_write("[HAL-WIFI] Conectado! IP: 192.168.1.105\n");
}

void ble_init_stub(void) {
    hal_console_write("[HAL-BLE]  Inicializando controlador Bluetooth LE...\n");
    hal_console_write("[HAL-BLE]  HCI Ready.\n");
}

void mqtt_client_init_stub(void) {
    hal_console_write("[HAL-MQTT] Hook para lwIP/MQTT registrado.\n");
}
