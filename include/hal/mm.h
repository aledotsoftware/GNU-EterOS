#ifndef ETEROS_HAL_MM_H
#define ETEROS_HAL_MM_H

#include "../types.h"

/* ========================================================================= */
/* HAL Memory Interface (Abstract)                                            */
/* ========================================================================= */

#define HAL_PAGE_SIZE  4096

/* Flags for memory mapping */
/* Note: These are abstract flags. The implementation (vmm.c) will translate them. */

#define HAL_MEM_READ          (1 << 0)  /* Page is readable */
#define HAL_MEM_WRITE         (1 << 1)  /* Page is writable */
#define HAL_MEM_EXEC          (1 << 2)  /* Page is executable (if supported) */
#define HAL_MEM_USER          (1 << 3)  /* Page is accessible from User Mode (Ring 3) */
#define HAL_MEM_CACHE_DISABLE (1 << 4)  /* Disable caching (e.g. for MMIO) */
#define HAL_MEM_WRITETHROUGH  (1 << 5)  /* Write-through caching */
#define HAL_MEM_GLOBAL        (1 << 6)  /* Global page (not flushed on context switch) */

/* ========================================================================= */
/* API                                                                       */
/* ========================================================================= */

/**
 * Initializes the MMU / Paging system.
 * This function should be called by hal_init() or shortly after.
 */
void hal_mmu_init(void);

/**
 * Maps a physical address to a virtual address.
 *
 * @param phys_addr Physical address (must be HAL_PAGE_SIZE aligned)
 * @param virt_addr Virtual address (must be HAL_PAGE_SIZE aligned)
 * @param flags     Bitmask of HAL_MEM_* flags
 * @return 0 on success, < 0 on error
 */
int hal_mem_map(uint64_t phys_addr, uint64_t virt_addr, uint32_t flags);

/**
 * Unmaps a virtual address (marks it as not present).
 *
 * @param virt_addr Virtual address to unmap
 * @return 0 on success, < 0 on error
 */
int hal_mem_unmap(uint64_t virt_addr);

/**
 * Translates a virtual address to its physical address.
 *
 * @param virt_addr Virtual address
 * @return Physical address, or 0 if not mapped / invalid
 */
uint64_t hal_mem_get_phys(uint64_t virt_addr);

#endif /* ETEROS_HAL_MM_H */
