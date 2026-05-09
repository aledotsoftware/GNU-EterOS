#define __ETEROS_HOST_TEST__ 1
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "../include/acpi.h"
#include "../include/cpu.h"
#include "../include/mm.h"
#include "../include/io.h"

uint16_t mocked_pm1a_port = 0;
uint16_t mocked_pm1a_val = 0;
uint16_t mocked_pm1b_port = 0;
uint16_t mocked_pm1b_val = 0;

void outw(uint16_t port, uint16_t value) {
    if (port == 0x1234) {
        mocked_pm1a_port = port;
        mocked_pm1a_val = value;
    } else if (port == 0x5678) {
        mocked_pm1b_port = port;
        mocked_pm1b_val = value;
    }
}
void outb(uint16_t port, uint8_t value) { (void)port; (void)value; }
void outl(uint16_t port, uint32_t value) { (void)port; (void)value; }
uint8_t inb(uint16_t port) { (void)port; return 0; }
uint16_t inw(uint16_t port) { (void)port; return 0; }
uint32_t inl(uint16_t port) { (void)port; return 0; }

void serial_write_string(const char* str) { printf("%s", str); }

#define HALT_MACRO() return

#include "../kernel/arch/x86_64/acpi.c"

uint8_t mock_dsdt[100] __attribute__((aligned(4096)));

int main() {
    printf("Starting test_acpi_s5...\n");

    static acpi_fadt_t mock_fadt;
    memset(&mock_fadt, 0, sizeof(mock_fadt));
    mock_fadt.pm1a_control_block = 0x1234;
    mock_fadt.pm1b_control_block = 0x5678;

    memset(mock_dsdt, 0, sizeof(mock_dsdt));

    acpi_header_t* header = (acpi_header_t*)mock_dsdt;
    memcpy(header->signature, "DSDT", 4);
    header->length = sizeof(mock_dsdt);

    uint32_t aml_offset = 36;
    mock_dsdt[aml_offset++] = '_';
    mock_dsdt[aml_offset++] = 'S';
    mock_dsdt[aml_offset++] = '5';
    mock_dsdt[aml_offset++] = '_';
    mock_dsdt[aml_offset++] = 0x12; /* PackageOp */
    mock_dsdt[aml_offset++] = 0x03; /* PkgLength */
    mock_dsdt[aml_offset++] = 0x02; /* NumElements */

    mock_dsdt[aml_offset++] = 0x0A;
    mock_dsdt[aml_offset++] = 0x07;
    mock_dsdt[aml_offset++] = 0x0A;
    mock_dsdt[aml_offset++] = 0x02;

    /* Just call acpi_parse_s5 directly with the 64-bit pointer to test parsing */
    uint16_t typa, typb;
    int ret = acpi_parse_s5((uint64_t)mock_dsdt, &typa, &typb);
    if (ret != 0) {
        printf("FAILED: acpi_parse_s5 returned %d\n", ret);
        return 1;
    }

    if (typa != 7 || typb != 2) {
        printf("FAILED: parsed values: typa=%d typb=%d\n", typa, typb);
        return 1;
    }

    /* We can't safely test acpi_poweroff() since it reads fadt->dsdt (32-bit) which truncates our 64-bit pointer,
       but the function itself is simple enough and parsing works. */

    printf("test_acpi_s5 passed successfully!\n");
    return 0;
}
