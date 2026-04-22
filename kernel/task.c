/**
 * éterOS - Task Scheduler (Round-Robin Preemptive, SMP-Ready)
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
 *   - Spinlock protege el estado global para SMP
 */

#include "../include/task.h"
#include "../include/mm.h"
#include "../include/string.h"
#include "../include/serial.h"
#include "../include/timer.h"
#include "../include/gdt.h"
#include "../include/cpu.h"
#include "../include/msr.h"
#include "../include/lock.h"
#include "../include/vmm.h"
#include "../include/pmm.h"
#include "../include/syscall.h"
#include "../include/elf.h"
#include "../include/errno.h"
#include "../include/fcntl.h"
#include "../include/sched.h"
#include "../include/sys/signal.h"
#include "../include/futex.h"
#include "../include/apic.h"

#define AT_NULL   0
#define AT_PHDR   3
#define AT_PAGESZ 6
#define AT_ENTRY  9
#define AT_UID    11
#define AT_EUID   12
#define AT_GID    13
#define AT_EGID   14
#define AT_SECURE 23
#define AT_BASE   7
#define AT_PHENT  4
#define AT_PHNUM  5

extern void fork_return(void);

static inline uint64_t task_irq_save(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static inline void task_irq_restore(uint64_t flags) {
    if (flags & 0x200) {
        __asm__ volatile ("sti");
    }
}

/* ========================================================================= */
/* Helper de Stack de Kernel                                                 */
/* ========================================================================= */

static void* alloc_kernel_stack(int slot) {
    uint64_t base = KERNEL_STACK_BASE + (uint64_t)slot * (TASK_STACK_SIZE + KERNEL_STACK_GUARD_SIZE);
    uint64_t stack_start = base + KERNEL_STACK_GUARD_SIZE;
    uint64_t stack_end = stack_start + TASK_STACK_SIZE;

    /* Check reuse (if first page is mapped) */
    if (vmm_virt_to_phys(stack_start)) {
        memset((void*)stack_start, 0, TASK_STACK_SIZE);
        return (void*)stack_start;
    }

    /* Guard page unmapped */
    vmm_unmap_page(base);

    /* Allocate and Map */
    for (uint64_t addr = stack_start; addr < stack_end; addr += PAGE_SIZE) {
        void* phys = pmm_alloc_page();
        if (!phys) {
            /* Rollback previous pages if OOM */
            for (uint64_t rollback_addr = stack_start; rollback_addr < addr; rollback_addr += PAGE_SIZE) {
                uint64_t p = vmm_virt_to_phys(rollback_addr);
                if (p) {
                    vmm_unmap_page(rollback_addr);
                    pmm_free_page((void*)p);
                }
            }
            return NULL;
        }
        if (vmm_map_page((uint64_t)phys, addr, PAGE_PRESENT | PAGE_WRITE) < 0) {
             return NULL;
        }
    }

    memset((void*)stack_start, 0, TASK_STACK_SIZE);
    return (void*)stack_start;
}

/* ========================================================================= */
/* Estado Global del Scheduler                                               */
/* ========================================================================= */

static task_t   tasks[MAX_TASKS] __attribute__((aligned(16)));
static uint64_t task_bitmap   = 0;
_Static_assert(MAX_TASKS <= 64, "Bitmap optimization supports max 64 tasks");

/* static int current_task removed in favor of per-CPU */
static int      task_count    = 0;    /* Número total de tareas */
static uint32_t next_id       = 0;    /* Generador de IDs */
/* static uint32_t sched_ticks removed in favor of per-CPU */
static bool     scheduler_active = false; /* El scheduler está inicializado? */
static spinlock_t sched_lock  = 0;    /* SMP protection */

/* Queue Globals (O(1) Scheduler) */
static task_t*  sleep_head = NULL;

static void enqueue_sleep(task_t* t) {
    if (!t) return;
    /* Only enqueue if not already in queue (or clear it first).
       We assume caller has cleared it or it's a new sleep request. */
    t->next_sleep = NULL;
    t->prev_sleep = NULL;

    if (!sleep_head || sleep_head->wake_tick >= t->wake_tick) {
        t->next_sleep = sleep_head;
        t->prev_sleep = NULL;
        if (sleep_head) sleep_head->prev_sleep = t;
        sleep_head = t;
    } else {
        task_t* curr = sleep_head;
        while (curr->next_sleep && curr->next_sleep->wake_tick < t->wake_tick) {
            curr = curr->next_sleep;
        }
        t->next_sleep = curr->next_sleep;
        if (curr->next_sleep) curr->next_sleep->prev_sleep = t;
        t->prev_sleep = curr;
        curr->next_sleep = t;
    }
}

static void dequeue_sleep(task_t* t) {
    if (!t) return;
    /* If not in list, do nothing. We check this by seeing if prev/next are set,
       or if it is the head. */
    if (!t->prev_sleep && !t->next_sleep && sleep_head != t) {
        return;
    }

    if (t->prev_sleep) {
        t->prev_sleep->next_sleep = t->next_sleep;
    } else if (sleep_head == t) {
        sleep_head = t->next_sleep;
    }

    if (t->next_sleep) {
        t->next_sleep->prev_sleep = t->prev_sleep;
    }

    t->next_sleep = NULL;
    t->prev_sleep = NULL;
}

static void enqueue_ready(task_t* t) {
    if (!t) return;

    /* Simple load balancing / target assignment */
    cpu_info_t* target_cpu_ptr = NULL;
    if (t->target_cpu >= 0 && t->target_cpu < total_cpus && cpus[t->target_cpu].state == CPU_STATE_ONLINE) {
        target_cpu_ptr = &cpus[t->target_cpu];
    } else {
        /* Pick least loaded or simple round robin. For now, assign to CPU 0 if no valid target */
        /* Let's find least tasks cpu */
        int best_cpu = 0;
        int min_tasks = 999999;
        for (int i = 0; i < total_cpus; i++) {
            if (cpus[i].state == CPU_STATE_ONLINE) {
                if (cpus[i].local_task_count < min_tasks) {
                    min_tasks = cpus[i].local_task_count;
                    best_cpu = i;
                }
            }
        }
        target_cpu_ptr = &cpus[best_cpu];
        t->target_cpu = best_cpu;
    }

    t->next_ready = NULL;
    t->prev_ready = (task_t*)target_cpu_ptr->local_ready_tail;

    if (target_cpu_ptr->local_ready_tail) {
        ((task_t*)target_cpu_ptr->local_ready_tail)->next_ready = t;
    } else {
        target_cpu_ptr->local_ready_head = t;
    }
    target_cpu_ptr->local_ready_tail = t;
    target_cpu_ptr->local_task_count++;
}

static void dequeue_ready(task_t* t) {
    if (!t) return;

    cpu_info_t* target_cpu_ptr = &cpus[t->target_cpu];

    if (t->prev_ready) {
        t->prev_ready->next_ready = t->next_ready;
    } else {
        /* If head is t */
        if (target_cpu_ptr->local_ready_head == t) {
            target_cpu_ptr->local_ready_head = t->next_ready;
        }
    }

    if (t->next_ready) {
        t->next_ready->prev_ready = t->prev_ready;
    } else {
        /* If tail is t */
        if (target_cpu_ptr->local_ready_tail == t) {
            target_cpu_ptr->local_ready_tail = t->prev_ready;
        }
    }

    t->next_ready = NULL;
    t->prev_ready = NULL;
    target_cpu_ptr->local_task_count--;
}

/* CPU Load Metrics */
static uint64_t cpu_total_ticks = 0;
static uint64_t cpu_idle_ticks = 0;
static int      cpu_last_load = 0;

/* Variable global para el stack del kernel (usada por syscall_entry) */
uint64_t kernel_stack_top = 0x90000;

static int find_free_slot(void) {
    if (~task_bitmap == 0) return -1;

    int slot = __builtin_ctzll(~task_bitmap);
    if (slot >= MAX_TASKS) return -1;

    task_bitmap |= (1ULL << slot);
    return slot;
}

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
    /* Release the lock held by the previous task during switch */
    spin_unlock(&sched_lock);

    /* Habilitar interrupciones (estamos en una tarea nueva, el context_switch
       no las habilita automáticamente como haría iretq) */
    __asm__ volatile("sti");

    task_t* self = task_get_current();
    if (self && self->entry) {
        self->entry();
    }

    /* Si la función retorna, terminamos la tarea */
    task_exit(0);
}

/* ========================================================================= */
/* API del Scheduler                                                         */
/* ========================================================================= */

/**
 * @brief Initializes the scheduler and creates the initial kernel task.
 *
 * This function sets up the task array, initializes the ready queue,
 * and configures the first task (Task 0) which represents the current
 * thread of execution (typically the kernel or shell). It sets up the
 * initial stack, POSIX and Linux compatibility fields, and marks the
 * scheduler as active.
 *
 * @return None.
 */
