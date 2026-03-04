#ifndef ACPI_H
#define ACPI_H

#include <types.h>

/* Firmas de tablas */
#define ACPI_RSDP_SIG "RSD PTR "
#define ACPI_MADT_SIG "APIC"

/* Estructura RSDP (Root System Description Pointer) */
/* Se busca en 0xE0000 - 0xFFFFF o en EBDA */
typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_addr;     /* Dirección física de RSDT (32-bit) */
    
    /* ACPI 2.0+ fields */
    uint32_t length;
    uint64_t xsdt_addr;     /* Dirección física de XSDT (64-bit) */
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

/* Header genérico para todas las tablas (RSDT, MADT, etc.) */
typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_header_t;

/* RSDT (Root System Description Table) */
typedef struct {
    acpi_header_t header;
    uint32_t tables[];      /* Punteros a otras tablas */
} __attribute__((packed)) acpi_rsdt_t;

/* MADT (Multiple APIC Description Table) */
typedef struct {
    acpi_header_t header;
    uint32_t lapic_addr;    /* Dirección física del Local APIC */
    uint32_t flags;         /* Bit 0 = PCAT_COMPAT (Dual 8259 present) */
    uint8_t entries[];      /* Entradas de longitud variable */
} __attribute__((packed)) acpi_madt_t;

/* Tipos de entradas en MADT */
#define MADT_ENTRY_LAPIC      0
#define MADT_ENTRY_IOAPIC     1
#define MADT_ENTRY_ISO        2
#define MADT_ENTRY_NMI        4

/* Estructura genérica de entrada MADT */
typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) madt_entry_header_t;

/* Processor Local APIC Entry */
typedef struct {
    madt_entry_header_t header;
    uint8_t acpi_processor_id;
    uint8_t apic_id;        /* El ID real del hardware para interrupciones */
    uint32_t flags;         /* Bit 0 = Processor Enabled */
} __attribute__((packed)) madt_lapic_t;

/* I/O APIC Entry */
typedef struct {
    madt_entry_header_t header;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_addr;
    uint32_t global_system_interrupt_base;
} __attribute__((packed)) madt_ioapic_t;

/* FADT (Fixed ACPI Description Table) */
typedef struct {
    acpi_header_t header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t reserved;
    uint8_t preferred_power_management;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t pm1_event_length;
    uint8_t pm1_control_length;
    uint8_t pm2_control_length;
    uint8_t pm_timer_length;
    uint8_t gpe0_length;
    uint8_t gpe1_length;
    uint8_t gpe1_base;
    uint8_t cstate_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alarm;
    uint8_t month_alarm;
    uint8_t century;
    uint16_t boot_architecture_flags;
    uint8_t reserved2;
    uint32_t flags;
} __attribute__((packed)) acpi_fadt_t;

/* Funciones Públicas */
void acpi_init(void);
int acpi_get_cpu_count(void);
uint32_t acpi_get_lapic_addr(void);
void acpi_poweroff(void);

#endif
