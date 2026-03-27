/**
 * éterOS - Virtual Memory Manager (VMM)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 * 
 * Gestiona la paginación de 4 niveles (x86_64 Long Mode).
 * Controla PML4, PDPT, PD, PT para mapear direcciones virtuales a físicas.
 */

#include "../../include/vmm.h"
#include "../../include/pmm.h"
#include "../../include/string.h"
#include "../../include/errno.h"
#include "../../include/serial.h"
#include "../../include/hal/mm.h"
#include "../../include/apic.h"
#include "../../include/cpu.h"
#include "../../include/lock.h"
#include "../../include/idt.h" /* For IPI_TLB_SHOOTDOWN */

/* Estructura de una entrada de tabla de páginas (64-bit) */
typedef uint64_t pt_entry_t;

/* Puntero a la tabla PML4 activa */
static pt_entry_t* pml4 = (pt_entry_t*)BOOT_PML4_ADDR;

bool vmm_initialized_flag = false;

/* TLB Shootdown Globals */
volatile uint64_t tlb_flush_addr = 0;
volatile uint64_t tlb_ack_count = 0;
spinlock_t tlb_lock = 0;

/* Invalida una página en el TLB */
static inline void invlpg(uint64_t addr) {
#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile("invlpg (%0)" : : "r" (addr) : "memory");
#else
    (void)addr;
#endif
}

void vmm_flush_tlb_local(uint64_t addr) {
    invlpg(addr);
}

void vmm_flush_tlb_smp(uint64_t addr) {
    /* If single core or interrupts disabled (early boot), just flush local */
    if (total_cpus <= 1) {
        invlpg(addr);
        return;
    }

    /* Acquire lock to serialize shootdowns */
    spin_lock(&tlb_lock);

    tlb_flush_addr = addr;
    tlb_ack_count = 0;

    /* Current CPU ID */
    int my_id = get_cpu_id();
    uint64_t expected_acks = 0;

    /* Send IPI to all other CPUs */
    /* Iterate all CPUs */
    for (int i = 0; i < total_cpus; i++) {
        if (i == my_id) continue;
        if (cpus[i].state != CPU_STATE_ONLINE) continue;

        lapic_send_ipi(cpus[i].apic_id, IPI_TLB_SHOOTDOWN);
        expected_acks++;
    }

    /* Wait for ACKs (Strict Consistency) */
    /* We wait for other cores to acknowledge invalidation. */
    if (expected_acks > 0) {
        uint64_t timeout = 50000000; /* Increased to 50M cycles for stability on slow VMs */
        while (tlb_ack_count < expected_acks) {
#ifndef __ETEROS_HOST_TEST__
            __asm__ volatile("pause");
#endif
            timeout--;
            if (timeout == 0) {
                serial_write_string("[VMM] CRITICAL: TLB Shootdown Timeout! System might be unstable.\n");
                /* We break to avoid deadlock, but this is a serious error. */
                break;
            }
        }
    }

    spin_unlock(&tlb_lock);

    /* Always flush local */
    invlpg(addr);
}

/* Recarga CR3 (flush completo de TLB - costoso) */
static inline void load_cr3(uint64_t pml4_addr) {
#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile("mov %0, %%cr3" : : "r" (pml4_addr) : "memory");
#else
    (void)pml4_addr;
#endif
}

static inline pt_entry_t* get_active_pml4() {
    uint64_t cr3_val;
#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3_val));
#else
    cr3_val = (uint64_t)pml4;
#endif
    return (pt_entry_t*)(cr3_val & PAGE_ADDR_MASK);
}

/*
 * Obtiene o crea la siguiente tabla en la jerarquía.
 * Si la entrada no existe, asigna una nueva página física del PMM,
 * la limpia, y la enlaza en la tabla actual.
 */