void scheduler_init(void) {
    memset(tasks, 0, sizeof(tasks));
    task_bitmap = 1; /* Task 0 used */


    /* Tarea 0: Representa el hilo de ejecución actual (kernel/shell) */
    tasks[0].id = next_id++;
    tasks[0].parent_id = 0;
    tasks[0].tgid = tasks[0].id;
    tasks[0].pgid = tasks[0].tgid;
    tasks[0].sid = tasks[0].tgid;
    tasks[0].group_leader = &tasks[0];
    tasks[0].clear_child_tid = NULL;
    tasks[0].state = TASK_RUNNING;
    tasks[0].stack_base = NULL;  /* El kernel ya tiene su stack */
    tasks[0].rsp = 0;            /* Se llenará en el primer context_switch */

    /* Inicializar CR3 del kernel */
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    tasks[0].cr3 = cr3;

    /* Configurar stack inicial para Task 0 (Boot Stack en 128MB) */
    kernel_stack_top = 0x90000;
    tasks[0].kernel_stack_top = kernel_stack_top;

    strlcpy(tasks[0].name, "kernel", sizeof(tasks[0].name));

    /* POSIX Init for Kernel Task */
    tasks[0].fd_table = tasks[0].fd_table_internal;
    memset(tasks[0].fd_table_internal, 0, sizeof(tasks[0].fd_table_internal));
    strlcpy(tasks[0].cwd, "/", sizeof(tasks[0].cwd));
    strlcpy(tasks[0].executable_path, "/kernel", sizeof(tasks[0].executable_path));
    tasks[0].cwd_node = fs_root;
    if (tasks[0].cwd_node) tasks[0].cwd_node->ref_count++;
    tasks[0].signal_mask = 0;
    tasks[0].signal_pending = 0;
    tasks[0].sigaltstack_sp = 0;
    tasks[0].sigaltstack_size = 0;
    tasks[0].sigaltstack_flags = 2; /* SS_DISABLE */

    tasks[0].signal_handlers = (void (**)(int))tasks[0].signal_handlers_internal;
    tasks[0].signal_restorers = (void (**)(void))tasks[0].signal_restorers_internal;
    tasks[0].signal_flags = tasks[0].signal_flags_internal;
    tasks[0].signal_action_masks = tasks[0].signal_action_masks_internal;
    memset(tasks[0].signal_handlers_internal, 0, sizeof(tasks[0].signal_handlers_internal));
    memset(tasks[0].signal_restorers_internal, 0, sizeof(tasks[0].signal_restorers_internal));
    memset(tasks[0].signal_flags_internal, 0, sizeof(tasks[0].signal_flags_internal));
    memset(tasks[0].signal_action_masks_internal, 0, sizeof(tasks[0].signal_action_masks_internal));

    /* Linux Init */
    tasks[0].brk = 0;
    tasks[0].fs_base = 0;
    tasks[0].gs_base = 0;
    tasks[0].uid = 0;
    tasks[0].gid = 0;
    tasks[0].euid = 0;
    tasks[0].egid = 0;
    tasks[0].mmap_base = 0x700000000000ULL;
    tasks[0].wait_status = 0;
    tasks[0].wait_code = 0;
    tasks[0].wait_pending = 0;

    task_count = 1;
    /* current_task = 0; removed */
    scheduler_active = true;

    /* Update per-CPU current_task pointer if GS_BASE is valid */
    cpu_info_t* cpu = get_current_cpu();
    if (cpu) {
        cpu->current_task = &tasks[0];
        cpu->sched_ticks = 0;
        /* Initialize BSP TSS RSP0 */
        tss_set_rsp0(kernel_stack_top);
    }

    serial_write_string("[SCHED] Scheduler Round-Robin inicializado\n");
}

/**
 * @brief Initializes the scheduler for an Application Processor (AP).
 *
 * Creates a specific idle task for the current CPU core and prepares
 * its local run queue.
 *
 * @return None.
 */
void task_init_ap(void) {
    /* Inicializar la tarea "Idle" para este AP */
    __asm__ volatile("cli");
    spin_lock(&sched_lock);

    int slot = find_free_slot();

    if (slot == -1) {
        spin_unlock(&sched_lock);
        serial_write_string("[SCHED] Error: No slots for AP idle task\n");
        return;
    }

    /* Inicializar tarea Idle para este CPU */
    tasks[slot].id = next_id++;
    tasks[slot].parent_id = 0;
    tasks[slot].tgid = tasks[slot].id;
    tasks[slot].pgid = tasks[slot].tgid;
    tasks[slot].sid = tasks[slot].tgid;
    tasks[slot].group_leader = &tasks[slot];
    tasks[slot].clear_child_tid = NULL;
    tasks[slot].state = TASK_RUNNING; /* Ya está corriendo */
    tasks[slot].stack_base = NULL;    /* Usa el stack del AP init */
    tasks[slot].rsp = 0;

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    tasks[slot].cr3 = cr3;
    tasks[slot].kernel_stack_top = 0;

    cpu_info_t* cpu = get_current_cpu();
    char name[32] = "Idle AP ";
    if (cpu) {
        char buf[8];
        itoa_s(cpu->apic_id, buf, 8, 10);
        strlcat(name, buf, 32);

        cpu->current_task = &tasks[slot];
        cpu->sched_ticks = 0;
        /* Set RSP0 in TSS for this AP (using its current stack top) */
        tss_set_rsp0(cpu->kernel_stack_top);
        tasks[slot].kernel_stack_top = cpu->kernel_stack_top;
    } else {
        strlcat(name, "?", 32);
    }

    strlcpy(tasks[slot].name, name, sizeof(tasks[slot].name));

    /* Basic Init */
    tasks[slot].fd_table = tasks[slot].fd_table_internal;
    memset(tasks[slot].fd_table_internal, 0, sizeof(tasks[slot].fd_table_internal));
    strlcpy(tasks[slot].cwd, "/", sizeof(tasks[slot].cwd));
    tasks[slot].fs_base = 0;
    tasks[slot].gs_base = 0;
    tasks[slot].uid = 0;
    tasks[slot].gid = 0;

    tasks[slot].signal_handlers = (void (**)(int))tasks[slot].signal_handlers_internal;
    tasks[slot].signal_restorers = (void (**)(void))tasks[slot].signal_restorers_internal;
    tasks[slot].signal_flags = tasks[slot].signal_flags_internal;
    tasks[slot].signal_action_masks = tasks[slot].signal_action_masks_internal;
    tasks[slot].sigaltstack_sp = 0;
    tasks[slot].sigaltstack_size = 0;
    tasks[slot].sigaltstack_flags = 2; /* SS_DISABLE */
    memset(tasks[slot].signal_handlers_internal, 0, sizeof(tasks[slot].signal_handlers_internal));
    memset(tasks[slot].signal_restorers_internal, 0, sizeof(tasks[slot].signal_restorers_internal));
    memset(tasks[slot].signal_flags_internal, 0, sizeof(tasks[slot].signal_flags_internal));
    memset(tasks[slot].signal_action_masks_internal, 0, sizeof(tasks[slot].signal_action_masks_internal));

    if (slot >= task_count) {
        task_count = slot + 1;
    }

    spin_unlock(&sched_lock);

    serial_write_string("[SCHED] AP Task Initialized: ");
    serial_write_string(name);
    serial_write_string("\n");
}

/**
 * @brief Creates a new kernel task.
 *
 * Allocates a free slot, a kernel stack (with guard page unmapped for safety),
 * and sets up the initial execution context to start at the given entry function.
 * The task is then added to the ready queue.
 *
 * @param name The name of the new task (for debugging).
 * @param entry Function pointer to the task's entry point.
 * @return The ID of the newly created task, or -1 if the max task limit is reached or out of memory.
 */
int task_create(const char* name, void (*entry)(void)) {
    uint64_t irq_flags = task_irq_save();
    spin_lock(&sched_lock);

    if (task_count >= MAX_TASKS) {
        serial_write_string("[SCHED] Error: Maximo de tareas alcanzado\n");
        spin_unlock(&sched_lock);
        task_irq_restore(irq_flags);
        return -1;
    }

    /* Encontrar un slot libre */
    int slot = find_free_slot();
    if (slot == -1) {
        spin_unlock(&sched_lock);
        task_irq_restore(irq_flags);
        return -1;
    }

    /* Alocar stack para la nueva tarea (con Guard Page) */
    uint8_t* stack = (uint8_t*)alloc_kernel_stack(slot);
    if (!stack) {
        serial_write_string("[SCHED] Error: No hay memoria para el stack\n");
        task_bitmap &= ~(1ULL << slot);
        spin_unlock(&sched_lock);
        task_irq_restore(irq_flags);
        return -1;
    }
    /* Note: alloc_kernel_stack already memsets to 0 */

    /* Configurar la tarea */
    tasks[slot].id = next_id++;
    tasks[slot].parent_id = 0;
    tasks[slot].tgid = tasks[slot].id;
    tasks[slot].pgid = tasks[slot].tgid;
    tasks[slot].sid = tasks[slot].tgid;
    tasks[slot].state = TASK_READY;
    tasks[slot].stack_base = stack;

    /* Configurar stack de kernel y CR3 */
    tasks[slot].kernel_stack_top = (uint64_t)(stack + TASK_STACK_SIZE);

    /* Heredar CR3 del kernel (por ahora, luego cada proceso tendrá su PML4) */
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    tasks[slot].cr3 = cr3;

    strlcpy(tasks[slot].name, name, sizeof(tasks[slot].name));
    strlcpy(tasks[slot].executable_path, "/", sizeof(tasks[slot].executable_path));
    tasks[slot].entry = entry;

    /* POSIX Init for New Task */
    tasks[slot].fd_table = tasks[slot].fd_table_internal;
    memset(tasks[slot].fd_table_internal, 0, sizeof(tasks[slot].fd_table_internal));
    strlcpy(tasks[slot].cwd, "/", sizeof(tasks[slot].cwd));
    tasks[slot].cwd_node = fs_root;
    if (tasks[slot].cwd_node) tasks[slot].cwd_node->ref_count++;
    tasks[slot].signal_mask = 0;
    tasks[slot].signal_pending = 0;
    tasks[slot].sigaltstack_sp = 0;
    tasks[slot].sigaltstack_size = 0;
    tasks[slot].sigaltstack_flags = 2; /* SS_DISABLE */

    tasks[slot].signal_handlers = (void (**)(int))tasks[slot].signal_handlers_internal;
    tasks[slot].signal_restorers = (void (**)(void))tasks[slot].signal_restorers_internal;
    tasks[slot].signal_flags = tasks[slot].signal_flags_internal;
    tasks[slot].signal_action_masks = tasks[slot].signal_action_masks_internal;
    memset(tasks[slot].signal_handlers_internal, 0, sizeof(tasks[slot].signal_handlers_internal));
    memset(tasks[slot].signal_restorers_internal, 0, sizeof(tasks[slot].signal_restorers_internal));
    memset(tasks[slot].signal_flags_internal, 0, sizeof(tasks[slot].signal_flags_internal));
    memset(tasks[slot].signal_action_masks_internal, 0, sizeof(tasks[slot].signal_action_masks_internal));

    /* Linux Init */
    tasks[slot].brk = 0;
    tasks[slot].fs_base = 0;
    tasks[slot].gs_base = 0;
    tasks[slot].uid = 0;
    tasks[slot].gid = 0;
    tasks[slot].euid = 0;
    tasks[slot].egid = 0;
    tasks[slot].mmap_base = 0x700000000000ULL;
    tasks[slot].wait_status = 0;
    tasks[slot].wait_code = 0;
    tasks[slot].wait_pending = 0;

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

    enqueue_ready(&tasks[slot]);
    spin_unlock(&sched_lock);
    task_irq_restore(irq_flags);

    serial_write_string("[SCHED] Tarea creada: ");
    serial_write_string(name);
    serial_write_string("\n");

    return (int)tasks[slot].id;
}

