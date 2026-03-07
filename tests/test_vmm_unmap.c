#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Undefine macros that may conflict */
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

/* Undefine macros to avoid recursion if include/string.h was picked up */
#ifdef memset
#undef memset
#endif
#ifdef memcpy
#undef memcpy
#endif

/* Standard library function pointers */
static void* (*std_memset)(void*, int, size_t) = memset;
static void* (*std_memcpy)(void*, const void*, size_t) = memcpy;

/* Override for kernel memset */
void* eteros_memset(void* dest, int c, size_t n) { return std_memset(dest, c, n); }
void* eteros_memcpy(void* dest, const void* src, size_t n) { return std_memcpy(dest, src, n); }

/* Globals for testing */
bool flush_tlb_local_called = false;
uint64_t flush_tlb_addr = 0;

/* Mock Kernel/Hardware functions */
void serial_write_string(const char* str) { (void)str; }
void terminal_write_string(const char* str) { (void)str; }
void utoa_hex_s(uint64_t value, char* buffer, size_t buffer_size) {
    if (buffer && buffer_size > 0) {
        snprintf(buffer, buffer_size, "%lx", value);
    }
}

int eteros_snprintf(char* str, size_t size, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

#undef vsnprintf
int eteros_vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    return vsnprintf(str, size, format, ap);
}

/* Memory mocks */
void* pmm_alloc_page(void) {
    void* ptr = NULL;
    posix_memalign(&ptr, 4096, 4096);
    if (ptr) std_memset(ptr, 0, 4096);
    return ptr;
}
void pmm_free_page(void* addr) { free(addr); }
uint16_t pmm_get_ref_count(void* addr) { return 1; }
void pmm_ref_page(void* addr) { }
void pmm_unref_page(void* addr) { }

void lapic_send_ipi(uint32_t apic_id, uint32_t vector) { (void)apic_id; (void)vector; }

int total_cpus = 1;

#include "../include/cpu.h"

cpu_info_t cpus[MAX_CPUS];

/* Include modified source file directly */
#include "vmm_mock.c"

/* Helper to setup VMM */
void setup_vmm() {
    pml4 = pmm_alloc_page(); /* Provide a valid mock PML4 array */
    vmm_init();
}

void cleanup_vmm() {
    pmm_free_page(pml4);
}

void test_vmm_unmap_page_mapped() {
    printf("Running test_vmm_unmap_page_mapped...\n");

    /* Initialize a dummy PML4 */
    if (pml4) {
        pmm_free_page(pml4);
    }
    pml4 = (pt_entry_t*)pmm_alloc_page();

    uint64_t virt_addr = 0x40000000;
    uint64_t phys_addr = 0x80000000;

    /* Map the page */
    int map_res = vmm_map_page(phys_addr, virt_addr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    if (map_res != 0) {
        printf("FAILED: vmm_map_page returned %d\n", map_res);
        exit(1);
    }

    /* Verify it is mapped */
    /* We need to mask out flags to compare physically */
    uint64_t mapped_phys = vmm_virt_to_phys(virt_addr);
    if (mapped_phys != phys_addr) {
        printf("FAILED (mapped_phys %lx != %lx)\n", mapped_phys, phys_addr);
        exit(1);
    }

    /* Reset flush TLB flag to track if unmap flushes it */
    flush_tlb_local_called = false;

    /* Unmap the page */
    vmm_unmap_page(virt_addr);

    /* Verify it is no longer mapped */
    mapped_phys = vmm_virt_to_phys(virt_addr);
    if (mapped_phys != 0) {
        printf("FAILED (mapped_phys %lx != 0 after unmap)\n", mapped_phys);
        exit(1);
    }

    /* TLB should be flushed after unmapping */
    if (!flush_tlb_local_called) {
        printf("FAILED (TLB not flushed)\n");
        printf("FAILED: vmm_virt_to_phys returned %lx instead of 0 after unmap\n", mapped_phys);
        exit(1);
    }

    printf("PASSED\n");
}

void test_unmap_non_existent_page() {
    printf("Running test_unmap_non_existent_page... ");

    uint64_t virt_addr = 0x300000; /* Virtual address that is not mapped */

    /* Make sure it's not mapped */
    uint64_t mapped_phys = vmm_virt_to_phys(virt_addr);
    if (mapped_phys != 0) {
        printf("FAILED (Address %lx is already mapped to %lx)\n", virt_addr, mapped_phys);
        exit(1);
    }

    flush_tlb_local_called = false;

    /* Attempt to unmap the non-existent page. It should simply return without crashing. */
    vmm_unmap_page(virt_addr);

    /* TLB should NOT be flushed if page was not present (although vmm_unmap_page currently flushes if it makes it, but in non-existent case it returns early) */
    if (flush_tlb_local_called) {
        printf("FAILED (TLB flushed for non-existent page)\n");
        exit(1);
    }
}

void test_vmm_unmap_page_unmapped() {
    printf("Running test_vmm_unmap_page_unmapped...\n");

    /* Initialize a dummy PML4 */
    if (pml4) {
        pmm_free_page(pml4);
    }
    pml4 = (pt_entry_t*)pmm_alloc_page();

    uint64_t virt_addr = 0x80000000; // Not mapped

    /* Attempt to unmap a page that isn't mapped - should not crash */
    vmm_unmap_page(virt_addr);

    printf("PASSED\n");
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);

    setup_vmm();

    printf("\nAll VMM Unmap tests passed!\n");
    printf("Starting VMM tests...\n");

    cpus[0].state = CPU_STATE_ONLINE;

    test_vmm_unmap_page_mapped();
    test_unmap_non_existent_page();
    test_vmm_unmap_page_unmapped();

    cleanup_vmm();

    printf("All VMM tests passed!\n");
    return 0;
}