static pt_entry_t* get_next_table(pt_entry_t* table, uint64_t index, int alloc) {
    if (table[index] & PAGE_PRESENT) {
        /* Check for Huge Page (Identity Mapped by Bootloader) */
        if (table[index] & PAGE_HUGE) {
             /* 
              * Huge Page Splitting: 
              * If we hit a 2MB page but need a 4KB mapping, we must split it.
              */
             void* new_pt_phys = pmm_alloc_page();
             if (!new_pt_phys) return NULL;
             memset(new_pt_phys, 0, PAGE_SIZE);

             pt_entry_t* new_pt = (pt_entry_t*)new_pt_phys;
             uint64_t phys_base = table[index] & PAGE_ADDR_MASK;
             uint64_t flags = table[index] & ~PAGE_ADDR_MASK;
             flags &= ~PAGE_HUGE; /* Remove PS bit for entries in PT */

             /* Fill PT with 4KB entries mapping the same 2MB region */
             for (int i = 0; i < 512; i++) {
                 new_pt[i] = (phys_base + i * PAGE_SIZE) | flags;
             }

             /* Replace Huge Page entry with PT pointer */
             table[index] = (uint64_t)new_pt_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
             
             /* Success, return the new PT */
             return new_pt;
        }
    
        /* La tabla existe, devolver su dirección virtual (Identity Mapping) */
        return (pt_entry_t*)(table[index] & PAGE_ADDR_MASK);
    }

    if (!alloc) return NULL;

    /* No existe, crear nueva tabla */
    void* new_table_phys = pmm_alloc_page();
    if (!new_table_phys) {
        serial_write_string("[VMM] Error: OOM, no se pudo alocar tabla de paginacion\n");
        return NULL;
    }

    char dbg_buf[64];
    serial_write_string("[VMM] Allocating new page table at ");
    utoa_hex_s((uintptr_t)new_table_phys, dbg_buf, sizeof(dbg_buf));
    serial_write_string(dbg_buf);
    serial_write_string("\n");

    /* Limpiar la nueva tabla (crítico para evitar entradas basura) */
    memset(new_table_phys, 0, PAGE_SIZE);

    /* Enlazar en la tabla padre (Present + Reserved/User/RW flags) */
    /* Heredamos permisos permisivos para las tablas intermedias, restringimos en la hoja (PT) */
    table[index] = (uint64_t)new_table_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;

    return (pt_entry_t*)new_table_phys;
}

void vmm_init(void) {
    serial_write_string("[VMM] Inicializando Gestor de Memoria Virtual...\n");
    
    /* El bootloader ya configuró un Identity Mapping básico (0-4MB o 0-8MB con Huge Pages) */
    /* Por ahora, seguimos usando ese PML4. */
    /* En el futuro, aquí crearíamos un nuevo PML4 limpio y cambiaríamos a él. */
    
    /* FIX: El Bootloader no configuró el flag de Usuario en la jerarquía inicial. */
    /* Necesitamos PAGE_USER en PML4[0] para permitir acceso a las aplicaciones Ring 3 */
    pml4[0] |= PAGE_USER;

    char pml4_addr_buf[32];
    serial_write_string("[VMM] Usando PML4 del bootloader en 0x");
    utoa_hex_s((uint64_t)pml4, pml4_addr_buf, sizeof(pml4_addr_buf));
    serial_write_string(pml4_addr_buf);
    serial_write_string("\n");

    vmm_initialized_flag = true;
}

int vmm_map_page(uint64_t phys_addr, uint64_t virt_addr, uint64_t flags) {
    /* Navegar la jerarquía: PML4 -> PDPT -> PD -> PT */
    
    uint64_t pml4_idx = PML4_INDEX(virt_addr);
    uint64_t pdpt_idx = PDPT_INDEX(virt_addr);
    uint64_t pd_idx   = PD_INDEX(virt_addr);
    uint64_t pt_idx   = PT_INDEX(virt_addr);

    pt_entry_t* active_pml4 = get_active_pml4();
    pt_entry_t* pdpt = get_next_table(active_pml4, pml4_idx, 1);
    if (!pdpt) return -1;

    pt_entry_t* pd = get_next_table(pdpt, pdpt_idx, 1);
    if (!pd) return -1;

    pt_entry_t* pt = get_next_table(pd, pd_idx, 1);
    if (!pt) return -1;

    /* Configurar la entrada de la tabla de páginas (PT Entry) */
    pt[pt_idx] = (phys_addr & PAGE_ADDR_MASK) | flags;

    /* Invalidar TLB para esta dirección (SMP) */
    vmm_flush_tlb_smp(virt_addr);

    return 0;
}