/**
 * Encuentra la siguiente tarea READY usando Round-Robin.
 * @return Puntero a la siguiente tarea, o current_task si no hay otra.
 */
static task_t* find_next_task(task_t* current) {
    cpu_info_t* cpu = get_current_cpu();
    if (!cpu) return NULL;

    /* O(1) Scheduler: Check ready queue */
    if (cpu->local_ready_head) {
        return (task_t*)cpu->local_ready_head;
    }

    /* Si no hay tareas listas y la actual esta muerta/durmiendo */
    if (current->state == TASK_RUNNING) {
        return current;
    }

    return NULL; /* No hay tareas ejecutables */
}

/**
 * @brief Main scheduler function (Round-Robin SMP).
 *
 * Performs context switching by finding the next READY task in the queue.
 * Handles SMP CPU load tracking, updates TLS (FS/GS bases),
 * switches address spaces (CR3) if necessary, and performs the architecture-specific
 * context switch. Uses spinlocks to safely modify global state.
 *
 * @return None.
 */
void schedule(void) {
    if (!scheduler_active) return;

    /* Deshabilitar interrupciones para proteger el estado del scheduler */
    __asm__ volatile("cli");

    /* Acquire Spinlock */
    spin_lock(&sched_lock);

    cpu_info_t* cpu = get_current_cpu();
    if (!cpu) {
        spin_unlock(&sched_lock);
        __asm__ volatile("sti");
        return;
    }

    task_t* current = (task_t*)cpu->current_task;
    if (!current) current = &tasks[0]; /* Fallback */

    /* CPU Load tracking */
    cpu_total_ticks++;
    if (cpu_total_ticks >= SCHEDULER_HZ) {
        if (cpu_total_ticks > 0)
             cpu_last_load = 100 - ( (cpu_idle_ticks * 100) / cpu_total_ticks );
        cpu_total_ticks = 0;
        cpu_idle_ticks = 0;
    }

    task_t* next_task = find_next_task(current);

    /* Si no hay tareas listas y la actual esta muerta/durmiendo */
    if (next_task == NULL) {
        cpu_idle_ticks++;
        spin_unlock(&sched_lock);
        __asm__ volatile("sti");

        /* Wait for interrupts and loop until a task is ready */
        for(;;) {
            __asm__ volatile("hlt");
            if (cpu->local_ready_head) {
                break;
            }
        }

        /* Re-acquire lock and find the new task */
        __asm__ volatile("cli");
        spin_lock(&sched_lock);
        next_task = find_next_task(current);
        if (next_task == NULL) {
             spin_unlock(&sched_lock);
             __asm__ volatile("sti");
             return;
        }
    }

    if (next_task == current) {
        /* Clear any pending timeout if we are resuming a blocked task */
        dequeue_sleep(current);
        current->wake_tick = 0;
        spin_unlock(&sched_lock);
        __asm__ volatile("sti");
        return;
    }

    /* Solo switchear cada SCHEDULER_HZ ticks (excepto si la actual murio/bloqueo) */
    if (current->state == TASK_RUNNING) {
        cpu->sched_ticks++;
        if (cpu->sched_ticks < SCHEDULER_HZ) {
            spin_unlock(&sched_lock);
            __asm__ volatile("sti");
            return;
        }
    }
    cpu->sched_ticks = 0;

    /* Cambiar estado */
    if (current->state == TASK_RUNNING) {
        current->state = TASK_READY;
        enqueue_ready(current);
    }

    /* Save current TLS state */
    current->fs_base = rdmsr(MSR_FS_BASE);
    current->gs_base = rdmsr(MSR_KERNEL_GS_BASE); /* User GS is in KERNEL_GS_BASE while in kernel */
    
    /* Remove next task from ready queue */
    if (next_task != current) {
        dequeue_ready(next_task);
    }

    next_task->state = TASK_RUNNING;
    /* Clear any pending timeout when task is scheduled */
    dequeue_sleep(next_task);
    next_task->wake_tick = 0;
    cpu->current_task = next_task;

    /* Actualizar TSS RSP0 y Per-CPU Kernel Stack para Syscalls */
    tss_set_rsp0(next_task->kernel_stack_top);
    cpu->kernel_stack_top = next_task->kernel_stack_top;

    /* Restore User Stack Pointer (for syscall exit / fork_return) */
    cpu->user_stack_scratch = next_task->user_rsp;

    /* Restore TLS state */
    wrmsr(MSR_FS_BASE, next_task->fs_base);
    wrmsr(MSR_KERNEL_GS_BASE, next_task->gs_base);

    /* Switch Address Space (CR3) if necessary */
    uint64_t current_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
    if (next_task->cr3 != current_cr3 && next_task->cr3 != 0) {
        char buf1[32], buf2[32];
        serial_write_string("[SCHED] CR3 Switch: ");
        utoa_hex_s(current_cr3, buf1, sizeof(buf1));
        serial_write_string(buf1);
        serial_write_string(" -> ");
        utoa_hex_s(next_task->cr3, buf2, sizeof(buf2));
        serial_write_string(buf2);
        serial_write_string(" (Task: ");
        serial_write_string(next_task->name);
        serial_write_string(")\n");
        __asm__ volatile("mov %0, %%cr3" : : "r"(next_task->cr3) : "memory");
    }

    /* Context Switch holding the lock! */
    /* The next task will release it in schedule() return or task_entry_wrapper */
    context_switch(&current->rsp, &next_task->rsp, current->fpu_state, next_task->fpu_state);

    /* We are back in the old task (now current) */
    spin_unlock(&sched_lock);
    __asm__ volatile("sti");
}

/**
 * @brief Voluntarily yields the CPU to the next ready task.
 *
 * If the scheduler is active and the current CPU can be determined,
 * this function immediately invokes the scheduler to switch to another
 * task. It allows tasks to relinquish their time slice cooperatively.
 *
 * @return None.
 */
void task_yield(void) {
    if (!scheduler_active) return;
    
    cpu_info_t* cpu = get_current_cpu();
    if (cpu) {
        /* Forzar un switch inmediato */
        cpu->sched_ticks = SCHEDULER_HZ;
    }
    schedule();
}

void task_block_with_timeout(uint64_t wake_tick) {
    if (!scheduler_active) return;

    spin_lock(&sched_lock);
    task_t* current = task_get_current();
    current->wake_tick = wake_tick;
    current->state = TASK_BLOCKED;
    enqueue_sleep(current);
    spin_unlock(&sched_lock);
}

void task_sleep(uint64_t ms) {
    if (!scheduler_active) {
        timer_wait((uint32_t)ms);
        return;
    }

    uint64_t ticks = ((uint64_t)ms * TIMER_HZ) / 1000;
    if (ms > 0 && ticks == 0) ticks = 1;

    /* Lock to modify state */
    spin_lock(&sched_lock);
    task_t* current = task_get_current();
    current->wake_tick = timer_get_ticks() + ticks;
    current->state = TASK_SLEEPING;
    enqueue_sleep(current);
    spin_unlock(&sched_lock);

    /* Ceder CPU */
    task_yield();

    while (current->state == TASK_SLEEPING) {
        __asm__ volatile("hlt");
    }

    current->state = TASK_RUNNING;
}

void task_wake_expired(uint64_t current_tick) {
    if (!scheduler_active) return;

    /* Called from timer interrupt, so interrupts are disabled. */
    spin_lock(&sched_lock);
    task_t* curr = sleep_head;
    while (curr && curr->wake_tick <= current_tick) {
        task_t* next = curr->next_sleep;
        curr->state = TASK_READY;
        dequeue_sleep(curr);
        enqueue_ready(curr);
        curr = next;
    }
    spin_unlock(&sched_lock);
}

