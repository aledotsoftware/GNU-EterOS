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

struct syscall_regs;

/* ========================================================================= */
/* Configuración                                                             */
/* ========================================================================= */
#define MAX_TASKS       64
#define TASK_STACK_SIZE  32768   /* 32 KB por tarea (GUI requires more) */
#define SCHEDULER_HZ     10    /* Switch cada 10 ticks (100ms a 100Hz PIT) */
#define MAX_FD           256   /* Máximo de descriptores de archivo por tarea */

#define KERNEL_STACK_BASE       0xFFFFFF0000000000ULL
#define KERNEL_STACK_GUARD_SIZE 4096  /* 4KB Guard Page */

/**
 * @brief Initializes the scheduler for an Application Processor (AP).
 *
 * Creates a specific idle task for the current CPU core and prepares
 * its local run queue.
 *
 * @return None.
 */
void task_init_ap(void);

static inline uint64_t task_irq_save(void) {
    uint64_t flags = 0;
#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
#endif
    return flags;
}

static inline void task_irq_restore(uint64_t flags) {
#ifndef __ETEROS_HOST_TEST__
    if (flags & 0x200) {
        __asm__ volatile ("sti");
    }
#endif
}

typedef struct file_descriptor {
    struct fs_node* node;
    uint32_t offset;
    int flags;
    char path[128]; // Store resolved absolute path to support dirfd
} file_descriptor_t;

/* ========================================================================= */
/* Estados de Tarea                                                          */
/* ========================================================================= */
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_STOPPED,
    TASK_DEAD
} task_state_t;

/* ========================================================================= */
/* Estructura de Tarea                                                       */
/* ========================================================================= */
struct semaphore; /* Forward declaration */

