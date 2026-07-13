/**
 * éterOS - PIT Timer Driver
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Programa el PIT 8254 Channel 0 en Rate Generator mode
 * para generar ~100 interrupciones por segundo (IRQ0).
 */

#include "../../../include/timer.h"
#include "../../../include/io.h"
#include "../../../include/serial.h"
#include "../../../include/task.h"
#include <hal.h>

/* Contador global de ticks (volatile — modificado desde ISR) */
static volatile uint64_t tick_count = 0;

void timer_init(void) {
    /* Calcular divisor para la frecuencia deseada */
    uint16_t divisor = (uint16_t)(PIT_FREQUENCY / TIMER_HZ);

    /*
     * Command byte: 0x36
     *   Channel 0 (bits 7-6: 00)
     *   Access: lobyte/hibyte (bits 5-4: 11)
     *   Mode 3: Square wave (bits 3-1: 011)
     *   Binary counting (bit 0: 0)
     */
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));         /* Low byte */
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));  /* High byte */

    serial_write_string("[eterOS] PIT configurado a ~100 Hz.\n");
}

void timer_tick(void) {
    tick_count++;
    /* Despertar tareas dormidas cuyo tiempo ha expirado */
    task_wake_expired(tick_count);
}

uint64_t timer_get_ticks(void) {
    return tick_count;
}

uint32_t timer_get_uptime_seconds(void) {
    return (uint32_t)(tick_count / TIMER_HZ);
}

uint32_t timer_get_uptime_minutes(void) {
    return (uint32_t)(tick_count / (TIMER_HZ * 60));
}

void timer_wait(uint32_t ms) {
    uint64_t start_ticks = tick_count;
    /* Usar cast a uint64_t para evitar desbordamiento en la multiplicación */
    uint64_t ticks_to_wait = ((uint64_t)ms * TIMER_HZ) / 1000;

    /* Esperar al menos 1 tick si se pidió tiempo y la resolución es >= 10ms */
    if (ms > 0 && ticks_to_wait == 0) {
        ticks_to_wait = 1;
    }

    while (tick_count < (start_ticks + ticks_to_wait)) {
        /* Poner la CPU en bajo consumo hasta la próxima interrupción (timer tick) */
        hal_cpu_enable_interrupts_and_halt();
    }
}

/* API de sueño no bloqueante (cede el CPU) */
void timer_sleep(uint32_t ms) {
    task_sleep((uint64_t)ms);
}