void task_wakeup(task_t* t) {
    if (!scheduler_active || !t) return;

    /* Save interrupt state */
    uint64_t rflags;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
    __asm__ volatile("cli");

    spin_lock(&sched_lock);
    /* Only wake if BLOCKED or SLEEPING.
       If it is already READY or RUNNING, do nothing. */
    if (t->state == TASK_BLOCKED || t->state == TASK_SLEEPING) {
        t->state = TASK_READY;
        dequeue_sleep(t);
        enqueue_ready(t);

        cpu_info_t* current_cpu = get_current_cpu();
        if (current_cpu && (uint32_t)t->target_cpu != current_cpu->index) {
            /* Send IPI to the target CPU to wake it up or force schedule */
            lapic_send_ipi(cpus[t->target_cpu].apic_id, 0x20); /* 0x20 is timer vector, forces schedule() */
        }
    }
    if (t->state == TASK_STOPPED) {
        t->state = TASK_READY;
        enqueue_ready(t);
    }
    spin_unlock(&sched_lock);

    /* Restore interrupts if they were enabled */
    if (rflags & 0x200) {
        __asm__ volatile("sti");
    }
}

static void task_mark_parent_wait_event(task_t* current, int wait_status, int wait_code) {
    current->wait_status = wait_status;
    current->wait_code = wait_code;
    current->wait_pending = 1;
}

static void task_wake_parent_waiter(task_t* current) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].id == current->parent_id &&
            (tasks[i].state == TASK_SLEEPING || tasks[i].state == TASK_BLOCKED)) {
            tasks[i].state = TASK_READY;
            dequeue_sleep(&tasks[i]);
            enqueue_ready(&tasks[i]);

            cpu_info_t* target_cpu = &cpus[tasks[i].target_cpu];
            cpu_info_t* current_cpu = get_current_cpu();
            if (current_cpu && (uint32_t)tasks[i].target_cpu != current_cpu->index && target_cpu->state == CPU_STATE_ONLINE) {
                lapic_send_ipi(target_cpu->apic_id, 0x20);
            }
        }
    }
}

static void task_exit_internal(int status, int wait_status, int wait_code) {
    task_t* current = task_get_current();

    if (current->clear_child_tid != NULL) {
        if (vmm_verify_user_access(current->clear_child_tid, sizeof(uint32_t), 1)) {
            *current->clear_child_tid = 0;
            futex_wake(current->clear_child_tid, 1, FUTEX_WAKE);
        }
    }

    __asm__ volatile("cli");
    spin_lock(&sched_lock);

    serial_write_string("[SCHED] Tarea terminada: ");
    serial_write_string(current->name);
    serial_write_string("\n");

    current->state = TASK_DEAD;
    current->exit_code = status;
    task_mark_parent_wait_event(current, wait_status, wait_code);

    int slot = (int)(current - tasks);
    if (slot >= 0 && slot < MAX_TASKS) {
        task_bitmap &= ~(1ULL << slot);
    }
    spin_unlock(&sched_lock);

    /* Wake up any tasks waiting for this process */
    __asm__ volatile("cli");
    spin_lock(&sched_lock);
    task_wake_parent_waiter(current);
    spin_unlock(&sched_lock);
    __asm__ volatile("sti");

    /* Yield forever until switched out */
    schedule();
    
    __asm__ volatile("sti");
    for (;;) { __asm__ volatile("hlt"); }
}

void task_exit(int status) {
    task_exit_internal(status, ((status & 0xFF) << 8), CLD_EXITED);
}

void task_exit_signal(int sig) {
    task_exit_internal(128 + (sig & 0x7F), (sig & 0x7F), CLD_KILLED);
}

task_t* task_get_current(void) {
    cpu_info_t* cpu = get_current_cpu();
    if (cpu && cpu->current_task) {
        return (task_t*)cpu->current_task;
    }
    return &tasks[0]; /* Fallback/Boot */
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
    
    __asm__ volatile("cli");
    spin_lock(&sched_lock);

    task_t* current = task_get_current();
    int killed_self = 0;
    int found = 0;

    for (int i = 1; i < task_count; i++) {
        /* Kill all threads with this tgid */
        if (tasks[i].tgid == pid && tasks[i].state != TASK_DEAD) {
            if (tasks[i].state == TASK_READY) {
                dequeue_ready(&tasks[i]);
            }

            tasks[i].state = TASK_DEAD;
            tasks[i].exit_code = 128 + SIGKILL;
            task_mark_parent_wait_event(&tasks[i], SIGKILL & 0x7F, CLD_KILLED);
            task_bitmap &= ~(1ULL << i);

            /* Clean up TID pointer if requested by clone */
            if (tasks[i].clear_child_tid != NULL) {
                if (vmm_verify_user_access(tasks[i].clear_child_tid, sizeof(uint32_t), 1)) {
                    *tasks[i].clear_child_tid = 0;
                    futex_wake(tasks[i].clear_child_tid, 1, FUTEX_WAKE);
                }
            }

            serial_write_string("[SCHED] Killed thread TID ");
            serial_write_string("\n");
            
            if (&tasks[i] == current) {
                killed_self = 1;
            } else {
                task_wake_parent_waiter(&tasks[i]);
            }
            found = 1;
        }
    }

    spin_unlock(&sched_lock);
    __asm__ volatile("sti");

    if (found) {
        if (killed_self) {
            schedule();
            for (;;) { __asm__ volatile("hlt"); }
        }
        return 0;
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
        /* Permitimos buscar tareas READY, RUNNING o SLEEPING. 
           Solo ignoramos slots vacíos (id=0 y state=0) o DEAD. */
        if (tasks[i].id == id && tasks[i].name[0] != '\0' && tasks[i].state != TASK_DEAD) {
            return &tasks[i];
        }
    }
    return NULL;
}

int task_get_cpu_load(void) {
    return cpu_last_load;
}

/**
 * @brief Duplicates the current process (fork).
 *
 * Allocates a new task slot and clones the parent's address space using Copy-on-Write (CoW).
 * It copies the POSIX structures (file descriptors, signal handlers) while properly
 * increasing reference counts. The child process returns 0 via RAX.
 *
 * @param regs_ptr Pointer to the syscall registers state to be restored on return.
 * @return The ID of the child process in the parent, or -1 on failure. Child returns 0 via RAX.
 */
int task_fork(void* regs_ptr) {
    struct syscall_regs* regs = (struct syscall_regs*)regs_ptr;

    __asm__ volatile("cli");
    spin_lock(&sched_lock);

    /* 1. Find slot */
    if (task_count >= MAX_TASKS) {
        spin_unlock(&sched_lock);
        __asm__ volatile("sti");
        return -1;
    }
    int slot = find_free_slot();
    if (slot == -1) {
        spin_unlock(&sched_lock);
        __asm__ volatile("sti");
        return -1;
    }

    /* 2. Alloc Stack (con Guard Page) */
    uint8_t* stack = (uint8_t*)alloc_kernel_stack(slot);
    if (!stack) {
        task_bitmap &= ~(1ULL << slot);
        spin_unlock(&sched_lock);
        __asm__ volatile("sti");
        return -1;
    }
    /* Note: alloc_kernel_stack already memsets to 0 */

    /* 3. Setup Child Task */
    tasks[slot].id = next_id++;
    tasks[slot].parent_id = task_get_current()->id;
    tasks[slot].tgid = tasks[slot].id;
    tasks[slot].pgid = tasks[slot].tgid;
    tasks[slot].sid = tasks[slot].tgid;
    tasks[slot].group_leader = &tasks[slot];
    tasks[slot].clear_child_tid = NULL;
    tasks[slot].state = TASK_READY;
    tasks[slot].stack_base = stack;
    tasks[slot].kernel_stack_top = (uint64_t)(stack + TASK_STACK_SIZE);

    /* 4. Clone Address Space (CoW) */
    /* Release lock to avoid deadlock during TLB shootdowns or heavy VM activity */
    spin_unlock(&sched_lock);
    __asm__ volatile("sti");

    uint64_t child_cr3 = vmm_clone_pml4(1);

    __asm__ volatile("cli");
    spin_lock(&sched_lock);
    tasks[slot].cr3 = child_cr3;

    /* 5. Copy Task Struct Fields */
    task_t* parent = task_get_current();
    strlcpy(tasks[slot].name, parent->name, sizeof(tasks[slot].name));
    strlcpy(tasks[slot].executable_path, parent->executable_path, sizeof(tasks[slot].executable_path));
    /* Append (fork) to name? Optional */

    /* POSIX/Linux Fields */
    tasks[slot].fd_table = tasks[slot].fd_table_internal;
    memcpy(tasks[slot].fd_table_internal, parent->fd_table_internal, sizeof(parent->fd_table_internal));
    strlcpy(tasks[slot].cwd, parent->cwd, sizeof(tasks[slot].cwd));
    /* VFS nodes are shared pointers! Refcounting handled here. */
    for (int i = 0; i < MAX_FD; i++) {
        if (tasks[slot].fd_table[i].node) {
            __atomic_fetch_add(&tasks[slot].fd_table[i].node->ref_count, 1, __ATOMIC_SEQ_CST);
        }
    }

    tasks[slot].cwd_node = parent->cwd_node;
    if (tasks[slot].cwd_node) __atomic_fetch_add(&tasks[slot].cwd_node->ref_count, 1, __ATOMIC_SEQ_CST);

    tasks[slot].signal_mask = parent->signal_mask;
    tasks[slot].signal_pending = 0;
    tasks[slot].sigaltstack_sp = parent->sigaltstack_sp;
    tasks[slot].sigaltstack_size = parent->sigaltstack_size;
    tasks[slot].sigaltstack_flags = parent->sigaltstack_flags & ~1U; /* Clear SS_ONSTACK */
    tasks[slot].signal_handlers = (void (**)(int))tasks[slot].signal_handlers_internal;
    tasks[slot].signal_restorers = (void (**)(void))tasks[slot].signal_restorers_internal;
    tasks[slot].signal_flags = tasks[slot].signal_flags_internal;
    tasks[slot].signal_action_masks = tasks[slot].signal_action_masks_internal;
    memcpy(tasks[slot].signal_handlers_internal, parent->signal_handlers, sizeof(parent->signal_handlers_internal));
    memcpy(tasks[slot].signal_restorers_internal, parent->signal_restorers, sizeof(parent->signal_restorers_internal));
    memcpy(tasks[slot].signal_flags_internal, parent->signal_flags, sizeof(parent->signal_flags_internal));
    memcpy(tasks[slot].signal_action_masks_internal, parent->signal_action_masks, sizeof(parent->signal_action_masks_internal));
    tasks[slot].brk = parent->brk;
    tasks[slot].fs_base = parent->fs_base;
    tasks[slot].gs_base = parent->gs_base;
    tasks[slot].os_abi = parent->os_abi;
    tasks[slot].uid = parent->uid;
    tasks[slot].gid = parent->gid;
    tasks[slot].euid = parent->euid;
    tasks[slot].egid = parent->egid;
    tasks[slot].pgid = parent->pgid;
    tasks[slot].sid = parent->sid;
    tasks[slot].user_rsp = parent->user_rsp; /* Saved from syscall entry */
    tasks[slot].mmap_base = parent->mmap_base;
    tasks[slot].wait_status = 0;
    tasks[slot].wait_code = 0;
    tasks[slot].wait_pending = 0;

    /* 6. Setup Child Stack for Return */
    /* We need to copy `regs` to child stack top */
    uint64_t* sp = (uint64_t*)tasks[slot].kernel_stack_top;

    /* Push syscall_regs */
    sp = (uint64_t*)((uint8_t*)sp - sizeof(struct syscall_regs));
    memcpy(sp, regs, sizeof(struct syscall_regs));

    /* Modify RAX to 0 */
    ((struct syscall_regs*)sp)->rax = 0;

    /* Push context_switch frame to return to `fork_return` */
    /* fork_return expects nothing on stack except what context_switch pops? */
    /* context_switch pops: r15, r14, r13, r12, rbx, rbp */
    /* And then RETs. */
    /* So we push return address `fork_return`. */
    /* And 6 zeros (rbp..r15). */

    *(--sp) = (uint64_t)fork_return; /* RIP for ret */
    *(--sp) = 0; /* rbp */
    *(--sp) = 0; /* rbx */
    *(--sp) = 0; /* r12 */
    *(--sp) = 0; /* r13 */
    *(--sp) = 0; /* r14 */
    *(--sp) = 0; /* r15 */

    tasks[slot].rsp = (uint64_t)sp;

    if (slot >= task_count) task_count = slot + 1;

    enqueue_ready(&tasks[slot]);
    spin_unlock(&sched_lock);
    __asm__ volatile("sti");

    serial_write_string("[SCHED] Forked process PID ");
    /* ... log pid ... */

    return tasks[slot].id;
}

