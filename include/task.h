/**
 * éterOS - Task Scheduler (Round-Robin Preemptive)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Multitarea preemptiva con Round-Robin.
 * Cada tarea tiene su propio stack de kernel (4 KB).
 * El PIT timer (IRQ0, 100 Hz) fuerza context switches.
 */

#ifndef ETEROS_TASK_H
#define ETEROS_TASK_H

#include "types.h"

/* ========================================================================= */
/* Configuración                                                             */
/* ========================================================================= */
#define MAX_TASKS       32
#define TASK_STACK_SIZE  4096   /* 4 KB por tarea */
#define SCHEDULER_HZ     10    /* Switch cada 10 ticks (100ms a 100Hz PIT) */

/* ========================================================================= */
/* Estados de Tarea                                                          */
/* ========================================================================= */
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD
} task_state_t;

/* ========================================================================= */
/* Estructura de Tarea                                                       */
/* ========================================================================= */
typedef struct task {
    uint64_t       rsp;                     /* Stack pointer guardado */
    uint8_t*       stack_base;              /* Base del stack alocado */
    uint32_t       id;                      /* Task ID único */
    task_state_t   state;                   /* Estado actual */
    char           name[32];                /* Nombre descriptivo */
} task_t;

/* ========================================================================= */
/* API Pública                                                               */
/* ========================================================================= */

/**
 * Inicializa el scheduler. Crea la tarea "kernel" (tarea 0) 
 * que representa el hilo de ejecución actual.
 */
void scheduler_init(void);

/**
 * Crea una nueva tarea.
 * @param name Nombre de la tarea (para debug).
 * @param entry Punto de entrada (función void -> void).
 * @return ID de la tarea, o -1 si no hay espacio.
 */
int task_create(const char* name, void (*entry)(void));

/**
 * Llamada desde el timer ISR. Incrementa el tick counter del scheduler
 * y realiza un context switch si es necesario.
 */
void schedule(void);

/**
 * Cede voluntariamente el CPU a otra tarea (cooperative yield).
 */
void task_yield(void);

/**
 * Termina la tarea actual.
 */
void task_exit(void);

/**
 * Obtiene la tarea actual.
 */
task_t* task_get_current(void);

/**
 * Obtiene el número de tareas activas.
 */
int task_get_count(void);

/**
 * Termina una tarea específica por PID.
 * @param pid ID de la tarea a matar. (0 = kernel, no permitido)
 * @return 0 si éxito, -1 si error o no encontrada.
 */
int task_kill(uint32_t pid);

/* ========================================================================= */
/* Context Switch (definido en context_switch.asm)                           */
/* ========================================================================= */

/**
 * Cambia el contexto de una tarea a otra.
 * Guarda registros callee-saved + RSP de la tarea actual.
 * Restaura registros callee-saved + RSP de la tarea nueva.
 *
 * @param old_rsp Puntero donde guardar el RSP actual.
 * @param new_rsp RSP de la nueva tarea a restaurar.
 */
extern void context_switch(uint64_t* old_rsp, uint64_t new_rsp);

#endif /* ETEROS_TASK_H */
