#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

/* Undefine string macros to avoid recursion if include/string.h was picked up */
#ifdef memset
#undef memset
#endif
#ifdef memcpy
#undef memcpy
#endif

/* Capture standard library functions before they are renamed */
static void* (*std_memset)(void*, int, size_t) = memset;
static void* (*std_memcpy)(void*, const void*, size_t) = memcpy;

/* Include necessary headers */
#include "../include/string.h"
#include "../include/vmm.h"
#include "../include/pmm.h"
#include "../include/cpu.h"
#include "../include/lock.h"

/* Implement string functions */
void* eteros_memset(void* dest, int c, size_t n) { return std_memset(dest, c, n); }
void* eteros_memcpy(void* dest, const void* src, size_t n) { return std_memcpy(dest, src, n); }

/* Globals for our mocks */
uint64_t mock_invlpg_addr = 0;
int mock_invlpg_called = 0;

/* Mock other required globals and functions */
uint8_t _kernel_end = 0;
int total_cpus = 1;
cpu_info_t cpus[MAX_CPUS];

void lapic_send_ipi(uint32_t apic_id, uint32_t vector) { (void)apic_id; (void)vector; }

void* pmm_alloc_page(void) { return NULL; }
void pmm_free_page(void* addr) { (void)addr; }
void pmm_ref_page(void* addr) { (void)addr; }
void pmm_unref_page(void* addr) { (void)addr; }
uint16_t pmm_get_ref_count(void* addr) { (void)addr; return 0; }

void serial_write_string(const char* str) { (void)str; }

/* Include VMM implementation to test it */
#include "../kernel/mm/vmm.c"

void test_vmm_flush_tlb_local() {
    printf("Running test_vmm_flush_tlb_local... ");

    mock_invlpg_addr = 0;
    mock_invlpg_called = 0;

    uint64_t test_addr = 0x12345000;
    vmm_flush_tlb_local(test_addr);

    assert(mock_invlpg_called == 1);
    assert(mock_invlpg_addr == test_addr);

    printf("PASSED\n");
}

void test_vmm_flush_tlb_smp_single_core() {
    printf("Running test_vmm_flush_tlb_smp_single_core... ");

    mock_invlpg_addr = 0;
    mock_invlpg_called = 0;
    total_cpus = 1; /* Ensure single core path is taken */

    uint64_t test_addr = 0x87654000;
    vmm_flush_tlb_smp(test_addr);

    assert(mock_invlpg_called == 1);
    assert(mock_invlpg_addr == test_addr);

    printf("PASSED\n");
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);

    test_vmm_flush_tlb_local();
    test_vmm_flush_tlb_smp_single_core();

    printf("\nAll VMM Flush tests passed!\n");
    return 0;
}