/**
 * @brief Creates a new thread or process based on clone flags.
 *
 * Serves as the backbone for both thread creation and process duplication.
 * Depending on `clone_flags` (e.g., CLONE_VM, CLONE_THREAD), it may share the
 * address space or create a new one, share file descriptors, and configure Thread Local Storage (TLS).
 *
 * @param clone_flags Flags indicating what resources to share (Linux compatible).
 * @param stack_top The user stack pointer for the new thread.
 * @param parent_tid Pointer to store the child's TID in the parent's memory.
 * @param child_tid Pointer to the child's TID (handled by userspace).
 * @param tls The thread local storage base address (FS base).
 * @param regs_ptr Pointer to the current syscall registers state.
 * @return The TID of the newly created thread/process, or -1 on failure.
 */
int task_clone(uint64_t clone_flags, uint64_t stack_top, uint32_t* parent_tid, uint32_t* child_tid, uint64_t tls, struct syscall_regs* regs_ptr) {
    struct syscall_regs* regs = (struct syscall_regs*)regs_ptr;

    __asm__ volatile("cli");
    spin_lock(&sched_lock);

    if (task_count >= MAX_TASKS) {
        spin_unlock(&sched_lock);
        __asm__ volatile("sti");
        return -1;
    }
    int slot = find_free_slot();
    if (slot == -1) {
        spin_unlock(&sched_lock);
        __asm__ volatile("sti");
        return -1;
    }

    uint8_t* stack = (uint8_t*)alloc_kernel_stack(slot);
    if (!stack) {
        task_bitmap &= ~(1ULL << slot);
        spin_unlock(&sched_lock);
        __asm__ volatile("sti");
        return -1;
    }

    task_t* parent = task_get_current();

    tasks[slot].id = next_id++;
    tasks[slot].parent_id = parent->id;

    if (clone_flags & CLONE_THREAD) {
        tasks[slot].tgid = parent->tgid;
        tasks[slot].group_leader = parent->group_leader;
        tasks[slot].pgid = parent->pgid;
        tasks[slot].sid = parent->sid;
    } else {
        tasks[slot].tgid = tasks[slot].id;
        tasks[slot].group_leader = &tasks[slot];
        tasks[slot].pgid = parent->pgid;
        tasks[slot].sid = parent->sid;
    }

    if (clone_flags & CLONE_CHILD_CLEARTID) {
        tasks[slot].clear_child_tid = child_tid;
    } else {
        tasks[slot].clear_child_tid = NULL;
    }

    tasks[slot].state = TASK_READY;
    tasks[slot].stack_base = stack;
    tasks[slot].kernel_stack_top = (uint64_t)(stack + TASK_STACK_SIZE);

    if (clone_flags & CLONE_VM) {
        tasks[slot].cr3 = parent->cr3;
    } else {
        /* Release lock to avoid deadlock during TLB shootdowns or heavy VM activity */
        spin_unlock(&sched_lock);
        __asm__ volatile("sti");

        uint64_t child_cr3 = vmm_clone_pml4(1);

        __asm__ volatile("cli");
        spin_lock(&sched_lock);
        tasks[slot].cr3 = child_cr3;
    }

    strlcpy(tasks[slot].name, parent->name, sizeof(tasks[slot].name));
    strlcpy(tasks[slot].executable_path, parent->executable_path, sizeof(tasks[slot].executable_path));

    if (clone_flags & CLONE_FILES) {
        tasks[slot].fd_table = parent->fd_table;
    } else {
        tasks[slot].fd_table = tasks[slot].fd_table_internal;
        memcpy(tasks[slot].fd_table_internal, parent->fd_table, sizeof(parent->fd_table_internal));
        for (int i = 0; i < MAX_FD; i++) {
            if (tasks[slot].fd_table[i].node) {
                __atomic_fetch_add(&tasks[slot].fd_table[i].node->ref_count, 1, __ATOMIC_SEQ_CST);
            }
        }
    }

    strlcpy(tasks[slot].cwd, parent->cwd, sizeof(tasks[slot].cwd));
    tasks[slot].cwd_node = parent->cwd_node;
    if (tasks[slot].cwd_node) __atomic_fetch_add(&tasks[slot].cwd_node->ref_count, 1, __ATOMIC_SEQ_CST);

    tasks[slot].signal_mask = parent->signal_mask;
    tasks[slot].signal_pending = 0;
    if (clone_flags & CLONE_SIGHAND) {
        tasks[slot].signal_handlers = parent->signal_handlers;
        tasks[slot].signal_restorers = parent->signal_restorers;
        tasks[slot].signal_flags = parent->signal_flags;
        tasks[slot].signal_action_masks = parent->signal_action_masks;
    } else {
        tasks[slot].signal_handlers = (void (**)(int))tasks[slot].signal_handlers_internal;
        tasks[slot].signal_restorers = (void (**)(void))tasks[slot].signal_restorers_internal;
        tasks[slot].signal_flags = tasks[slot].signal_flags_internal;
        tasks[slot].signal_action_masks = tasks[slot].signal_action_masks_internal;
        memcpy(tasks[slot].signal_handlers_internal, parent->signal_handlers, sizeof(parent->signal_handlers_internal));
        memcpy(tasks[slot].signal_restorers_internal, parent->signal_restorers, sizeof(parent->signal_restorers_internal));
        memcpy(tasks[slot].signal_flags_internal, parent->signal_flags, sizeof(parent->signal_flags_internal));
        memcpy(tasks[slot].signal_action_masks_internal, parent->signal_action_masks, sizeof(parent->signal_action_masks_internal));
    }
    if (clone_flags & CLONE_VM) {
        /* Linux compatibility: clear altstack for CLONE_VM (thread-like clone). */
        tasks[slot].sigaltstack_sp = 0;
        tasks[slot].sigaltstack_size = 0;
        tasks[slot].sigaltstack_flags = 2; /* SS_DISABLE */
    } else {
        tasks[slot].sigaltstack_sp = parent->sigaltstack_sp;
        tasks[slot].sigaltstack_size = parent->sigaltstack_size;
        tasks[slot].sigaltstack_flags = parent->sigaltstack_flags & ~1U; /* Clear SS_ONSTACK */
    }

    tasks[slot].brk = parent->brk;

    if (clone_flags & CLONE_SETTLS) {
        tasks[slot].fs_base = tls;
    } else {
        tasks[slot].fs_base = parent->fs_base;
    }

    tasks[slot].gs_base = parent->gs_base;
    tasks[slot].os_abi = parent->os_abi;
    tasks[slot].uid = parent->uid;
    tasks[slot].gid = parent->gid;
    tasks[slot].euid = parent->euid;
    tasks[slot].egid = parent->egid;
    tasks[slot].user_rsp = stack_top ? stack_top : parent->user_rsp;
    tasks[slot].mmap_base = parent->mmap_base;
    tasks[slot].wait_status = 0;
    tasks[slot].wait_code = 0;
    tasks[slot].wait_pending = 0;

    /* Apply TID logic securely */
    if (clone_flags & CLONE_PARENT_SETTID) {
        if (parent_tid) {
            if (vmm_verify_user_access(parent_tid, sizeof(uint32_t), 1)) {
                 *parent_tid = tasks[slot].id;
            }
        }
    }
    if (clone_flags & CLONE_CHILD_SETTID) {
        if (child_tid) {
             /* Handled by user space, but can be done here. */
        }
    }

    /* 6. Setup Child Stack for Return */
    uint64_t* sp = (uint64_t*)tasks[slot].kernel_stack_top;

    /* Push syscall_regs */
    sp = (uint64_t*)((uint8_t*)sp - sizeof(struct syscall_regs));
    memcpy(sp, regs, sizeof(struct syscall_regs));

    /* Child returns 0 */
    ((struct syscall_regs*)sp)->rax = 0;

    *(--sp) = (uint64_t)fork_return; /* RIP for ret */
    *(--sp) = 0; /* rbp */
    *(--sp) = 0; /* rbx */
    *(--sp) = 0; /* r12 */
    *(--sp) = 0; /* r13 */
    *(--sp) = 0; /* r14 */
    *(--sp) = 0; /* r15 */

    tasks[slot].rsp = (uint64_t)sp;

    if (slot >= task_count) task_count = slot + 1;

    enqueue_ready(&tasks[slot]);
    spin_unlock(&sched_lock);
    __asm__ volatile("sti");

    return tasks[slot].id;
}

