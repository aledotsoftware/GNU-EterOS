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
#include <fs/vfs.h>

/* ========================================================================= */
/* Configuración                                                             */
/* ========================================================================= */
#define MAX_TASKS       32
#define TASK_STACK_SIZE  32768   /* 32 KB por tarea (GUI requires more) */
#define SCHEDULER_HZ     10    /* Switch cada 10 ticks (100ms a 100Hz PIT) */
#define MAX_FD           16    /* Máximo de descriptores de archivo por tarea */

/**
 * Inicializa el scheduler para un Application Processor (AP).
 * Crea una tarea idle específica para este núcleo.
 */
void task_init_ap(void);

typedef struct file_descriptor {
    struct fs_node* node;
    uint32_t offset;
    int flags;
} file_descriptor_t;

/* ========================================================================= */
/* Estados de Tarea                                                          */
/* ========================================================================= */
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

/* ========================================================================= */
/* Estructura de Tarea                                                       */
/* ========================================================================= */
typedef struct task {
    uint64_t       rsp;                     /* Stack pointer guardado */
    uint8_t*       stack_base;              /* Base del stack alocado */
    uint64_t       kernel_stack;            /* Tope del stack de kernel (para TSS RSP0) */
    uint64_t       cr3;                     /* Directorio de páginas (PML4) */
    uint32_t       id;                      /* Task ID único */
    task_state_t   state;                   /* Estado actual */
    uint64_t       wake_tick;               /* Tick para despertar si duerme */
    char           name[32];                /* Nombre descriptivo */

    /* POSIX Compatibility */
    file_descriptor_t fd_table[MAX_FD];     /* File Descriptor Table */
    uint32_t       signal_mask;             /* Mask of blocked signals */
    uint32_t       signal_pending;          /* Bitmap of pending signals */
    void           (*signal_handlers[32])(int); /* Signal Handlers */

    /* Linux Compatibility (TLS & Heap) */
    uint64_t       brk;                     /* Program break (end of data segment) */
    uint64_t       fs_base;                 /* FS Segment Base (TLS) */
    uint64_t       gs_base;                 /* GS Segment Base (TLS) */
    uint8_t        os_abi;                  /* ABI (0=SysV/Native, 3=Linux) */
    uint64_t       user_rsp;                /* User Stack Pointer (saved during syscall) */
    uint64_t       mmap_base;               /* Base address for mmap allocator */

    void           (*entry)(void);          /* Entry point for task_entry_wrapper */
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
 * Pone la tarea actual a dormir durante ms milisegundos.
 * Cede el CPU a otras tareas.
 */
void task_sleep(uint64_t ms);

/**
 * Verifica si hay tareas dormidas que deban despertar.
 * Llamado desde el timer interrupt.
 */
void task_wake_expired(uint64_t current_tick);

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
 * Obtiene una tarea por su índice en la tabla interna.
 * Útil para listar todas las tareas (ps).
 * @return Puntero a la tarea, o NULL si índice inválido.
 */
task_t* task_get_at(int index);

/**
 * Retorna el tamaño máximo de la tabla de tareas.
 */
int task_get_max(void);

/**
 * Termina una tarea específica por PID.
 * @param pid ID de la tarea a matar. (0 = kernel, no permitido)
 * @return 0 si éxito, -1 si error o no encontrada.
 */
int task_kill(uint32_t pid);

/**
 * Crea un duplicado del proceso actual (Fork).
 * @param regs Puntero a struct syscall_regs (estado actual).
 * @return PID del hijo al padre, 0 al hijo.
 */
int task_fork(void* regs);

/**
 * Busca una tarea por su ID único.
 * @return Puntero a la tarea o NULL si no existe.
 */
task_t* task_get_by_id(uint32_t id);

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
