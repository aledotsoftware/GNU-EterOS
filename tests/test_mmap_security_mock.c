#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define PAGE_SIZE 4096
#define PAGE_ALIGN_UP(x) (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_DOWN(x) ((x) & ~(PAGE_SIZE - 1))
#define USER_LIMIT 0x7FFFFFFFFFFF
#define ENOMEM 12
#define EINVAL 22
#define ENODEV 19
#define HAL_MEM_USER 1
#define HAL_MEM_READ 2
#define HAL_MEM_WRITE 4
#define HAL_MEM_EXEC 8

typedef struct {
    uint64_t mmap_base;
} task_t;

/* Mocks */
static task_t mock_task = { .mmap_base = 0x10000000 };
static int hal_mem_get_phys_calls = 0;
static int hal_mem_unmap_calls = 0;
static int pmm_unref_calls = 0;
static int pmm_alloc_calls = 0;
static int hal_mem_map_calls = 0;
static uint64_t mock_phys_addr = 0xCAFE0000;

task_t* task_get_current(void) {
    return &mock_task;
}

uint64_t hal_mem_get_phys(uint64_t virt) {
    hal_mem_get_phys_calls++;
    if (virt == 0x20000000) return mock_phys_addr; /* Simulating existing mapping */
    return 0;
}

int hal_mem_unmap(uint64_t virt) {
    hal_mem_unmap_calls++;
    printf("hal_mem_unmap called for 0x%lx\n", virt);
    return 0;
}

void pmm_unref_page(void* phys) {
    pmm_unref_calls++;
    printf("pmm_unref_page called for %p\n", phys);
}

void* pmm_alloc_page(void) {
    pmm_alloc_calls++;
    return (void*)0xDEADBEEF;
}

int hal_mem_map(uint64_t phys, uint64_t virt, uint32_t flags) {
    hal_mem_map_calls++;
    return 0;
}

int vmm_validate_user_ptr(const void* addr, size_t size) {
    return 1;
}

/* Copy of sys_mmap logic from kernel/arch/x86_64/syscall.c */
static int64_t sys_mmap_test(void* addr, size_t len, int prot, int flags, int fd, int64_t offset) {
    if (len == 0) return -EINVAL;
    if (!(flags & 0x20)) return -ENODEV; /* MAP_ANONYMOUS required */

    task_t* current = task_get_current();
    uint64_t virt;

    if (addr && (flags & 0x10)) { /* MAP_FIXED */
        virt = (uint64_t)addr;
        if (virt & 0xFFF) return -EINVAL;
        if (!vmm_validate_user_ptr(addr, len)) return -ENOMEM;
    } else {
        /* ... elided default case ... */
        virt = current->mmap_base;
    }

    uint64_t start = PAGE_ALIGN_DOWN(virt);
    uint64_t end = PAGE_ALIGN_UP(virt + len);

    for (uint64_t v = start; v < end; v += PAGE_SIZE) {
        uint64_t old_phys = hal_mem_get_phys(v);
        if (old_phys != 0) {
            hal_mem_unmap(v);
            pmm_unref_page((void*)old_phys);
        }

        void* phys = pmm_alloc_page();
        if (!phys) return -ENOMEM;
        uint32_t map_flags = HAL_MEM_USER | HAL_MEM_READ;
        if (prot & 2) map_flags |= HAL_MEM_WRITE;
        if (prot & 4) map_flags |= HAL_MEM_EXEC;
        hal_mem_map((uint64_t)phys, v, map_flags);
        // memset((void*)v, 0, PAGE_SIZE); // Commented out as we can't memset real addresses
    }
    return virt;
}

int main() {
    printf("Testing sys_mmap security vulnerability...\n");

    /* Test Case: mmap MAP_FIXED over existing mapping */
    /* Address 0x20000000 has an existing mapping (mocked) */
    void* addr = (void*)0x20000000;
    size_t len = 4096;
    int prot = 3; /* RW */
    int flags = 0x20 | 0x10; /* MAP_ANONYMOUS | MAP_FIXED */

    sys_mmap_test(addr, len, prot, flags, -1, 0);

    if (hal_mem_unmap_calls == 0 && pmm_unref_calls == 0 && pmm_alloc_calls == 0) {
        printf("[FAIL] Existing mapping was reused (VULNERABLE)\n");
        return 1; /* Return 1 for failure */
    } else if (hal_mem_unmap_calls > 0 && pmm_unref_calls > 0) {
        printf("[PASS] Existing mapping was replaced\n");
        return 0;
    } else {
        printf("[FAIL] Unexpected state: unmap=%d, unref=%d, alloc=%d\n",
               hal_mem_unmap_calls, pmm_unref_calls, pmm_alloc_calls);
        return 2;
    }
}