void vmm_unmap_page(uint64_t virt_addr) {
    uint64_t pml4_idx = PML4_INDEX(virt_addr);
    uint64_t pdpt_idx = PDPT_INDEX(virt_addr);
    uint64_t pd_idx   = PD_INDEX(virt_addr);
    uint64_t pt_idx   = PT_INDEX(virt_addr);

    pt_entry_t* active_pml4 = get_active_pml4();
    pt_entry_t* pdpt = get_next_table(active_pml4, pml4_idx, 0);
    if (!pdpt) return;

    pt_entry_t* pd = get_next_table(pdpt, pdpt_idx, 0);
    if (!pd) return;

    pt_entry_t* pt = get_next_table(pd, pd_idx, 0);
    if (!pt) return;

    /* Verificar si la página está presente */
    if (!(pt[pt_idx] & PAGE_PRESENT)) return;

    /* Marcar como no presente */
    pt[pt_idx] = 0;
    
    vmm_flush_tlb_smp(virt_addr);
}

uint64_t vmm_virt_to_phys(uint64_t virt_addr) {
    uint64_t pml4_idx = PML4_INDEX(virt_addr);
    uint64_t pdpt_idx = PDPT_INDEX(virt_addr);
    uint64_t pd_idx   = PD_INDEX(virt_addr);
    uint64_t pt_idx   = PT_INDEX(virt_addr);

    pt_entry_t* active_pml4 = get_active_pml4();

    /* 1. Walk PML4 */
    if (!(active_pml4[pml4_idx] & PAGE_PRESENT)) return 0;
    pt_entry_t* pdpt = (pt_entry_t*)(active_pml4[pml4_idx] & PAGE_ADDR_MASK);

    /* 2. Walk PDPT */
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;
    /* Check for 1GB Huge Page */
    if (pdpt[pdpt_idx] & PAGE_HUGE) {
        return (pdpt[pdpt_idx] & 0xFFFFFC0000000ULL) + (virt_addr & 0x3FFFFFFF);
    }
    pt_entry_t* pd = (pt_entry_t*)(pdpt[pdpt_idx] & PAGE_ADDR_MASK);

    /* 3. Walk PD */
    if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
    /* Check for 2MB Huge Page */
    if (pd[pd_idx] & PAGE_HUGE) {
        return (pd[pd_idx] & 0xFFFFFFFFFE00000ULL) + (virt_addr & 0x1FFFFF);
    }
    pt_entry_t* pt = (pt_entry_t*)(pd[pd_idx] & PAGE_ADDR_MASK);

    /* 4. Walk PT */
    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
    
    return (pt[pt_idx] & PAGE_ADDR_MASK) + (virt_addr & 0xFFF);
}

/* ========================================================================= */
/* HAL Implementation                                                        */
/* ========================================================================= */

int hal_mem_map(uint64_t phys_addr, uint64_t virt_addr, uint32_t flags) {
    uint64_t arch_flags = PAGE_PRESENT;

    /* Translate HAL flags to x86_64 Paging flags */
    if (flags & HAL_MEM_WRITE) {
        arch_flags |= PAGE_WRITE;
    }
    if (flags & HAL_MEM_USER) {
        arch_flags |= PAGE_USER;
    }

    /*
     * NX (No-Execute) Handling:
     * If HAL_MEM_EXEC is NOT set, we should theoretically set PAGE_NO_EXEC.
     * However, enabling NX requires EFER.NXE to be set.
     * For now, we will be permissive to avoid breaking legacy code that
     * forgets HAL_MEM_EXEC.
     *
     * Strict mode would be:
     * if (!(flags & HAL_MEM_EXEC)) arch_flags |= PAGE_NO_EXEC;
     */

    if (flags & HAL_MEM_CACHE_DISABLE) {
        /* PCD (Page Cache Disable) is usually bit 4 */
        arch_flags |= PAGE_PCD;
    }

    if (flags & HAL_MEM_WRITE_COMBINING) {
        /* WC via PAT: Index 1 (PWT=1, PCD=0) */
        /* We set PWT. We ensure PCD is cleared just in case. */
        arch_flags |= PAGE_PWT;
        arch_flags &= ~PAGE_PCD;
    }

    /* Call the architecture specific mapper */
    return vmm_map_page(phys_addr, virt_addr, arch_flags);
}

int hal_mem_unmap(uint64_t virt_addr) {
    vmm_unmap_page(virt_addr);
    return 0;
}

uint64_t hal_mem_get_phys(uint64_t virt_addr) {
    return vmm_virt_to_phys(virt_addr);
}

uint64_t vmm_get_pml4(void) {
    return (uint64_t)pml4;
}

