#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
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

/* Undefine macros from include/stdio.h if we picked it up */
#ifdef snprintf
#undef snprintf
#endif
#ifdef vsnprintf
#undef vsnprintf
#endif

/* Capture standard library functions */
static void* (*std_memset)(void*, int, size_t) = memset;
static void* (*std_memcpy)(void*, const void*, size_t) = memcpy;

/* Implement string functions used by kernel */
void* eteros_memset(void* dest, int c, size_t n) { return std_memset(dest, c, n); }
void* eteros_memcpy(void* dest, const void* src, size_t n) { return std_memcpy(dest, src, n); }

/* Mock Kernel/Hardware functions */
void serial_write_string(const char* str) { (void)str; }
void utoa_hex_s(uint64_t value, char* buffer, size_t buffer_size) {
    if (buffer && buffer_size > 0) {
        snprintf(buffer, buffer_size, "%lx", value);
    }
}

int total_cpus = 1;

#include "../include/vmm.h"
#include "../include/pmm.h"
#include "../include/lock.h"
#include "../include/apic.h"

/* We mock pmm functions directly since we compile with vmm_mock.c */
void* allocated_pages[1024];
int allocated_pages_count = 0;

/* Align to page size for Identity Mapping assumption */
void* pmm_alloc_page(void) {
    void* ptr;
    posix_memalign(&ptr, PAGE_SIZE, PAGE_SIZE);
    std_memset(ptr, 0, PAGE_SIZE);
    allocated_pages[allocated_pages_count++] = ptr;
    return ptr;
}

void pmm_free_page(void* ptr) {
    free(ptr);
    for (int i = 0; i < allocated_pages_count; i++) {
        if (allocated_pages[i] == ptr) {
            allocated_pages[i] = NULL;
            break;
        }
    }
}

uint16_t pmm_get_ref_count(void* ptr) { return 1; }
void pmm_ref_page(void* ptr) {}
void pmm_unref_page(void* ptr) {}

/* Mock apic and cpu things */
void lapic_send_ipi(uint32_t apic_id, uint32_t vector) {}

#define get_current_cpu real_get_current_cpu
#define get_cpu_id real_get_cpu_id
#include "../include/cpu.h"
#undef get_current_cpu
#undef get_cpu_id

cpu_info_t cpus[MAX_CPUS];
int get_cpu_id(void) { return 0; }
cpu_info_t* get_current_cpu(void) { return &cpus[0]; }

/* We include vmm.c directly to test static functions and variables */
#include "../kernel/mm/vmm.c"

void test_vmm_unmap_page_mapped() {
    printf("Running test_vmm_unmap_page_mapped...\n");

    /* Initialize a dummy PML4 */
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
    uint64_t mapped_phys = vmm_virt_to_phys(virt_addr);
    if (mapped_phys != phys_addr) {
        printf("FAILED: vmm_virt_to_phys returned %lx instead of %lx\n", mapped_phys, phys_addr);
        exit(1);
    }

    /* Unmap the page */
    vmm_unmap_page(virt_addr);

    /* Verify it is no longer mapped */
    mapped_phys = vmm_virt_to_phys(virt_addr);
    if (mapped_phys != 0) {
        printf("FAILED: vmm_virt_to_phys returned %lx instead of 0 after unmap\n", mapped_phys);
        exit(1);
    }

    printf("PASSED\n");
}

void test_vmm_unmap_page_unmapped() {
    printf("Running test_vmm_unmap_page_unmapped...\n");

    /* Initialize a dummy PML4 */
    pml4 = (pt_entry_t*)pmm_alloc_page();

    uint64_t virt_addr = 0x80000000; // Not mapped

    /* Attempt to unmap a page that isn't mapped - should not crash */
    vmm_unmap_page(virt_addr);

    printf("PASSED\n");
}

int main() {
    printf("Starting VMM tests...\n");

    cpus[0].state = CPU_STATE_ONLINE;

    test_vmm_unmap_page_mapped();
    test_vmm_unmap_page_unmapped();

    /* Cleanup allocated pages */
    for (int i = 0; i < allocated_pages_count; i++) {
        if (allocated_pages[i] != NULL) {
            free(allocated_pages[i]);
        }
    }

    printf("All VMM tests passed!\n");
    return 0;
}
