/*
 * smp.c - Symmetric Multi-Processing Initialization
 *
 * Handles per-CPU initialization, AP startup, and topography management.
 */

#include <cpu.h>
#include <msr.h>
#include <string.h>
#include <serial.h>
#include <hal.h>
#include <mm.h>
#include <apic.h>
#include <vmm.h>
#include <task.h>
#include <gdt.h>
#include <idt.h>

/* Symbols from trampoline.asm */
extern char trampoline_start[];
extern char trampoline_end[];

/* Variables inside trampoline to be patched */
#define TRAMPOLINE_BASE 0x8000
#define TRAMPOLINE_OFFSET(x) ((uint64_t)(x) - (uint64_t)trampoline_start)

extern void syscall_init(void);

void cpu_init_bsp(void) {
    if (total_cpus == 0) {
        total_cpus = 1;
        cpus[0].apic_id = lapic_get_id(); /* Use real ID */
        cpus[0].index = 0;
        cpus[0].state = CPU_STATE_ONLINE;
    }

    for (int i = 0; i < total_cpus; i++) {
        cpus[i].self = (uint64_t)&cpus[i];
    }

    cpu_info_t* bsp = &cpus[0];
    wrmsr(MSR_GS_BASE, (uint64_t)bsp);
    wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)bsp);
    
    bsp->kernel_stack_top = 0x90000; 
    
    serial_write_string("[SMP] BSP (CPU 0) Initialized. GS_BASE set.\n");
}

/* Common GDT 32-bit for Trampoline */
static uint64_t temp_gdt[] = {
    0x0000000000000000, /* Null */
    0x00cf9a000000ffff, /* Code 32 */
    0x00cf92000000ffff  /* Data 32 */
};

extern struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr;

void smp_init(void) {
    if (total_cpus <= 1) {
        serial_write_string("[SMP] Single CPU system detected, skipping SMP boot.\n");
        return;
    }

    serial_write_string("[SMP] Booting Application Processors...\n");

    /* 1. Preparar el trampolín */
    uint64_t len = (uint64_t)trampoline_end - (uint64_t)trampoline_start;
    memcpy((void*)TRAMPOLINE_BASE, trampoline_start, len);

    /* 2. Parchear valores en el trampolín */
    extern char trampoline_gdt_ptr[];
    extern char trampoline_gdt64_ptr[];
    extern char trampoline_pml4[];
    extern char trampoline_stack[];
    extern char trampoline_entry[];
    extern char trampoline_cpu_index[];

    /* GDT 32 ptr */
    *(uint16_t*)(TRAMPOLINE_BASE + TRAMPOLINE_OFFSET(trampoline_gdt_ptr)) = sizeof(temp_gdt) - 1;
    *(uint32_t*)(TRAMPOLINE_BASE + TRAMPOLINE_OFFSET(trampoline_gdt_ptr) + 2) = (uint32_t)((uintptr_t)temp_gdt);

    /* GDT 64 ptr (Reuse the system GDT) */
    *(uint16_t*)(TRAMPOLINE_BASE + TRAMPOLINE_OFFSET(trampoline_gdt64_ptr)) = gdtr.limit;
    *(uint64_t*)(TRAMPOLINE_BASE + TRAMPOLINE_OFFSET(trampoline_gdt64_ptr) + 2) = gdtr.base;

    /* PML4 */
    *(uint32_t*)(TRAMPOLINE_BASE + TRAMPOLINE_OFFSET(trampoline_pml4)) = (uint32_t)vmm_get_pml4();

    /* Entry Point */
    extern void cpu_init_ap_wrapper(int index);
    *(uint64_t*)(TRAMPOLINE_BASE + TRAMPOLINE_OFFSET(trampoline_entry)) = (uint64_t)cpu_init_ap_wrapper;

    /* 3. Despertar cada AP */
    for (int i = 1; i < total_cpus; i++) {
        cpu_info_t* cpu = &cpus[i];
        cpu->self = (uint64_t)cpu;
        cpu->index = i;
        
        serial_write_string("[SMP] Booting CPU ");
        char b[16]; itoa_s(i, b, 16, 10); serial_write_string(b);
        serial_write_string(" (APIC ID: ");
        itoa_s(cpu->apic_id, b, 16, 10); serial_write_string(b);
        serial_write_string("\n");

        /* Preparar stack para este CPU */
        void* stack = kmalloc(16384); /* Increased to 16KB for safety */
        if (!stack) {
            serial_write_string("[SMP] ERROR: OOM allocating stack for CPU ");
            serial_write_string(b);
            serial_write_string(". Skipping.\n");
            cpu->state = CPU_STATE_OFFLINE;
            continue;
        }
        cpu->kernel_stack_top = (uint64_t)stack + 16384;
        
        *(uint64_t*)(TRAMPOLINE_BASE + TRAMPOLINE_OFFSET(trampoline_stack)) = cpu->kernel_stack_top;
        *(uint32_t*)(TRAMPOLINE_BASE + TRAMPOLINE_OFFSET(trampoline_cpu_index)) = i;

        /* Initialize Per-CPU GDT/TSS */
        gdt_init_ap(cpu);

        cpu->state = CPU_STATE_BOOTING;

        /* INIT IPI */
        lapic_send_init(cpu->apic_id);
        
        /* STARTUP IPI (Vector 0x08 -> 0x8000) */
        lapic_send_startup(cpu->apic_id, 0x08);

        /* Esperar a que el AP se marque como ONLINE */
        /* Bajo virtualizacion (sin aceleracion VT-x estable), la calibracion del timer
           en el AP puede tardar mucho mas de lo previsto. 500M es ~0.5s a 1GHz */
        uint64_t timeout = 500000000; 
        while (cpu->state != CPU_STATE_ONLINE && timeout > 0) {
            timeout--;
            __asm__ volatile("pause");
        }

        if (cpu->state == CPU_STATE_ONLINE) {
            serial_write_string("[SMP] CPU ");
            serial_write_string(b);
            serial_write_string(" is ONLINE.\n");
        } else {
            serial_write_string("[SMP] CPU ");
            serial_write_string(b);
            serial_write_string(" FAILED to boot. Marking OFFLINE.\n");
            cpu->state = CPU_STATE_OFFLINE;
        }
    }
}

void cpu_init_ap(int index) {
    /* Deshabilitar interrupciones locales temporalmente */
    hal_interrupts_disable();
    
    cpu_info_t* cpu = &cpus[index];

    /* Load Per-CPU GDT/TSS */
    /* This will reset GS Base to 0, so we must do it BEFORE setting MSR_GS_BASE */
    gdt_load_for_cpu(cpu);

    /* Load IDT (shared table, already initialized by BSP) */
    idt_load_ap();
    
    /* Configurar GS Base */
    wrmsr(MSR_GS_BASE, (uint64_t)cpu);
    wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)cpu);

    /* Habilitar Syscalls en este AP (EFER.SCE, LSTAR, etc.) */
    syscall_init();

    /* Habilitar interrupciones locales del APIC */
    lapic_init();

    /* Inicializar timer del LAPIC (100 Hz) para Scheduling */
    lapic_timer_init(100);

    /* Inicializar scheduler local */
    task_init_ap();
    
    /* Habilitar interrupciones globales y esperar */
    hal_interrupts_enable();

    /* Señalizar que estamos listos SÓLO después de activar interrupciones,
       así el BSP no espera IPI ACKs de un CPU que tiene interrupts OFF. */
    cpu->state = CPU_STATE_ONLINE;

    /* Enter scheduler loop instead of simply halting. */
    for(;;) {
        task_yield();
        __asm__ volatile("hlt");
    }
}
