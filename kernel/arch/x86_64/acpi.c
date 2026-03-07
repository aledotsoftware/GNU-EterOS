/*
 * ACPI Parser para éterOS
 *
 * Implementa la detección básica de hardware para Multicore (SMP).
 * Escanea memoria en busca de RSDP -> RSDT -> MADT.
 */
#include <acpi.h>
#include <string.h>
#include <serial.h>
#include <mm.h>

#define ACPI_SCAN_START 0x000E0000
#define ACPI_SCAN_END   0x000FFFFF

#include <cpu.h>

/* Variables globales */
static acpi_rsdp_t* rsdp = NULL;
static acpi_rsdt_t* rsdt = NULL;
static acpi_madt_t* madt = NULL;
static acpi_fadt_t* fadt = NULL;

/* Estado global de CPUs (Topología) */
cpu_info_t cpus[MAX_CPUS];
int total_cpus = 0;

/* Verificar checksum de una tabla */
static int acpi_check_sum(void* table, size_t length) {
    uint8_t sum = 0;
    uint8_t* val = (uint8_t*)table;
    for (size_t i = 0; i < length; i++) {
        sum += val[i];
    }
    return (sum == 0);
}

/* Buscar "RSD PTR " en memoria */
static void* acpi_find_rsdp(void) {
    uint8_t* ptr;
    for (ptr = (uint8_t*)ACPI_SCAN_START; ptr < (uint8_t*)ACPI_SCAN_END; ptr += 16) {
        if (memcmp(ptr, ACPI_RSDP_SIG, 8) == 0) {
            /* Validar checksum */
            if (acpi_check_sum(ptr, 20)) {
                return (void*)ptr;
            } else {
                serial_write_string("[ACPI] Found bad checksum RSDP\n");
            }
        }
    }
    return NULL;
}

/* Parsear MADT para encontrar CPUs */
static void parse_madt(void) {
    if (!madt) return;
    
    char buf[64];
    serial_write_string("[ACPI] MADT en: 0x");
    utoa_hex_s((uint64_t)madt, buf, 64);
    serial_write_string(buf);
    
    serial_write_string(" LAPIC Addr: 0x");
    utoa_hex_s((uint64_t)madt->lapic_addr, buf, 64);
    serial_write_string(buf);
    serial_write_string("\n");
    
    uint8_t* ptr = (uint8_t*)madt->entries;
    uint8_t* end = (uint8_t*)madt + madt->header.length;
    
    
    total_cpus = 0;
    
    while (ptr < end) {
        madt_entry_header_t* entry = (madt_entry_header_t*)ptr;
        
        switch (entry->type) {
            case MADT_ENTRY_LAPIC: {
                madt_lapic_t* lapic = (madt_lapic_t*)entry;
                if (lapic->flags & 1) { /* Processor Enabled */
                    if (total_cpus < MAX_CPUS) {
                        cpus[total_cpus].apic_id = lapic->apic_id;
                        cpus[total_cpus].acpi_id = lapic->acpi_processor_id;
                        cpus[total_cpus].index = total_cpus;
                        cpus[total_cpus].state = (total_cpus == 0) ? CPU_STATE_ONLINE : CPU_STATE_OFFLINE; /* BSP is online */
                        
                        serial_write_string("[ACPI] CPU Found! Index: ");
                        itoa_s(total_cpus, buf, 64, 10);
                        serial_write_string(buf);
                        serial_write_string(" APIC ID: ");
                        itoa_s(lapic->apic_id, buf, 64, 10);
                        serial_write_string(buf);
                        serial_write_string("\n");
                        
                        total_cpus++;
                    }
                }
                break;
            }
            case MADT_ENTRY_IOAPIC: {
                madt_ioapic_t* ioapic = (madt_ioapic_t*)entry;
                serial_write_string("[ACPI] I/O APIC Found! Addr: 0x");
                utoa_hex_s((uint64_t)ioapic->ioapic_addr, buf, 64);
                serial_write_string(buf);
                serial_write_string("\n");
                break;
            }
        }
        
        ptr += entry->length;
    }
    
    serial_write_string("[ACPI] Total CPUs detected: ");
    itoa_s(total_cpus, buf, 64, 10);
    serial_write_string(buf);
    serial_write_string("\n");
}

void acpi_init(void) {
    serial_write_string("[ACPI] Detecting System Hardware...\n");
    
    rsdp = (acpi_rsdp_t*)acpi_find_rsdp();
    if (!rsdp) {
        serial_write_string("[ACPI] ERROR: RSDP Not Found! (Are we in BIOS mode?)\n");
        return;
    }
    
    /* RSDT Address (32-bit legacy) */
    rsdt = (acpi_rsdt_t*)(uint64_t)rsdp->rsdt_addr;
    
    if (!acpi_check_sum(rsdt, rsdt->header.length)) {
        serial_write_string("[ACPI] ERROR: RSDT Checksum Invalid!\n");
        return;
    }
    
    /* Find tables inside RSDT */
    int entries = (rsdt->header.length - sizeof(acpi_header_t)) / 4;
    
    for (int i = 0; i < entries; i++) {
        acpi_header_t* header = (acpi_header_t*)(uint64_t)rsdt->tables[i];
        
        if (memcmp(header->signature, ACPI_MADT_SIG, 4) == 0) {
            madt = (acpi_madt_t*)header;
        } else if (memcmp(header->signature, "FACP", 4) == 0) {
            fadt = (acpi_fadt_t*)header;
        }
    }
    
    if (madt) {
        parse_madt();
    } else {
        serial_write_string("[ACPI] WARNING: MADT table not found in RSDT.\n");
    }

    if (fadt) {
        serial_write_string("[ACPI] FACP (FADT) table found.\n");
    } else {
        serial_write_string("[ACPI] WARNING: FACP (FADT) table not found in RSDT.\n");
    }
}

int acpi_get_cpu_count(void) {
    return total_cpus > 0 ? total_cpus : 1;
}

uint32_t acpi_get_lapic_addr(void) {
    return madt ? madt->lapic_addr : 0xFEE00000;
}

#include <io.h>
void acpi_poweroff(void) {
    if (!fadt) {
        serial_write_string("[ACPI] Cannot poweroff, FADT not found.\n");
        return;
    }

    /* Out to PM1a control block */
    /* SLP_EN (bit 13) | SLP_TYPa = 5 (bits 10-12) */
    outw(fadt->pm1a_control_block, 0x2000 | (5 << 10));

    if (fadt->pm1b_control_block) {
        outw(fadt->pm1b_control_block, 0x2000 | (5 << 10));
    }

    serial_write_string("[ACPI] Poweroff signal sent.\n");
    while (1) {
        __asm__ volatile("hlt");
    }
}
