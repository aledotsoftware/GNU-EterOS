/**
 * éterOS - GDT & TSS Implementation (x86_64)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Configura la Global Descriptor Table y el Task State Segment.
 * Es crítico en Long Mode tener un TSS válido para el manejo de interrupciones
 * (especialmente para el cambio de stack vía IST, aunque no lo usemos activamente).
 */

#include "../../../include/gdt.h"
#include "../../../include/string.h"
#include "../../../include/serial.h"
#include "../../../include/cpu.h"
#include "../../../include/mm.h"
#include "../../../include/msr.h"

/* ========================================================================= */
/* Estructuras de Datos (Internas)                                           */
/* ========================================================================= */

/* Entrada de la GDT (8 bytes) - Modo Largo */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

/* Descriptor de TSS (System Segment de 16 bytes en x86_64) */
struct tss_entry_struct {
    uint16_t length;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  flags1;
    uint8_t  flags2;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

/* Estructura del TSS en sí (Task State Segment) */
struct tss_struct {
    uint32_t reserved0;
    uint64_t rsp0;       /* Stack pointer para Ring 0 */
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];     /* Interrupt Stack Table */
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

/* Puntero GDTR */
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* ========================================================================= */
/* Variables Estáticas                                                       */
/* ========================================================================= */

#define GDT_ENTRIES 7

/* 
 * 0: Null
 * 1: Kernel Code
 * 2: Kernel Data
 * 3: User Data
 * 4: User Code
 * 5: TSS (Low)
 * 6: TSS (High)
 */
static struct gdt_entry gdt[GDT_ENTRIES] __attribute__((aligned(16)));
static struct tss_struct tss __attribute__((aligned(16)));
struct gdt_ptr gdtr;

/* Double Fault Stack */
#define DF_STACK_SIZE 4096
static uint8_t df_stack_bsp[DF_STACK_SIZE] __attribute__((aligned(16)));

/* Funciones en ASM */
extern void gdt_flush(uint64_t gdtr_addr);
extern void tss_flush(void);


/* ========================================================================= */
/* Implementación                                                            */
/* ========================================================================= */

static void gdt_set_gate(int num, uint64_t base, uint64_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access      = access;
}

static void gdt_set_gate_ptr(struct gdt_entry* gdt_base, int num, uint64_t base, uint64_t limit, uint8_t access, uint8_t gran) {
    gdt_base[num].base_low    = (base & 0xFFFF);
    gdt_base[num].base_middle = (base >> 16) & 0xFF;
    gdt_base[num].base_high   = (base >> 24) & 0xFF;

    gdt_base[num].limit_low   = (limit & 0xFFFF);
    gdt_base[num].granularity = (limit >> 16) & 0x0F;

    gdt_base[num].granularity |= (gran & 0xF0);
    gdt_base[num].access      = access;
}

static void write_tss(int num, uint16_t ss0, uint64_t esp0) {
    /* Calcular base y limite del TSS */
    uint64_t base = (uint64_t)&tss;
    uint64_t limit = sizeof(tss) - 1;

    /* Añadir descriptor TSS a la GDT (ocupa 2 entradas en 64-bit) */
    /* Entrada Baja - Byte 5 (Type) debe ser 0x9 (Available TSS 64-bit) => 0x89 (P=1) */
    gdt_set_gate(num, base, limit, 0x89, 0x00);
    
    /* Entrada Alta (Index num+1) */
    struct tss_entry_struct* tss_desc = (struct tss_entry_struct*)&gdt[num];
    tss_desc->base_upper = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    tss_desc->reserved = 0;

    /* Inicializar TSS a 0 */
    memset(&tss, 0, sizeof(tss));

    /* Configurar Stack de Ring 0 */
    extern uint64_t kernel_stack_top;
    tss.rsp0 = (uint64_t)kernel_stack_top;
    /* Use dedicated isolated stack for Double Faults (IST 1) */
    tss.ist[1] = (uint64_t)df_stack_bsp + DF_STACK_SIZE;
    tss.iomap_base = sizeof(tss); /* Sin I/O map */
    
    (void)ss0; /* Unused */
    (void)esp0; /* Unused */
}

void gdt_init(void) {
    serial_write_string("[GDT] Inicializando GDT y TSS (BSP)...\n");

    /* Limpiar GDT explícitamente para asegurar estado conocido */
    memset(gdt, 0, sizeof(gdt));

    /* 0: Null Descriptor */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* 1: Kernel Code (0x08) */
    gdt_set_gate(1, 0, 0, 0x9A, 0x20);

    /* 2: Kernel Data (0x10) */
    gdt_set_gate(2, 0, 0, 0x92, 0x00);

    /* 3: User Data (0x18) */
    gdt_set_gate(3, 0, 0, 0xF2, 0x00);

    /* 4: User Code (0x20) */
    gdt_set_gate(4, 0, 0, 0xFA, 0x20);

    /* 5: TSS (0x28) - 16 bytes */
    write_tss(5, 0x10, 0); 

    /* Cargar GDTR */
    gdtr.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gdtr.base  = (uint64_t)&gdt;

    /* Flush GDT y cargar TSS */
    gdt_flush((uint64_t)&gdtr);
    tss_flush();

    /* Asignar al BSP */
    cpus[0].gdt = (void*)&gdt;
    cpus[0].tss = (void*)&tss;
    cpus[0].self = (uint64_t)&cpus[0];

    /* 
     * IMPORTANT: gdt_flush() overwrites GS segment register, which might 
     * reset GS_BASE MSR to 0 on some CPUs/Environments. 
     * We must ensure GS_BASE points to our per-CPU structure for get_current_cpu().
     */
    wrmsr(MSR_GS_BASE, (uint64_t)&cpus[0]);
    wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)&cpus[0]);

    serial_write_string("[GDT] GDT cargada. TSS activa.\n");
}

