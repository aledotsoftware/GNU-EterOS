/**
 * éterOS - Timer API (PIT 8254)
 * Copyright (c) 2025 Tudex Networks. All rights reserved.
 */

#ifndef ETEROS_TIMER_H
#define ETEROS_TIMER_H

#include "types.h"

/** Frecuencia del oscilador del PIT (Hz). */
#define PIT_FREQUENCY     1193182

/** Frecuencia deseada de ticks (Hz). Aprox 100 ticks/segundo. */
#define TIMER_HZ          1000

/** Puertos del PIT. */
#define PIT_CHANNEL0      0x40
#define PIT_COMMAND       0x43

/**
 * Inicializa el PIT Channel 0 a TIMER_HZ interrupciones por segundo.
 */
void timer_init(void);

/**
 * Llamado por el handler de IRQ0 — incrementa el tick counter.
 */
void timer_tick(void);

/**
 * Retorna el número total de ticks desde el boot.
 */
uint64_t timer_get_ticks(void);

/**
 * Retorna el uptime en segundos.
 */
uint32_t timer_get_uptime_seconds(void);

/**
 * Retorna minutos de uptime.
 */
uint32_t timer_get_uptime_minutes(void);

/**
 * Bloquea la ejecución durante el tiempo indicado en milisegundos.
 * Implementado mediante espera activa eficiente (hlt).
 */
void timer_wait(uint32_t ms);

/**
 * Duerme la tarea actual durante el tiempo indicado en milisegundos.
 * Cede el CPU a otras tareas (no bloqueante / yield).
 */
void timer_sleep(uint32_t ms);

#endif /* ETEROS_TIMER_H */