static void clone_pt_recursive(pt_entry_t* dest, pt_entry_t* src, int level, int cow) {
    for (int i = 0; i < 512; i++) {
        if (!(src[i] & PAGE_PRESENT)) continue;

        /* Check for Shared Kernel Regions */
        /* Level 4 (PML4), Index != 0 -> Shared Kernel (Higher Half) */
        if (level == 4 && i != 0) {
            dest[i] = src[i];
            continue;
        }
        /* Level 3 (PDPT inside PML4[0]), Index < 8 -> Shared Kernel (0-8GB Identity) */
        if (level == 3 && i < 8) {
             dest[i] = src[i];
             continue;
        }

        if (level > 1) {
            /* Alloc new table for next level */
            void* new_table = pmm_alloc_page();
            if (!new_table) return; /* OOM */
            memset(new_table, 0, PAGE_SIZE);

            /* Recurse */
            pt_entry_t* child_src = (pt_entry_t*)(src[i] & PAGE_ADDR_MASK);
            pt_entry_t* child_dest = (pt_entry_t*)new_table;

            clone_pt_recursive(child_dest, child_src, level - 1, cow);

            /* Link new table */
            dest[i] = (uint64_t)new_table | (src[i] & 0xFFF);
        } else {
            /* Level 1 (PT) -> Clone Page */
            uint64_t phys = src[i] & PAGE_ADDR_MASK;
            uint64_t flags = src[i] & ~PAGE_ADDR_MASK;

            /* Ignore non-user pages found in user region (shouldn't happen but safety) */
            if (!(flags & PAGE_USER)) {
                dest[i] = src[i];
                continue;
            }

            if (cow) {
                /* CoW Logic */
                /* Update Source: Read-Only, CoW (Aether-Linux-Subsystem Phase 5.5) */
                src[i] |= PAGE_COW;
                src[i] &= ~PAGE_WRITE;

                /* Dest: Same phys, same flags */
                dest[i] = src[i];

                /* Increment Ref */
                pmm_ref_page((void*)phys);
            } else {
                /* Deep Copy */
                void* new_page = pmm_alloc_page();
                if (new_page) {
                    memcpy(new_page, (void*)phys, PAGE_SIZE);
                    dest[i] = (uint64_t)new_page | flags;
                }
            }
        }
    }
}

uint64_t vmm_clone_pml4(int cow) {
    pt_entry_t* new_pml4 = (pt_entry_t*)pmm_alloc_page();
    if (!new_pml4) return 0;
    memset(new_pml4, 0, PAGE_SIZE);

    pt_entry_t* active_pml4 = get_active_pml4();
    /* Start recursion from Level 4 */
    clone_pt_recursive(new_pml4, active_pml4, 4, cow);

    /* If we modified current tables (CoW), we must flush TLB */
    if (cow) {
        uint64_t current_cr3;
#ifndef __ETEROS_HOST_TEST__
        __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
        __asm__ volatile("mov %0, %%cr3" : : "r"(current_cr3) : "memory");
#else
        current_cr3 = (uint64_t)pml4;
        (void)current_cr3;
#endif
    }

    return (uint64_t)new_pml4;
}

static int vmm_resolve_cow(uint64_t addr, pt_entry_t* pt, uint64_t pt_idx) {
    if (!(pt[pt_idx] & PAGE_COW)) return 0;

    uint64_t old_phys = pt[pt_idx] & PAGE_ADDR_MASK;
    uint64_t flags = pt[pt_idx] & ~PAGE_ADDR_MASK;

    /* Check refcount */
    if (pmm_get_ref_count((void*)old_phys) == 1) {
        /* Optimize: If refcount is 1, just make it writable again (we are the last owner) */
        pt[pt_idx] &= ~PAGE_COW;
        pt[pt_idx] |= PAGE_WRITE;
    } else {
        /* Alloc new page */
        void* new_phys = pmm_alloc_page();
        if (!new_phys) {
            serial_write_string("[VMM] OOM during CoW\n");
            return 0;
        }

        /* Copy content */
        memcpy(new_phys, (void*)old_phys, PAGE_SIZE);

        /* Remap */
        pt[pt_idx] = (uint64_t)new_phys | (flags & ~PAGE_COW) | PAGE_WRITE;

        /* Decrement old ref */
        pmm_unref_page((void*)old_phys);
    }

    vmm_flush_tlb_smp(addr);
    return 1; /* Handled */
}

