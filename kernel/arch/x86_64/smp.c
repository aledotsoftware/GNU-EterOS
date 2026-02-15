/*
 * smp.c - Symmetric Multi-Processing Initialization
 *
 * Handles per-CPU initialization, AP startup, and topography management.
 */

#include <cpu.h>
#include <msr.h>
#include <string.h>
#include <serial.h>
#include <mm.h>

void cpu_init_bsp(void) {
    if (total_cpus == 0) {
        /* Si ACPI falló o no detectó nada, asumimos 1 CPU (BSP) */
        total_cpus = 1;
        cpus[0].apic_id = 0; /* Asumimos ID 0 por defecto */
        cpus[0].index = 0;
        cpus[0].state = CPU_STATE_ONLINE;
    }

    cpu_info_t* bsp = &cpus[0];
    
    /* Configurar GS Base para apuntar a la estructura per-cpu */
    /* Esto permite usar get_current_cpu() */
    wrmsr(MSR_GS_BASE, (uint64_t)bsp);
    wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)bsp); /* SwapGS fallback */
    
    serial_write_string("[SMP] BSP (CPU 0) Initialized. GS_BASE set.\n");
    
    /* Verificar que get_current_cpu funciona */
    cpu_info_t* current = get_current_cpu();
    if (current == bsp) {
        serial_write_string("[SMP] Self-check OK: get_current_cpu() returns correct pointer.\n");
        // char buf[32];
        // itoa_s(current->apic_id, buf, 32, 10);
        // serial_write_string("[SMP] APIC ID: ");
        // serial_write_string(buf);
        // serial_write_string("\n");
    } else {
        serial_write_string("[SMP] CRITICAL ERROR: GS_BASE mismatch!\n");
    }
}

/* Esta función será llamada por cada AP cuando despierten */
void cpu_init_ap(void) {
    /* TODO: Fase 6 - Implementar inicialización de APs */
}
