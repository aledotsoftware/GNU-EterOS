/**
 * éterOS - Task Scheduler (Round-Robin Preemptive)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Scheduler preemptivo basado en timer (PIT IRQ0).
 * Cada SCHEDULER_HZ ticks, se selecciona la siguiente tarea READY.
 *
 * Diseño:
 *   - Array fijo de MAX_TASKS tareas
 *   - Tarea 0 = kernel/shell (la tarea que corría antes de init)
 *   - Cada tarea tiene su propio stack de 4 KB
 *   - Context switch guarda/restaura registros callee-saved + RSP
 */

#include "../include/task.h"
#include "../include/mm.h"
#include "../include/string.h"
#include "../include/serial.h"
#include "../include/vga.h"
#include "../include/timer.h"
#include "../include/gdt.h"
#include "../include/cpu.h"

/* ========================================================================= */
/* Estado Global del Scheduler                                               */
/* ========================================================================= */

static task_t   tasks[MAX_TASKS] __attribute__((aligned(16)));
static int      current_task  = 0;    /* Índice de la tarea actual */
static int      task_count    = 0;    /* Número total de tareas */
static uint32_t next_id       = 0;    /* Generador de IDs */
static uint32_t sched_ticks   = 0;    /* Contador para decidir cuándo switchear */
static bool     scheduler_active = false; /* El scheduler está inicializado? */

/* Variable global para el stack del kernel (usada por syscall_entry) */
uint64_t kernel_stack_top = 0;

/* ========================================================================= */
/* Task Entry Wrapper                                                        */
/* ========================================================================= */

/**
 * Wrapper que envuelve la función de entrada de cada tarea.
 * Cuando la función retorna, la tarea se marca como DEAD.
 * 
 * NOTA: Este wrapper se ejecuta con interrupciones deshabilitadas
 * la primera vez (vino de context_switch → ret). Debemos habilitarlas.
 */
typedef void (*task_func_t)(void);

/* Almacenamos el entry point en el stack de la tarea */
static void task_entry_wrapper(void) {
    /* Habilitar interrupciones (estamos en una tarea nueva, el context_switch
       no las habilita automáticamente como haría iretq) */
    __asm__ volatile("sti");

    /* Obtener el entry point de la tarea actual.
     * Lo guardamos en R15 del stack inicial de la tarea.
     * R15 fue restaurado por context_switch. */
    task_func_t entry;
    __asm__ volatile("mov %%r15, %0" : "=r"(entry));

    /* Ejecutar la función de la tarea */
    if (entry) {
        entry();
    }

    /* Si la función retorna, terminamos la tarea */
    task_exit();
}

/* ========================================================================= */
/* API del Scheduler                                                         */
/* ========================================================================= */

void scheduler_init(void) {
    memset(tasks, 0, sizeof(tasks));

    /* Tarea 0: Representa el hilo de ejecución actual (kernel/shell) */
    tasks[0].id = next_id++;
    tasks[0].state = TASK_RUNNING;
    tasks[0].stack_base = NULL;  /* El kernel ya tiene su stack */
    tasks[0].rsp = 0;            /* Se llenará en el primer context_switch */

    /* Inicializar CR3 del kernel */
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    tasks[0].cr3 = cr3;
    tasks[0].kernel_stack = 0;   /* No necesario para tarea kernel pura */

    strlcpy(tasks[0].name, "kernel", sizeof(tasks[0].name));

    /* POSIX Init for Kernel Task */
    memset(tasks[0].fd_table, 0, sizeof(tasks[0].fd_table));
    tasks[0].signal_mask = 0;
    tasks[0].signal_pending = 0;
    memset(tasks[0].signal_handlers, 0, sizeof(tasks[0].signal_handlers));

    task_count = 1;
    current_task = 0;
    scheduler_active = true;

    /* Configurar stack inicial para Task 0 (Boot Stack en 0x90000) */
    kernel_stack_top = 0x90000;
    tss_set_rsp0(kernel_stack_top);

    serial_write_string("[SCHED] Scheduler Round-Robin inicializado\n");
}