int vmm_handle_page_fault(uint64_t addr, uint64_t error_code) {
    /* Error Code:
       Bit 0: Present (0=Not Present, 1=Protection Violation)
       Bit 1: Write (1=Write)
       Bit 2: User (1=User)
    */

    int present = error_code & 1;
    int write   = error_code & 2;
    int user    = error_code & 4;

    /* Handle CoW: Write + Present + User (Aether-Linux-Subsystem Phase 5.5) */
    if (present && write && user) {
        /* Walk to find the entry */
        uint64_t pml4_idx = PML4_INDEX(addr);
        uint64_t pdpt_idx = PDPT_INDEX(addr);
        uint64_t pd_idx   = PD_INDEX(addr);
        uint64_t pt_idx   = PT_INDEX(addr);

        pt_entry_t* active_pml4 = get_active_pml4();
        pt_entry_t* pdpt = get_next_table(active_pml4, pml4_idx, 0);
        if (!pdpt) return 0;
        pt_entry_t* pd = get_next_table(pdpt, pdpt_idx, 0);
        if (!pd) return 0;
        pt_entry_t* pt = get_next_table(pd, pd_idx, 0);
        if (!pt) return 0;

        return vmm_resolve_cow(addr, pt, pt_idx);
    }
    return 0; /* Not handled */
}

int vmm_verify_user_access(const void* addr, size_t size, int write) {
    if (!vmm_validate_user_ptr(addr, size)) return 0;

    uint64_t start = (uint64_t)addr;
    uint64_t end = start + size;
    uint64_t start_page = start & ~(PAGE_SIZE - 1);
    uint64_t end_page = PAGE_ALIGN_UP(end);

    /* Get current PML4 (CR3) */
    uint64_t cr3;
#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
#else
    cr3 = (uint64_t)pml4;
#endif
    pt_entry_t* pml4_table = (pt_entry_t*)(cr3 & PAGE_ADDR_MASK);

    for (uint64_t v = start_page; v < end_page; v += PAGE_SIZE) {
        uint64_t pml4_idx = PML4_INDEX(v);
        uint64_t pdpt_idx = PDPT_INDEX(v);
        uint64_t pd_idx   = PD_INDEX(v);
        uint64_t pt_idx   = PT_INDEX(v);

        if (!(pml4_table[pml4_idx] & PAGE_PRESENT) || !(pml4_table[pml4_idx] & PAGE_USER)) return 0;
        pt_entry_t* pdpt = (pt_entry_t*)(pml4_table[pml4_idx] & PAGE_ADDR_MASK);

        if (!(pdpt[pdpt_idx] & PAGE_PRESENT) || !(pdpt[pdpt_idx] & PAGE_USER)) return 0;
        /* Huge page check omitted */
        pt_entry_t* pd = (pt_entry_t*)(pdpt[pdpt_idx] & PAGE_ADDR_MASK);

        if (!(pd[pd_idx] & PAGE_PRESENT) || !(pd[pd_idx] & PAGE_USER)) return 0;
        pt_entry_t* pt = (pt_entry_t*)(pd[pd_idx] & PAGE_ADDR_MASK);

        if (!(pt[pt_idx] & PAGE_PRESENT) || !(pt[pt_idx] & PAGE_USER)) return 0;

        /* Permissions Check */
        if (write) {
            if (pt[pt_idx] & PAGE_WRITE) continue;

            /* If not writable, check for CoW */
            if (pt[pt_idx] & PAGE_COW) {
                if (!vmm_resolve_cow(v, pt, pt_idx)) return 0;
            } else {
                return 0; /* Read-only and not CoW */
            }
        }
    }
    return 1;
}