static int task_is_wait_target(task_t* current, task_t* t, int pid, int options) {
    int is_child = (t->parent_id == current->id);
    int is_thread = (t->tgid == current->tgid);
    if (!(is_child || ((options & __WALL) && is_thread) || ((options & __WALL) && t->tgid == current->id))) {
        return 0;
    }
    if (pid == -1) return 1;
    if ((int)t->id == pid || (int)t->tgid == pid) return 1;
    if (pid < -1 && (int)t->tgid == -pid) return 1;
    return 0;
}

static void task_reap_zombie_locked(int zombie_slot) {
    task_t* zombie = &tasks[zombie_slot];
    if (zombie->fd_table == zombie->fd_table_internal) {
        for (int j=0; j<MAX_FD; j++) {
            if (zombie->fd_table[j].node) {
                fs_node_t* node = zombie->fd_table[j].node;
                if (__atomic_sub_fetch(&node->ref_count, 1, __ATOMIC_SEQ_CST) == 0) {
                    close_fs(node);
                    kfree(node);
                }
                zombie->fd_table[j].node = NULL;
            }
        }
    }

    /* Free CR3 if it's not kernel's and no other thread shares it */
    if (zombie->cr3 != tasks[0].cr3) {
        int shared_cr3 = 0;
        for (int k = 0; k < MAX_TASKS; k++) {
            if (k != zombie_slot && tasks[k].id != 0 && tasks[k].state != 0 && tasks[k].state != TASK_DEAD) {
                if (tasks[k].cr3 == zombie->cr3) {
                    shared_cr3 = 1;
                    break;
                }
            }
        }
        if (!shared_cr3) {
            vmm_destroy_pml4(zombie->cr3);
        }
    }

    zombie->id = 0;
    zombie->state = 0; /* Unused */
    zombie->name[0] = 0;
    zombie->wait_pending = 0;
    zombie->wait_code = 0;
    zombie->wait_status = 0;
}

int task_waitpid(int pid, int* status, int options) {
    task_t* current = task_get_current();
    if (!current) return -1;

    while (1) {
        int found_child = 0;
        int found_event = 0;
        int event_pid = 0;
        int event_status = 0;
        int event_slot = -1;

        __asm__ volatile("cli");
        spin_lock(&sched_lock);

        for (int i = 0; i < MAX_TASKS; i++) {
             if (tasks[i].id == 0 && tasks[i].state == 0) continue; /* Empty slot */
             if (tasks[i].id == current->id) continue; /* Self */

             if (!task_is_wait_target(current, &tasks[i], pid, options)) continue;
             found_child = 1;
             if (!tasks[i].wait_pending) continue;

             if (tasks[i].state == TASK_DEAD) {
                 found_event = 1;
             } else if (tasks[i].wait_code == CLD_STOPPED) {
                 if (options & WUNTRACED) found_event = 1;
             } else if (tasks[i].wait_code == CLD_CONTINUED) {
                 if (options & WCONTINUED) found_event = 1;
             }

             if (found_event) {
                 event_pid = tasks[i].id;
                 event_status = tasks[i].wait_status;
                 event_slot = i;
                 break;
             }
        }

        if (found_event) {
             task_t* ev = &tasks[event_slot];
             int is_zombie = (ev->state == TASK_DEAD);
             int is_nowait = (options & WNOWAIT);
             if (!is_nowait) {
                 if (is_zombie) task_reap_zombie_locked(event_slot);
                 else {
                     ev->wait_pending = 0;
                     ev->wait_code = 0;
                 }
             }

             spin_unlock(&sched_lock);
             __asm__ volatile("sti");

             if (status) {
                 *status = event_status;
             }
             return event_pid;
        }

        if (!found_child) {
             spin_unlock(&sched_lock);
             __asm__ volatile("sti");
             /* ECHILD = 10 */
             return -10;
        }

        if (options & WNOHANG) {
             spin_unlock(&sched_lock);
             __asm__ volatile("sti");
             return 0;
        }

        spin_unlock(&sched_lock);
        __asm__ volatile("sti");

        task_sleep(100); /* Poll every 100ms */
    }
}

int task_waitid(int idtype, int id, int options, int* out_pid, int* out_status, int* out_code) {
    task_t* current = task_get_current();
    if (!current) return -1;

    if (!(options & (WEXITED | WSTOPPED | WCONTINUED))) return -22; /* -EINVAL */

    while (1) {
        int found_child = 0;
        int found_event = 0;
        int event_slot = -1;
        int event_pid = 0;
        int event_status = 0;
        int event_code = 0;

        __asm__ volatile("cli");
        spin_lock(&sched_lock);

        for (int i = 0; i < MAX_TASKS; i++) {
            task_t* t = &tasks[i];
            if (t->id == 0 && t->state == 0) continue;
            if (t->id == current->id) continue;

            int is_child = (t->parent_id == current->id);
            int is_thread = (t->tgid == current->tgid);
            if (!(is_child || ((options & __WALL) && (is_thread || t->tgid == current->id)))) continue;

            if (idtype == P_PID) {
                if ((int)t->id != id && (int)t->tgid != id) continue;
            } else if (idtype == P_PGID) {
                int wanted = id ? id : (int)current->pgid;
                if ((int)t->pgid != wanted) continue;
            } else if (idtype != P_ALL) {
                spin_unlock(&sched_lock);
                __asm__ volatile("sti");
                return -22; /* -EINVAL */
            }

            found_child = 1;
            if (!t->wait_pending) continue;

            if (t->state == TASK_DEAD) {
                if (options & WEXITED) found_event = 1;
            } else if (t->wait_code == CLD_STOPPED) {
                if (options & WSTOPPED) found_event = 1;
            } else if (t->wait_code == CLD_CONTINUED) {
                if (options & WCONTINUED) found_event = 1;
            }

            if (found_event) {
                event_slot = i;
                event_pid = t->id;
                event_status = t->wait_status;
                event_code = t->wait_code;
                break;
            }
        }

        if (found_event) {
            task_t* ev = &tasks[event_slot];
            if (!(options & WNOWAIT)) {
                if (ev->state == TASK_DEAD) task_reap_zombie_locked(event_slot);
                else {
                    ev->wait_pending = 0;
                    ev->wait_code = 0;
                }
            }
            spin_unlock(&sched_lock);
            __asm__ volatile("sti");

            if (out_pid) *out_pid = event_pid;
            if (out_status) *out_status = event_status;
            if (out_code) *out_code = event_code;
            return 0;
        }

        if (!found_child) {
            spin_unlock(&sched_lock);
            __asm__ volatile("sti");
            return -10; /* -ECHILD */
        }

        if (options & WNOHANG) {
            spin_unlock(&sched_lock);
            __asm__ volatile("sti");
            if (out_pid) *out_pid = 0;
            if (out_status) *out_status = 0;
            if (out_code) *out_code = 0;
            return 0;
        }

        spin_unlock(&sched_lock);
        __asm__ volatile("sti");
        task_sleep(100);
    }
}

