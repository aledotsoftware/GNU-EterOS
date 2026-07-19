#ifndef HALT_MACRO
#define HALT_MACRO() hal_cpu_halt()
#endif
/*
 * ACPI Parser para éterOS
 *
 * Implementa la detección básica de hardware para Multicore (SMP).
 * Escanea memoria en busca de RSDP -> RSDT -> MADT.
 */
#include <acpi.h>
#include <hal.h>
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
acpi_fadt_t* fadt = NULL;

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

static int acpi_parse_s5(uint64_t dsdt_addr, uint16_t *slp_typa, uint16_t *slp_typb) {
    if (!dsdt_addr) return -1;
    acpi_header_t* dsdt = (acpi_header_t*)(uint64_t)dsdt_addr;
    if (memcmp(dsdt->signature, "DSDT", 4) != 0) return -1;

    uint8_t* aml = (uint8_t*)dsdt + sizeof(acpi_header_t);
    uint32_t aml_len = dsdt->length - sizeof(acpi_header_t);

    for (uint32_t i = 0; i < aml_len - 5; i++) {
        /* Find _S5_ */
        if (aml[i] == '_' && aml[i+1] == 'S' && aml[i+2] == '5' && aml[i+3] == '_') {
            /* Check if it's a PackageOp (0x12) */
            if (aml[i+4] == 0x12) {
                uint32_t ptr = i + 5;
                /* Skip PkgLength (variable length encoding) */
                uint8_t lead_byte = aml[ptr];
                uint32_t pkg_len_bytes = (lead_byte >> 6) + 1;
                ptr += pkg_len_bytes;

                /* Skip NumElements */
                ptr++;

                /* The next bytes are the elements. They usually are BytePrefix (0x0A) followed by the byte,
                   or WordPrefix (0x0B) followed by word. Or if it's a small integer it could be 0x00, 0x01, or 0x0A <val> */

                uint16_t typa = 0, typb = 0;

                /* Parse SLP_TYPa */
                if (aml[ptr] == 0x0A) { typa = aml[ptr+1]; ptr += 2; }
                else if (aml[ptr] == 0x00) { typa = 0; ptr++; }
                else if (aml[ptr] == 0x01) { typa = 1; ptr++; }
                else { typa = aml[ptr]; ptr++; } /* Fallback */

                /* Parse SLP_TYPb */
                if (aml[ptr] == 0x0A) { typb = aml[ptr+1]; ptr += 2; }
                else if (aml[ptr] == 0x00) { typb = 0; ptr++; }
                else if (aml[ptr] == 0x01) { typb = 1; ptr++; }
                else { typb = aml[ptr]; ptr++; } /* Fallback */

                *slp_typa = typa;
                *slp_typb = typb;
                return 0;
            }
        }
    }
    return -1;
}

void acpi_poweroff(void) {
    if (!fadt) {
        serial_write_string("[ACPI] Cannot poweroff, FADT not found.\n");
        return;
    }

    uint16_t slp_typa = 5; /* Default */
    uint16_t slp_typb = 5;

    if (acpi_parse_s5(fadt->dsdt, &slp_typa, &slp_typb) == 0) {
        serial_write_string("[ACPI] _S5_ parsed successfully.\n");
    } else {
        serial_write_string("[ACPI] _S5_ not found, using defaults.\n");
    }

    /* Out to PM1a/b control block */
    /* SLP_EN (bit 13) | SLP_TYP */
    outw(fadt->pm1a_control_block, 0x2000 | (slp_typa << 10));

    if (fadt->pm1b_control_block) {
        outw(fadt->pm1b_control_block, 0x2000 | (slp_typb << 10));
    }

    serial_write_string("[ACPI] Poweroff signal sent.\n");
    while (1) {
        HALT_MACRO();
    }
}
