#ifndef ETEROS_CPU_H
#define ETEROS_CPU_H

#include <types.h>

/* Configuración de Escala */
#define MAX_CPUS 256             /* Soporte para hasta 256 núcleos lógicos (x2APIC standard limit is higher but 256 fits in manageable bitmap) */
#define CPU_SET_SIZE (MAX_CPUS / 64) /* 4 uint64_t for 256 CPUs */

/* Bitmap de CPUs para afinidad y máscaras */
typedef struct {
    uint64_t bits[CPU_SET_SIZE];
} cpu_set_t;

/* Operaciones de CPU Set */
static inline void CPU_ZERO(cpu_set_t *set) {
    for (int i = 0; i < CPU_SET_SIZE; i++) set->bits[i] = 0;
}

static inline void CPU_SET(int cpu, cpu_set_t *set) {
    if (cpu < MAX_CPUS) set->bits[cpu / 64] |= (1ULL << (cpu % 64));
}

static inline void CPU_CLR(int cpu, cpu_set_t *set) {
    if (cpu < MAX_CPUS) set->bits[cpu / 64] &= ~(1ULL << (cpu % 64));
}

static inline int CPU_ISSET(int cpu, cpu_set_t *set) {
    if (cpu >= MAX_CPUS) return 0;
    return (set->bits[cpu / 64] & (1ULL << (cpu % 64))) != 0;
}

/* Estado del procesador */
typedef enum {
    CPU_STATE_OFFLINE = 0,
    CPU_STATE_BOOTING,
    CPU_STATE_ONLINE,
    CPU_STATE_HALTED
} cpu_state_t;

/* Estructura de Control por CPU (PCB - Processor Control Block) */
/* Esta estructura debe estar alineada a cache line (64 bytes) para evitar false sharing */
typedef struct {
    /* Auto-referencia para get_current_cpu() */
    uint64_t self;         /* Offset 0 - Obligatorio para mov gs:0 */
    
    /* Identificación */
    uint32_t apic_id;       /* ID real del hardware (incluye x2APIC 32-bit ID) */
    uint32_t acpi_id;       /* ID lógico en ACPI/MADT */
    uint32_t index;         /* Índice secuencial en el kernel (0..N) */
    
    /* Estado */
    volatile cpu_state_t state;
    
    /* Contexto de Ejecución */
    void* current_task;               /* Pointer to current task_t */
    void* idle_task;                  /* Pointer to idle task_t */
    uint32_t sched_ticks;             /* Per-CPU scheduler tick counter */

    /* Per-CPU GDT/TSS */
    void* gdt;                        /* struct gdt_entry* */
    void* tss;                        /* struct tss_struct* */
    
    /* Stack de Kernel (para syscall entry) MUST REMAIN AT OFFSET 64 and 72 */
    uint64_t kernel_stack_top;  /* RSP0 en TSS */
    uint64_t user_stack_scratch; /* Espacio temporal para swapgs */
    
    /* Scheduler Local */
    void* local_ready_head;
    void* local_ready_tail;
    int local_task_count;

    /* Estadísticas */
    uint64_t context_switches;
    uint64_t uptime_ticks;
    
    /* Pad to cache line size if needed */
} __attribute__((aligned(64))) cpu_info_t;

/* Acceso a datos per-cpu via GS (x86_64) */
/* Se asume que GS base apunta a la estructura cpu_info_t del núcleo actual */
static inline cpu_info_t* get_current_cpu(void) {
    cpu_info_t* cpu;
#ifdef __x86_64__
    /* En x86_64, usamos GS para almacenar el puntero a cpu_info_t */
    /* La instrucción SWAPGS intercambia GS base entre user y kernel */
    /* Aquí asumimos que estamos en modo kernel y GS es válido */
    __asm__ volatile("mov %%gs:0, %0" : "=r"(cpu)); 
    /* Nota: Esto depende de cómo configuremos el GDT/MSR_GS_BASE. 
       Alternativamente, si GS base ES la estructura, sería 'return (cpu_info_t*)0' relative to read via segment override?
       Normalmente se pone un puntero a self en el offset 0 de la estructura per-cpu. */
#else
    cpu = NULL; /* Fallback for non-SMP archs */
#endif
    return cpu;
}

static inline int get_cpu_id(void) {
    cpu_info_t* cpu = get_current_cpu();
    return cpu ? cpu->index : 0;
}


/* Variables Globales de Topología */
extern cpu_info_t cpus[MAX_CPUS];
extern int total_cpus;

/* Inicialización */
void cpu_init_bsp(void);        /* Inicializar estructuras para el Bootstrap Processor */
void cpu_init_ap(int index);    /* Inicializar Application Processor (llamado tras trampolín) */
void smp_init(void);            /* Despertar Application Processors via INIT-SIPI-SIPI */
void pat_init(void);            /* Inicializar Page Attribute Table (PAT) */

#endif /* ETEROS_CPU_H */