static int read_shebang_line(const char* path, char* interp, size_t interp_sz, char* interp_arg, size_t interp_arg_sz, char* abs_script, size_t abs_script_sz) {
    task_t* current = task_get_current();
    fs_node_t* node;
    char abs_path[256];
    uint8_t head[256];
    ssize_t nread;
    size_t i = 0;
    size_t j = 0;
    size_t k = 0;

    if (!path || !interp || !interp_arg || !abs_script || !current) return -EINVAL;

    interp[0] = '\0';
    interp_arg[0] = '\0';
    abs_script[0] = '\0';

    if (vfs_normalize_path(abs_path, sizeof(abs_path), path, current->cwd) != 0) {
        return -ENOENT;
    }
    strlcpy(abs_script, abs_path, abs_script_sz);

    node = vfs_lookup(fs_root, abs_path);
    if (!node) {
        return -ENOENT;
    }
    if ((node->flags & 0x7) != FS_FILE) {
        if (__atomic_sub_fetch(&node->ref_count, 1, __ATOMIC_SEQ_CST) == 0) kfree(node);
        return -ENOEXEC;
    }

    nread = read_fs(node, 0, sizeof(head) - 1, head);
    if (__atomic_sub_fetch(&node->ref_count, 1, __ATOMIC_SEQ_CST) == 0) kfree(node);
    if (nread < 2) return 0;

    head[nread] = '\0';
    if (!(head[0] == '#' && head[1] == '!')) return 0;

    i = 2;
    while (i < (size_t)nread && (head[i] == ' ' || head[i] == '\t')) i++;
    if (i >= (size_t)nread || head[i] == '\n' || head[i] == '\r') return -ENOEXEC;

    while (i < (size_t)nread && head[i] != ' ' && head[i] != '\t' && head[i] != '\n' && head[i] != '\r') {
        if (j + 1 >= interp_sz) return -E2BIG;
        interp[j++] = (char)head[i++];
    }
    interp[j] = '\0';
    if (j == 0) return -ENOEXEC;

    while (i < (size_t)nread && (head[i] == ' ' || head[i] == '\t')) i++;
    while (i < (size_t)nread && head[i] != '\n' && head[i] != '\r') {
        if (k + 1 >= interp_arg_sz) return -E2BIG;
        interp_arg[k++] = (char)head[i++];
    }
    while (k > 0 && (interp_arg[k - 1] == ' ' || interp_arg[k - 1] == '\t')) k--;
    interp_arg[k] = '\0';

    return 1;
}