int task_create(const char* name, void (*entry)(void)) {
    if (task_count >= MAX_TASKS) {
        serial_write_string("[SCHED] Error: Maximo de tareas alcanzado\n");
        return -1;
    }

    /* Encontrar un slot libre */
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD || (i >= task_count && tasks[i].state == 0)) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;

    /* Alocar stack para la nueva tarea */
    uint8_t* stack = (uint8_t*)kmalloc(TASK_STACK_SIZE);
    if (!stack) {
        serial_write_string("[SCHED] Error: No hay memoria para el stack\n");
        return -1;
    }
    memset(stack, 0, TASK_STACK_SIZE);

    /* Configurar la tarea */
    tasks[slot].id = next_id++;
    tasks[slot].state = TASK_READY;
    tasks[slot].stack_base = stack;

    /* Configurar stack de kernel y CR3 */
    tasks[slot].kernel_stack = (uint64_t)(stack + TASK_STACK_SIZE);

    /* Heredar CR3 del kernel (por ahora, luego cada proceso tendrá su PML4) */
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    tasks[slot].cr3 = cr3;

    strlcpy(tasks[slot].name, name, sizeof(tasks[slot].name));

    /* POSIX Init for New Task */
    memset(tasks[slot].fd_table, 0, sizeof(tasks[slot].fd_table));
    tasks[slot].signal_mask = 0;
    tasks[slot].signal_pending = 0;
    memset(tasks[slot].signal_handlers, 0, sizeof(tasks[slot].signal_handlers));

    /*
     * Preparar el stack para que context_switch pueda "entrar" por primera vez.
     * 
     * Cuando context_switch cargue este RSP, hará:
     *   pop r15  (→ entry function pointer, usado por task_entry_wrapper)
     *   pop r14  (→ 0)
     *   pop r13  (→ 0)
     *   pop r12  (→ 0)
     *   pop rbx  (→ 0)
     *   pop rbp  (→ 0)
     *   ret      (→ task_entry_wrapper)
     *
     * Luego task_entry_wrapper lee R15 para obtener el entry point.
     */
    uint64_t* sp = (uint64_t*)(stack + TASK_STACK_SIZE);

    /* ret address → task_entry_wrapper */
    *(--sp) = (uint64_t)task_entry_wrapper;

    /* Callee-saved registers (en orden inverso al pop en context_switch) */
    *(--sp) = 0;                     /* rbp */
    *(--sp) = 0;                     /* rbx */
    *(--sp) = 0;                     /* r12 */
    *(--sp) = 0;                     /* r13 */
    *(--sp) = 0;                     /* r14 */
    *(--sp) = (uint64_t)entry;       /* r15 = entry point (leído por wrapper) */

    tasks[slot].rsp = (uint64_t)sp;

    if (slot >= task_count) {
        task_count = slot + 1;
    }

    serial_write_string("[SCHED] Tarea creada: ");
    serial_write_string(name);
    serial_write_string("\n");

    return (int)tasks[slot].id;
}

/**
 * Encuentra la siguiente tarea READY usando Round-Robin.
 * @return Índice de la siguiente tarea, o current_task si no hay otra.
 */
static int find_next_task(void) {
    int next = current_task;
    for (int i = 0; i < task_count; i++) {
        next = (next + 1) % task_count;
        if (tasks[next].state == TASK_READY || tasks[next].state == TASK_RUNNING) {
            return next;
        }
    }

    /* Si la tarea actual también está durmiendo/bloqueada, no podemos retornarla */
    if (tasks[current_task].state == TASK_READY || tasks[current_task].state == TASK_RUNNING) {
        return current_task;
    }

    return -1; /* No hay tareas ejecutables */
}