typedef struct task {
    uint64_t       rsp;                     /* Stack pointer guardado */
    uint8_t*       stack_base;              /* Base del stack alocado */
    uint64_t       kernel_stack_top;        /* Tope del stack de kernel (para TSS RSP0) */
    uint64_t       cr3;                     /* Directorio de páginas (PML4) */
    uint32_t       id;                      /* Task ID único */
    uint32_t       parent_id;               /* Parent Task ID */

    /* Threading */
    uint32_t       tgid;                    /* Thread Group ID (PID) */
    uint32_t       pgid;                    /* Process Group ID */
    uint32_t       sid;                     /* Session ID */
    struct task*   group_leader;            /* Puntero al líder del grupo */
    uint32_t*      clear_child_tid;         /* Dirección para futex_wake al terminar */

    task_state_t   state;                   /* Estado actual */
    uint64_t       wake_tick;               /* Tick para despertar si duerme */
    struct semaphore* waiting_sem;          /* Semaphore waiting on (if blocked) */
    char           name[32];                /* Nombre descriptivo */
    char           executable_path[256];    /* Absolute path to the loaded ELF */

    struct task*   next_ready;              /* Next task in ready queue */
    struct task*   prev_ready;              /* Previous task in ready queue */

    struct task*   next_sleep;              /* Next task in sleep queue */
    struct task*   prev_sleep;              /* Previous task in sleep queue */

    /* POSIX Compatibility */
    file_descriptor_t* fd_table;            /* Pointer to File Descriptor Table (shared in threads) */
    file_descriptor_t fd_table_internal[MAX_FD]; /* Internal FD table for processes */
    char           cwd[256];                /* Current Working Directory */
    struct fs_node* cwd_node;               /* Current Working Directory Node */
    uint32_t       signal_mask;             /* Mask of blocked signals */
    uint32_t       uid;                     /* User ID */
    uint32_t       gid;                     /* Group ID */
    uint32_t       euid;                    /* Effective User ID */
    uint32_t       egid;                    /* Effective Group ID */
    uint32_t       signal_pending;          /* Bitmap of pending signals */
    uint64_t       sigaltstack_sp;          /* Alternate signal stack base */
    uint64_t       sigaltstack_size;        /* Alternate signal stack size */
    uint32_t       sigaltstack_flags;       /* SS_DISABLE / SS_ONSTACK */

    void           (**signal_handlers)(int); /* Signal Handlers Pointer (shared in threads) */
    void*          signal_handlers_internal[32];
    void           (**signal_restorers)(void); /* Signal Restorer Trampolines Pointer */
    void*          signal_restorers_internal[32];
    uint32_t*      signal_flags;            /* Signal Flags Pointer */
    uint32_t       signal_flags_internal[32];
    uint64_t*      signal_action_masks;     /* Per-signal mask Pointer */
    uint64_t       signal_action_masks_internal[32];

    /* SMP & Threading extensions */
    uint64_t       affinity[4];             /* cpu_set_t inline for simplicity (256 bits max) */
    int            target_cpu;

    /* Linux Compatibility (TLS & Heap) */
    uint64_t       brk;                     /* Program break (end of data segment) */
    uint64_t       fs_base;                 /* FS Segment Base (TLS) */
    uint64_t       gs_base;                 /* GS Segment Base (TLS) */
    uint8_t        os_abi;
    uint8_t        is_linux;                /* Linux Compatibility Flag */                  /* ABI (0=SysV/Native, 3=Linux) */
    uint64_t       user_rsp;                /* User Stack Pointer (saved during syscall) */
    uint64_t       mmap_base;               /* Base address for mmap allocator */

    /* FPU Context (512 bytes, aligned to 16 bytes for FXSAVE/FXRSTOR) */
    uint8_t        fpu_state[512] __attribute__((aligned(16)));

    void           (*entry)(void);          /* Entry point for task_entry_wrapper */
    int            exit_code;               /* Exit status code */
    int            wait_status;             /* Raw wait status encoding */
    int            wait_code;               /* CLD_* event code */
    uint8_t        wait_pending;            /* Parent-visible wait event pending */
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
 * Pone la tarea actual en la cola de sleep con un wake_tick específico.
 * Utilizado por futex u otros primitivos de sincronización.
 */
void task_block_with_timeout(uint64_t wake_tick);

/**
 * Bloquea la tarea actual sin timeout, debe ser despertada manualmente.
 */
void task_block(void);

/**
 * Verifica si hay tareas dormidas que deban despertar.
 * Llamado desde el timer interrupt.
 */
void task_wake_expired(uint64_t current_tick);

/**
 * Despierta una tarea bloqueada o dormida.
 * Cambia su estado a TASK_READY y la encola.
 */
void task_wakeup(task_t* t);

/**
 * Termina la tarea actual.
 */
void task_exit(int status);
void task_exit_signal(int sig);

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
 * Crea un hilo (Clone).
 * @param clone_flags Flags de Linux CLONE_*
 * @param stack Stack del nuevo hilo
 * @param parent_tid Dirección donde guardar el TID en el padre
 * @param child_tid Dirección donde guardar el TID en el hijo (y limpiar al salir)
 * @param tls TLS a usar (FS base)
 * @param regs Registros del syscall
 */
int task_clone(uint64_t clone_flags, uint64_t stack, uint32_t* parent_tid, uint32_t* child_tid, uint64_t tls, struct syscall_regs* regs);

/**
 * Reemplaza la imagen del proceso actual por una nueva (Exec).
 * @param path Ruta al ejecutable ELF.
 * @param argv Argumentos.
 * @param envp Variables de entorno.
 * @param regs Registros del syscall (para modificar RIP/RSP).
 * @return -1 en error, no retorna en éxito.
 */
int task_exec(const char* path, char* const argv[], char* const envp[], struct syscall_regs* regs);

/**
 * Espera a que un proceso hijo termine.
 * @param pid PID del hijo (-1 para cualquiera).
 * @param status Puntero donde guardar el estado de salida.
 * @param options Opciones (WNOHANG, etc).
 * @return PID del hijo recolectado, o -1/0.
 */
int task_waitpid(int pid, int* status, int options);
int task_waitid(int idtype, int id, int options, int* out_pid, int* out_status, int* out_code);

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
 * @param old_fpu Puntero al buffer FPU (fpu_state) de la tarea saliente.
 * @param new_fpu Puntero al buffer FPU (fpu_state) de la tarea entrante a restaurar.
 */
extern void context_switch(uint64_t* old_rsp, uint64_t* new_rsp, void* old_fpu, void* new_fpu);

#endif /* ETEROS_TASK_H */