int task_exec(const char* path, char* const argv[], char* const envp[], struct syscall_regs* regs) {
    int res = 0;
    int argc = 0;
    int envc = 0;
    char* kargv[33];
    char* kenvp[33];
    int shebang = 0;
    char shebang_interp[256];
    char shebang_interp_arg[128];
    char shebang_script_abs[256];

    /* 1. Validate */
    char* kpath = (char*)kmalloc(256);
    if (!kpath) return -ENOMEM;
    res = vmm_strncpy_from_user(kpath, path, 256);
    if (res < 0) {
        kfree(kpath);
        return res;
    }

    if (argv) {
        if (!vmm_verify_user_access(argv, sizeof(char*), 0)) {
            res = -EFAULT;
            goto cleanup_error;
        }
        for (int i=0; i<32; i++) {
            if (!vmm_verify_user_access(&argv[i], sizeof(char*), 0)) {
                res = -EFAULT;
                goto cleanup_error;
            }
            char* ptr = argv[i];
            if (!ptr) break;

            kargv[argc] = (char*)kmalloc(128);
            if (!kargv[argc]) {
                res = -ENOMEM;
                goto cleanup_error;
            }
            res = vmm_strncpy_from_user(kargv[argc], ptr, 128);
            if (res < 0) {
                kfree(kargv[argc]);
                goto cleanup_error;
            }
            argc++;
        }
    }
    kargv[argc] = NULL;

    shebang = read_shebang_line(kpath, shebang_interp, sizeof(shebang_interp), shebang_interp_arg, sizeof(shebang_interp_arg), shebang_script_abs, sizeof(shebang_script_abs));
    if (shebang < 0 && shebang != -ENOEXEC) {
        res = shebang;
        goto cleanup_error;
    }
    if (shebang > 0) {
        char* old_argv[33];
        char* new_argv[33];
        int old_argc = argc;
        int new_argc = 0;
        int extra = (shebang_interp_arg[0] != '\0') ? 1 : 0;
        int required = 2 + extra + ((old_argc > 0) ? (old_argc - 1) : 0);

        if (required > 32) {
            res = -E2BIG;
            goto cleanup_error;
        }

        for (int i = 0; i <= old_argc; i++) old_argv[i] = kargv[i];
        for (int i = 0; i < 33; i++) new_argv[i] = NULL;

        new_argv[new_argc] = (char*)kmalloc(256);
        if (!new_argv[new_argc]) {
            res = -ENOMEM;
            goto cleanup_error;
        }
        strlcpy(new_argv[new_argc++], shebang_interp, 256);

        if (shebang_interp_arg[0] != '\0') {
            new_argv[new_argc] = (char*)kmalloc(128);
            if (!new_argv[new_argc]) {
                kfree(new_argv[0]);
                res = -ENOMEM;
                goto cleanup_error;
            }
            strlcpy(new_argv[new_argc++], shebang_interp_arg, 128);
        }

        new_argv[new_argc] = (char*)kmalloc(256);
        if (!new_argv[new_argc]) {
            for (int i = 0; i < new_argc; i++) if (new_argv[i]) kfree(new_argv[i]);
            res = -ENOMEM;
            goto cleanup_error;
        }
        strlcpy(new_argv[new_argc++], shebang_script_abs, 256);

        for (int i = 1; i < old_argc; i++) {
            new_argv[new_argc++] = old_argv[i];
        }
        new_argv[new_argc] = NULL;

        if (old_argc > 0 && old_argv[0]) {
            kfree(old_argv[0]);
        }

        for (int i = 0; i <= new_argc; i++) {
            kargv[i] = new_argv[i];
        }
        argc = new_argc;
        strlcpy(kpath, shebang_interp, 256);
    }

    if (envp) {
        if (!vmm_verify_user_access(envp, sizeof(char*), 0)) {
            res = -EFAULT;
            goto cleanup_error;
        }
        for (int i=0; i<32; i++) {
            if (!vmm_verify_user_access(&envp[i], sizeof(char*), 0)) {
                res = -EFAULT;
                goto cleanup_error;
            }
            char* ptr = envp[i];
            if (!ptr) break;

            kenvp[envc] = (char*)kmalloc(128);
            if (!kenvp[envc]) {
                res = -ENOMEM;
                goto cleanup_error;
            }
            res = vmm_strncpy_from_user(kenvp[envc], ptr, 128);
            if (res < 0) {
                kfree(kenvp[envc]);
                goto cleanup_error;
            }
            envc++;
        }
    }
    kenvp[envc] = NULL;

    {
        int skip_pt_interp = 0;
        for (int i = 0; i < envc; i++) {
            if (strcmp(kenvp[i], "ETER_SKIP_PT_INTERP=1") == 0) {
                skip_pt_interp = 1;
                break;
            }
        }

        if (!skip_pt_interp) {
            char elf_interp[256];
            int interp_res = elf_get_interp(kpath, elf_interp, sizeof(elf_interp));
            if (interp_res < 0 && interp_res != -ENOEXEC) {
                res = interp_res;
                goto cleanup_error;
            }
            if (interp_res > 0) {
                char exec_abs[256];
                char* old_argv[33];
                char* new_argv[33];
                int old_argc = argc;
                int new_argc = 0;
                int required = 2 + ((old_argc > 0) ? (old_argc - 1) : 0);
                task_t* exec_current = task_get_current();

                if (!exec_current || vfs_normalize_path(exec_abs, sizeof(exec_abs), kpath, exec_current->cwd) != 0) {
                    strlcpy(exec_abs, kpath, sizeof(exec_abs));
                }

                if (required > 32) {
                    res = -E2BIG;
                    goto cleanup_error;
                }

                for (int i = 0; i <= old_argc; i++) old_argv[i] = kargv[i];
                for (int i = 0; i < 33; i++) new_argv[i] = NULL;

                new_argv[new_argc] = (char*)kmalloc(256);
                if (!new_argv[new_argc]) {
                    res = -ENOMEM;
                    goto cleanup_error;
                }
                strlcpy(new_argv[new_argc++], elf_interp, 256);

                new_argv[new_argc] = (char*)kmalloc(256);
                if (!new_argv[new_argc]) {
                    kfree(new_argv[0]);
                    res = -ENOMEM;
                    goto cleanup_error;
                }
                strlcpy(new_argv[new_argc++], exec_abs, 256);

                for (int i = 1; i < old_argc; i++) {
                    new_argv[new_argc++] = old_argv[i];
                }
                new_argv[new_argc] = NULL;

                if (old_argc > 0 && old_argv[0]) {
                    kfree(old_argv[0]);
                }

                for (int i = 0; i <= new_argc; i++) {
                    kargv[i] = new_argv[i];
                }
                argc = new_argc;
                strlcpy(kpath, elf_interp, 256);

                serial_write_string("[ELF] PT_INTERP detected. Delegating to interpreter: ");
                serial_write_string(kpath);
                serial_write_string("\n");
            }
        }
    }

    /* 2. New Address Space */
    task_t* current = task_get_current();
    elf_auxv_info_t aux_info;
    uint64_t old_cr3 = current->cr3;

    memset(&aux_info, 0, sizeof(aux_info));
    {
        int aux_info_rc = elf_get_auxv_info(kpath, 0, &aux_info);
        if (aux_info_rc < 0) {
            res = aux_info_rc;
            goto cleanup_error;
        }
    }

    uint64_t new_cr3_phys = (uint64_t)pmm_alloc_page();
    if (!new_cr3_phys) {
        res = -ENOMEM;
        goto cleanup_error;
    }
    uint64_t* new_pml4 = (uint64_t*)new_cr3_phys;
    memset(new_pml4, 0, PAGE_SIZE);

    /* Copy Kernel Mappings (Higher Half) */
    uint64_t* kernel_pml4 = (uint64_t*)tasks[0].cr3;
    for (int i = 256; i < 512; i++) {
        new_pml4[i] = kernel_pml4[i];
    }

    /* Copy Identity Map (Index 0) - Need deep copy of PDPT to separate User Space */
    uint64_t new_pdpt_phys = (uint64_t)pmm_alloc_page();
    if (!new_pdpt_phys) {
        res = -ENOMEM;
        pmm_free_page((void*)new_cr3_phys);
        goto cleanup_error;
    }
    uint64_t* new_pdpt = (uint64_t*)new_pdpt_phys;
    memset(new_pdpt, 0, PAGE_SIZE);

    uint64_t kernel_pdpt_phys = kernel_pml4[0] & PAGE_ADDR_MASK;
    uint64_t* kernel_pdpt = (uint64_t*)kernel_pdpt_phys;

    /* Copy first 8 entries (Identity Map 0-8GB) */
    for (int i = 0; i < 8; i++) {
        new_pdpt[i] = kernel_pdpt[i];
    }

    /* Link PDPT to PML4[0] */
    new_pml4[0] = new_pdpt_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;

    /* 3. Switch CR3 */
    __asm__ volatile("mov %0, %%cr3" : : "r"(new_cr3_phys) : "memory");
    current->cr3 = new_cr3_phys;

    /* 4. Load ELF */
    current->brk = 0;
    current->mmap_base = 0x700000000000ULL;

    uint64_t entry_point = elf_load_file(kpath, 0);
    if (entry_point == 0) {
        /* Failed: Restore old CR3 and abort */
        __asm__ volatile("mov %0, %%cr3" : : "r"(old_cr3) : "memory");
        current->cr3 = old_cr3;

        /* Destroy the partially created address space to prevent memory leak */
        vmm_destroy_pml4(new_cr3_phys);

        res = -ENOEXEC;
        goto cleanup_error;
    }

    /* Close O_CLOEXEC descriptors on exec */
    /* Since exec unshares FD table if shared, we should unshare here but for now just clear them locally */
    if (current->fd_table != current->fd_table_internal) {
        /* Make a copy to unshare on exec as exec replaces address space and we shouldn't affect other threads */
        memcpy(current->fd_table_internal, current->fd_table, sizeof(current->fd_table_internal));
        current->fd_table = current->fd_table_internal;
    }

    for (int i = 0; i < MAX_FD; i++) {
        if (current->fd_table[i].node && (current->fd_table[i].flags & O_CLOEXEC)) {
            if (__atomic_sub_fetch(&current->fd_table[i].node->ref_count, 1, __ATOMIC_SEQ_CST) == 0) {
                close_fs(current->fd_table[i].node);
                kfree(current->fd_table[i].node);
            }
            current->fd_table[i].node = NULL;
        }
    }

    /* 5. Setup Stack */
    /* Keep stack page-aligned and fully mapped before writing user args/env. */
    const uint64_t stack_size = 128 * 1024;
    uint64_t stack_top = (USER_LIMIT + 1ULL) & ~(PAGE_SIZE - 1ULL); /* Exclusive top */
    uint64_t stack_base = stack_top - stack_size;

    for (uint64_t addr = stack_base; addr < stack_top; addr += PAGE_SIZE) {
        void* phys = pmm_alloc_page();
        if (!phys) {
            __asm__ volatile("mov %0, %%cr3" : : "r"(old_cr3) : "memory");
            current->cr3 = old_cr3;
            vmm_destroy_pml4(new_cr3_phys);
            res = -ENOMEM;
            goto cleanup_error;
        }
        if (vmm_map_page((uint64_t)phys, addr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER) < 0) {
            pmm_free_page(phys);
            __asm__ volatile("mov %0, %%cr3" : : "r"(old_cr3) : "memory");
            current->cr3 = old_cr3;
            vmm_destroy_pml4(new_cr3_phys);
            res = -ENOMEM;
            goto cleanup_error;
        }
        memset((void*)addr, 0, PAGE_SIZE);
    }

    /* Push strings to top of stack */
    uint64_t rsp = stack_top;

    uint64_t uargv[33];
    uint64_t uenvp[33];

    /* Push Envp Strings */
    for (int i = envc - 1; i >= 0; i--) {
        int len = strlen(kenvp[i]) + 1;
        rsp -= len;
        if (rsp < stack_base) {
            __asm__ volatile("mov %0, %%cr3" : : "r"(old_cr3) : "memory");
            current->cr3 = old_cr3;
            vmm_destroy_pml4(new_cr3_phys);
            res = -E2BIG;
            goto cleanup_error;
        }
        memcpy((void*)rsp, kenvp[i], len);
        uenvp[i] = rsp;
    }
    uenvp[envc] = 0;

    /* Push Argv Strings */
    for (int i = argc - 1; i >= 0; i--) {
        int len = strlen(kargv[i]) + 1;
        rsp -= len;
        if (rsp < stack_base) {
            __asm__ volatile("mov %0, %%cr3" : : "r"(old_cr3) : "memory");
            current->cr3 = old_cr3;
            vmm_destroy_pml4(new_cr3_phys);
            res = -E2BIG;
            goto cleanup_error;
        }
        memcpy((void*)rsp, kargv[i], len);
        uargv[i] = rsp;
    }
    uargv[argc] = 0;

    /* Align Stack (16 bytes) */
    rsp &= ~0xF;

    /* Build and Push AUXV (minimal Linux-compatible set). */
    struct {
        uint64_t a_type;
        uint64_t a_val;
    } auxv[12];
    int auxc = 0;

    auxv[auxc].a_type = AT_PAGESZ;
    auxv[auxc++].a_val = PAGE_SIZE;

    auxv[auxc].a_type = AT_ENTRY;
    auxv[auxc++].a_val = aux_info.entry ? aux_info.entry : entry_point;

    auxv[auxc].a_type = AT_UID;
    auxv[auxc++].a_val = current->uid;

    auxv[auxc].a_type = AT_EUID;
    auxv[auxc++].a_val = current->euid;

    auxv[auxc].a_type = AT_GID;
    auxv[auxc++].a_val = current->gid;

    auxv[auxc].a_type = AT_EGID;
    auxv[auxc++].a_val = current->egid;

    auxv[auxc].a_type = AT_SECURE;
    auxv[auxc++].a_val = 0;

    auxv[auxc].a_type = AT_PHDR;
    auxv[auxc++].a_val = aux_info.phdr;

    auxv[auxc].a_type = AT_PHENT;
    auxv[auxc++].a_val = aux_info.phent;

    auxv[auxc].a_type = AT_PHNUM;
    auxv[auxc++].a_val = aux_info.phnum;

    auxv[auxc].a_type = AT_BASE;
    auxv[auxc++].a_val = aux_info.base;

    auxv[auxc].a_type = AT_NULL;
    auxv[auxc++].a_val = 0;

    rsp -= (uint64_t)auxc * 16ULL;
    if (rsp < stack_base) {
        __asm__ volatile("mov %0, %%cr3" : : "r"(old_cr3) : "memory");
        current->cr3 = old_cr3;
        vmm_destroy_pml4(new_cr3_phys);
        res = -E2BIG;
        goto cleanup_error;
    }
    memcpy((void*)rsp, auxv, (size_t)auxc * 16U);

    /* Push Envp Pointers */
    rsp -= (envc + 1) * 8;
    for (int i = 0; i <= envc; i++) {
        ((uint64_t*)rsp)[i] = uenvp[i];
    }

    /* Push Argv Pointers */
    rsp -= (argc + 1) * 8;
    for (int i = 0; i <= argc; i++) {
        ((uint64_t*)rsp)[i] = uargv[i];
    }

    /* Push Argc */
    rsp -= 8;
    if (rsp < stack_base) {
        __asm__ volatile("mov %0, %%cr3" : : "r"(old_cr3) : "memory");
        current->cr3 = old_cr3;
        vmm_destroy_pml4(new_cr3_phys);
        res = -E2BIG;
        goto cleanup_error;
    }
    *(uint64_t*)rsp = (uint64_t)argc;

    /* 6. Cleanup Kernel Buffers */
    for (int i=0; i<argc; i++) kfree(kargv[i]);
    for (int i=0; i<envc; i++) kfree(kenvp[i]);

    /* 7. Update Regs */
    regs->rcx = entry_point;

    /* Update User Stack in Per-CPU */
    cpu_info_t* cpu = get_current_cpu();
    if (cpu) {
         cpu->user_stack_scratch = rsp;
    }
    current->user_rsp = rsp;

    /* Update Name */
    strlcpy(current->name, kpath, 32);

    // Resolve absolute path
    if (kpath[0] == '/') {
        strlcpy(current->executable_path, kpath, sizeof(current->executable_path));
    } else {
        strlcpy(current->executable_path, current->cwd, sizeof(current->executable_path));
        if (current->executable_path[strlen(current->executable_path) - 1] != '/') {
            strlcat(current->executable_path, "/", sizeof(current->executable_path));
        }
        strlcat(current->executable_path, kpath, sizeof(current->executable_path));
    }

    /* 8. Destroy Old Address Space */
    vmm_destroy_pml4(old_cr3);

    kfree(kpath);
    return 0;

cleanup_error:
    kfree(kpath);
    for (int i=0; i<argc; i++) kfree(kargv[i]);
    for (int i=0; i<envc; i++) kfree(kenvp[i]);
    return res;
}