void schedule(void) {
    if (!scheduler_active) return;
    if (task_count <= 1) return; /* Unica tarea no necesita switch */

    /* Solo switchear cada SCHEDULER_HZ ticks */
    sched_ticks++;
    if (sched_ticks < SCHEDULER_HZ) return;
    sched_ticks = 0;

    int next = find_next_task();

    if (next == -1 || next == current_task) return;

    /* Cambiar estado */
    int old = current_task;
    
    if (tasks[old].state == TASK_RUNNING) {
        tasks[old].state = TASK_READY;
    }
    /* Si era TASK_SLEEPING, se queda en SLEEPING */
    
    tasks[next].state = TASK_RUNNING;
    current_task = next;

    /* Actualizar TSS RSP0 y Per-CPU Kernel Stack para Syscalls */
    if (tasks[next].kernel_stack != 0) {
        tss_set_rsp0(tasks[next].kernel_stack);
        per_cpu_data.kernel_stack = tasks[next].kernel_stack;
    }

    /* Context switch: guardar RSP actual en tasks[old], cargar de tasks[next] */
    context_switch(&tasks[old].rsp, tasks[next].rsp);
}

void task_yield(void) {
    if (!scheduler_active) return;
    
    /* Forzar un switch inmediato */
    sched_ticks = SCHEDULER_HZ;
    schedule();
}

void task_sleep(uint64_t ms) {
    if (!scheduler_active) {
        /* Si no hay scheduler, usar busy-wait */
        timer_wait((uint32_t)ms);
        return;
    }

    /* Convertir ms a ticks */
    uint64_t ticks = ((uint64_t)ms * TIMER_HZ) / 1000;
    if (ms > 0 && ticks == 0) ticks = 1; /* Minimo 1 tick */

    task_t* current = task_get_current();
    current->wake_tick = timer_get_ticks() + ticks;
    current->state = TASK_SLEEPING;

    /* Ceder CPU */
    task_yield();

    /* Si somos la única tarea o el scheduler no encontró a nadie más,
       volveremos aquí inmediatamente pero seguiremos en estado SLEEPING.
       Debemos esperar (wait for interrupt) hasta que el timer nos despierte. */
    while (current->state == TASK_SLEEPING) {
        __asm__ volatile("hlt");
    }

    /* Restaurar estado a RUNNING si nos despertamos */
    current->state = TASK_RUNNING;
}

void task_wake_expired(uint64_t current_tick) {
    if (!scheduler_active) return;

    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state == TASK_SLEEPING) {
            if (current_tick >= tasks[i].wake_tick) {
                tasks[i].state = TASK_READY;
            }
        }
    }
}

void task_exit(void) {
    serial_write_string("[SCHED] Tarea terminada: ");
    serial_write_string(tasks[current_task].name);
    serial_write_string("\n");

    /* Marcar como muerta */
    tasks[current_task].state = TASK_DEAD;
    
    /* No liberamos el stack aquí (lo hacemos lazy o en un reaper).
     * Por ahora solo marcamos DEAD y forzamos un switch. */
    
    /* Forzar switch a otra tarea. No podemos retornar de aquí
     * porque el stack de esta tarea ya no es válido conceptualmente. */
    sched_ticks = SCHEDULER_HZ;
    schedule();

    /* Si llegamos aquí, no había otra tarea (no debería pasar) */
    for (;;) { __asm__ volatile("hlt"); }
}

task_t* task_get_current(void) {
    return &tasks[current_task];
}

int task_get_count(void) {
    int count = 0;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state == TASK_READY || tasks[i].state == TASK_RUNNING) {
            count++;
        }
    }
    return count;
}

int task_kill(uint32_t pid) {
    if (pid == 0) return -1; /* No matar kernel */
    
    for (int i = 1; i < task_count; i++) {
        if (tasks[i].id == pid && tasks[i].state != TASK_DEAD) {
            tasks[i].state = TASK_DEAD;
            serial_write_string("[SCHED] Killed task PID ");
            /* TODO: Convert PID to string properly */
            serial_write_string("\n");
            
            if (i == current_task) {
                schedule(); /* Yield if killing self */
            }
            return 0;
        }
    }

    return -1;
}

task_t* task_get_at(int index) {
    if (index >= 0 && index < MAX_TASKS) {
        return &tasks[index];
    }
    return NULL;
}

int task_get_max(void) {
    return MAX_TASKS;
}

task_t* task_get_by_id(uint32_t id) {
    if (!scheduler_active) return NULL;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].id == id && tasks[i].state != 0 && tasks[i].state != TASK_DEAD) {
            return &tasks[i];
        }
    }
    return NULL;
}