int vmm_is_user_page(uint64_t virt_addr) {
    /* Read CR3 to get current PML4 physical address */
    uint64_t cr3;
#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
#else
    cr3 = (uint64_t)pml4;
#endif

    /* In Identity Mapping, Phys == Virt */
    pt_entry_t* pml4 = (pt_entry_t*)(cr3 & PAGE_ADDR_MASK);

    uint64_t pml4_idx = PML4_INDEX(virt_addr);
    uint64_t pdpt_idx = PDPT_INDEX(virt_addr);
    uint64_t pd_idx   = PD_INDEX(virt_addr);
    uint64_t pt_idx   = PT_INDEX(virt_addr);

    /* Level 4: PML4 */
    if (!(pml4[pml4_idx] & PAGE_PRESENT)) return 0;
    if (!(pml4[pml4_idx] & PAGE_USER)) return 0;

    pt_entry_t* pdpt = (pt_entry_t*)(pml4[pml4_idx] & PAGE_ADDR_MASK);

    /* Level 3: PDPT */
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;
    if (!(pdpt[pdpt_idx] & PAGE_USER)) return 0;

    if (pdpt[pdpt_idx] & PAGE_HUGE) {
        /* 1GB Page - User bit is sufficient */
        return 1;
    }

    pt_entry_t* pd = (pt_entry_t*)(pdpt[pdpt_idx] & PAGE_ADDR_MASK);

    /* Level 2: PD */
    if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
    if (!(pd[pd_idx] & PAGE_USER)) return 0;

    if (pd[pd_idx] & PAGE_HUGE) {
        /* 2MB Page - User bit is sufficient */
        return 1;
    }

    pt_entry_t* pt = (pt_entry_t*)(pd[pd_idx] & PAGE_ADDR_MASK);

    /* Level 1: PT */
    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
    if (!(pt[pt_idx] & PAGE_USER)) return 0;

    return 1;
}

int vmm_check_user_string(const char* str, size_t max_len) {
    if (!str) return 0;
    if (!vmm_validate_user_ptr(str, 1)) return 0;

    uint64_t ptr_val = (uint64_t)str;
    /* USER_LIMIT is the last valid address, so size is limit - ptr + 1 */
    uint64_t remaining = USER_LIMIT - ptr_val + 1;
    uint64_t scan_len = (remaining < max_len) ? remaining : max_len;

    for (uint64_t i = 0; i < scan_len; i++) {
        uint64_t curr_addr = ptr_val + i;
        /* Check page access only when crossing page boundary or at start */
        if ((i == 0) || ((curr_addr & (PAGE_SIZE - 1)) == 0)) {
            if (!vmm_verify_user_access((void*)curr_addr, 1, 0)) return 0;
        }
        if (str[i] == '\0') return 1;
    }
    return 0;
}

static void free_pt_recursive(pt_entry_t* table, int level) {
    for (int i = 0; i < 512; i++) {
        if (!(table[i] & PAGE_PRESENT)) continue;

        /* Skip Kernel Mappings */
        /* Level 4 (PML4): Index != 0 -> Kernel (Higher Half) */
        if (level == 4 && i != 0) continue;

        /* Level 3 (PDPT inside PML4[0]): Index < 8 -> Kernel Identity Map (0-8GB) */
        if (level == 3 && i < 8) continue;

        if (level > 1) {
            /* Recurse */
            pt_entry_t* child = (pt_entry_t*)(table[i] & PAGE_ADDR_MASK);
            free_pt_recursive(child, level - 1);

            /* Free the table page itself */
            pmm_free_page(child);
        } else {
            /* Level 1 (PT): Free the page */
            if (table[i] & PAGE_USER) {
                uint64_t phys = table[i] & PAGE_ADDR_MASK;
                pmm_unref_page((void*)phys);
            }
        }
    }
}

void vmm_destroy_pml4(uint64_t pml4_phys) {
    if (!pml4_phys) return;
    pt_entry_t* pml4 = (pt_entry_t*)pml4_phys;

    free_pt_recursive(pml4, 4);

    /* Free PML4 itself */
    pmm_free_page(pml4);
}

int vmm_validate_user_ptr(const void* addr, size_t size) {
    uint64_t start = (uint64_t)addr;
    uint64_t end = start + size;

    /* Check for overflow (end < start) */
    if (end < start || size > (USER_LIMIT - start + 1)) return 0;

    /* Check bounds: [start, end) must be within [USER_BASE, USER_LIMIT] */
    if (start < USER_BASE) return 0;
    if (end > (USER_LIMIT + 1)) return 0;

    return 1;
}

int vmm_strncpy_from_user(char* dst, const char* src, size_t max) {
    if (!dst || !src || max == 0) return -EINVAL;

    size_t copied = 0;
    while (copied < max) {
        uint64_t addr = (uint64_t)(src + copied);

        /* Check page boundary or first byte */
        if ((copied == 0) || ((addr & (PAGE_SIZE - 1)) == 0)) {
             if (!vmm_verify_user_access((void*)addr, 1, 0)) {
                 return -EFAULT;
             }
        }

        char c = src[copied];
        dst[copied] = c;
        if (c == '\0') {
            return (int)(copied + 1); /* Return bytes copied including null */
        }
        copied++;
    }

    /* Max reached without null terminator */
    dst[max - 1] = '\0';
    return (int)max;
}