void tss_set_rsp0(uint64_t rsp0) {
    cpu_info_t* cpu = get_current_cpu();
    if (cpu && cpu->tss) {
        ((struct tss_struct*)cpu->tss)->rsp0 = rsp0;
        /* ((struct tss_struct*)cpu->tss)->ist[1] = rsp0; */
    } else {
        /* Fallback for early boot (BSP only) */
        tss.rsp0 = rsp0;
        /* tss.ist[1] = rsp0; */
    }
}

void gdt_init_ap(cpu_info_t* cpu) {
    if (!cpu) return;

    /* Allocate GDT and TSS for this AP */
    struct gdt_entry* new_gdt = (struct gdt_entry*)kmalloc(sizeof(struct gdt_entry) * GDT_ENTRIES);
    struct tss_struct* new_tss = (struct tss_struct*)kmalloc(sizeof(struct tss_struct));

    if (!new_gdt || !new_tss) {
        serial_write_string("[GDT] Error: OOM allocating AP structures\n");
        return;
    }

    /* Initialize GDT: Copy from BSP/template */
    /* Copy first 5 entries (Null, Kernel Code/Data, User Code/Data) */
    memcpy(new_gdt, gdt, sizeof(struct gdt_entry) * 5);

    /* Double fault stack for AP */
    uint8_t* ap_df_stack = (uint8_t*)kmalloc(DF_STACK_SIZE);

    /* Initialize TSS */
    memset(new_tss, 0, sizeof(struct tss_struct));
    new_tss->rsp0 = 0; /* Will be set by scheduler later */
    if (ap_df_stack) {
        new_tss->ist[1] = (uint64_t)ap_df_stack + DF_STACK_SIZE;
    }
    new_tss->iomap_base = sizeof(struct tss_struct);

    /* Setup TSS Descriptor in new GDT (index 5) */
    uint64_t base = (uint64_t)new_tss;
    uint64_t limit = sizeof(struct tss_struct) - 1;

    gdt_set_gate_ptr(new_gdt, 5, base, limit, 0x89, 0x00);

    /* TSS high part */
    struct tss_entry_struct* tss_desc = (struct tss_entry_struct*)&new_gdt[5];
    tss_desc->base_upper = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    tss_desc->reserved = 0;

    /* Save pointers to CPU struct */
    cpu->gdt = (void*)new_gdt;
    cpu->tss = (void*)new_tss;
}

void gdt_load_for_cpu(cpu_info_t* cpu) {
    if (!cpu || !cpu->gdt) return;

    struct gdt_ptr local_gdtr;
    local_gdtr.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    local_gdtr.base  = (uint64_t)cpu->gdt;

    gdt_flush((uint64_t)&local_gdtr);
    tss_flush();
}
