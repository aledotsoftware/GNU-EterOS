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
static struct gdt_ptr gdtr;

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

static void write_tss(int num, uint16_t ss0, uint64_t esp0) {
    /* Calcular base y limite del TSS */
    uint64_t base = (uint64_t)&tss;
    uint64_t limit = sizeof(tss) - 1;

    /* Añadir descriptor TSS a la GDT (ocupa 2 entradas en 64-bit) */
    /* Entrada Baja - Byte 5 (Type) debe ser 0x9 (Available TSS 64-bit) => 0x89 (P=1) */
    gdt_set_gate(num, base, limit, 0x89, 0x00);
    
    /* Entrada Alta (Index num+1) */
    /* Usamos un puntero directo a la entrada siguiente para configurar los bits altos de la base */
    struct gdt_entry* high_entry = &gdt[num + 1];
    memset(high_entry, 0, sizeof(struct gdt_entry));
    
    high_entry->limit_low = (uint16_t)((base >> 32) & 0xFFFF);
    high_entry->base_low  = (uint16_t)((base >> 48) & 0xFFFF);
    /* El resto de la entrada alta debe ser 0 para TSS 64-bit standard (Base 63:32 en bytes 0-3 y 7) */
    /* Wait, struct GDT Entry regular no mapea bien con el descriptor de sistema extendido de 16-bytes parte alta */
    
    /* Reimplementando para usar struct tss_entry_struct correctamente sobre las 2 entradas */
    struct tss_entry_struct* tss_desc = (struct tss_entry_struct*)&gdt[num];
    tss_desc->base_upper = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    tss_desc->reserved = 0;

    /* Inicializar TSS a 0 */
    memset(&tss, 0, sizeof(tss));

    /* Configurar Stack de Ring 0 */
    tss.rsp0 = esp0;
    tss.ist[1] = esp0; /* Mapear también IST1 por seguridad */
    tss.iomap_base = sizeof(tss); /* Sin I/O map */
    
    (void)ss0; /* Unused */
}

void gdt_init(void) {
    serial_write_string("[GDT] Inicializando GDT y TSS...\n");

    /* Limpiar GDT explícitamente para asegurar estado conocido */
    memset(gdt, 0, sizeof(gdt));

    /* 0: Null Descriptor */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* 1: Kernel Code (0x08) */
    /* Access: 0x9A (P=1, DPL=0, Type=Code/Exec/Read) */
    /* Granularity: 0x20 (L=1 Long Mode) */
    gdt_set_gate(1, 0, 0, 0x9A, 0x20);

    /* 2: Kernel Data (0x10) */
    /* Access: 0x92 (P=1, DPL=0, Type=Data/RW) */
    gdt_set_gate(2, 0, 0, 0x92, 0x00);

    /* 3: User Data (0x18) */
    /* Access: 0xF2 (P=1, DPL=3, Type=Data/RW) */
    gdt_set_gate(3, 0, 0, 0xF2, 0x00);

    /* 4: User Code (0x20) */
    /* Access: 0xFA (P=1, DPL=3, Type=Code/Exec/Read) */
    /* Granularity: 0x20 (L=1 Long Mode) */
    gdt_set_gate(4, 0, 0, 0xFA, 0x20);

    /* 5: TSS (0x28) - 16 bytes */
    /* Usamos el stack actual como RSP0 inicial (temporal, cambiará al scheduling) */
    /* Pero como estamos en single-core sin multithreading real aún, es seguro. */
    /* O mejor, usamos un stack conocido si tuviéramos. */
    write_tss(5, 0x10, 0); 

    /* Cargar GDTR */
    gdtr.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gdtr.base  = (uint64_t)&gdt;

    /* Flush GDT y cargar TSS */
    gdt_flush((uint64_t)&gdtr);
    tss_flush();

    serial_write_string("[GDT] GDT cargada. TSS activa.\n");
}

void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}
