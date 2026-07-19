/**
 * =============================================================================
 * éterOS - Kernel Main (Xtensa)
 * Copyright (c) 2025 Tudex Networks. All rights reserved.
 * =============================================================================
 *
 * Punto de entrada del kernel para la arquitectura Xtensa (ESP32).
 *
 * Responsabilidades:
 *   1. Inicializar la HAL (UART, Timer, Interrupciones)
 *   2. Mostrar banner de arranque
 *   3. Inicializar stubs de red (WiFi/BLE)
 *   4. Entrar en loop infinito (idle task)
 *
 * =============================================================================
 */

#include "../../../include/types.h"
#include "../../../include/hal.h"
#include "../../../include/string.h"

/* Prototipos para las funciones de red específicas de Xtensa */
/* Estas deberían ir en un header propio, pero por ahora las declaramos aquí */
void wifi_init_stub(void);
void ble_init_stub(void);
void mqtt_client_init_stub(void);

/**
 * kmain - Punto de entrada del kernel (Xtensa)
 *
 * Esta función es llamada desde el código de arranque (start.S / bootloader).
 */
void kmain(void) {
    /* 1. Inicializar Hardware Abstraction Layer */
    hal_init();

    /* 2. Inicializar consola (UART0) */
    hal_console_init();

    /* 3. Mostrar banner */
    hal_console_clear();
    hal_console_write("\n");
    hal_console_write("  eterOS (Xtensa Edition)\n");
    hal_console_write("  (c) 2025 Tudex Networks\n");
    hal_console_write("  -----------------------\n");

    /* 4. Habilitar interrupciones */
    hal_console_write("[INIT] Habilitando interrupciones...\n");
    hal_interrupts_enable();

    /* 5. Inicializar Timer */
    hal_console_write("[INIT] Configurando Timer...\n");
    hal_timer_init(100); /* 100 Hz */

    /* 6. Inicializar Subsistemas de Red (Stubs) */
    hal_console_write("[NET]  Inicializando stack WiFi...\n");
    wifi_init_stub();

    hal_console_write("[BLE]  Inicializando stack BLE...\n");
    ble_init_stub();

    hal_console_write("[MQTT] Preparando cliente MQTT...\n");
    mqtt_client_init_stub();

    /* 7. Loop principal */
    hal_console_write("[sys]  Sistema listo. Entrando en idle loop.\n");

    while (1) {
        /* Poner CPU en bajo consumo */
        hal_cpu_halt();
    }
}
