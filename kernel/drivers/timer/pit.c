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
