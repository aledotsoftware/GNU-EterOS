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
#include "../../include/serial.h"
#include "../../include/hal/mm.h"
#include "../../include/task.h" /* For Demand Paging (vm_area_t) */

/* Estructura de una entrada de tabla de páginas (64-bit) */
typedef uint64_t pt_entry_t;

/* Puntero a la tabla PML4 activa */
/* DEPRECATED: Use read_cr3() to get active table */
/* static pt_entry_t* pml4 = (pt_entry_t*)BOOT_PML4_ADDR; */

/* Reads current CR3 value (PML4 physical address) */
static inline uint64_t read_cr3(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3 & PAGE_ADDR_MASK;
}

/* Invalida una página en el TLB */
static inline void invlpg(uint64_t addr) {
    __asm__ volatile("invlpg (%0)" : : "r" (addr) : "memory");
}

/* Recarga CR3 (flush completo de TLB - costoso) */
static inline void load_cr3(uint64_t pml4_addr) {
    __asm__ volatile("mov %0, %%cr3" : : "r" (pml4_addr) : "memory");
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
             serial_write_string("[VMM] Error: Cannot traverse Huge Page\n");
             return NULL;
        }
    
        /* La tabla existe, devolver su dirección virtual (Identity Mapping) */
        /* NOTA: Como usamos Identity Mapping para el kernel, Physical == Virtual */
        /* En un kernel Higher Half, necesitaríamos convertir Phys -> Virt aquí */
        return (pt_entry_t*)(table[index] & PAGE_ADDR_MASK);
    }

    if (!alloc) return NULL;

    /* No existe, crear nueva tabla */
    void* new_table_phys = pmm_alloc_page();
    if (!new_table_phys) {
        serial_write_string("[VMM] Error: OOM, no se pudo alocar tabla de paginacion\n");
        return NULL;
    }

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
    
    serial_write_string("[VMM] Usando PML4 del bootloader en 0x70000\n");
}

int vmm_map_page(uint64_t phys_addr, uint64_t virt_addr, uint64_t flags) {
    /* Navegar la jerarquía: PML4 -> PDPT -> PD -> PT */
    
    /* Use current active PML4 */
    pt_entry_t* pml4 = (pt_entry_t*)read_cr3();

    uint64_t pml4_idx = PML4_INDEX(virt_addr);
    uint64_t pdpt_idx = PDPT_INDEX(virt_addr);
    uint64_t pd_idx   = PD_INDEX(virt_addr);
    uint64_t pt_idx   = PT_INDEX(virt_addr);

    pt_entry_t* pdpt = get_next_table(pml4, pml4_idx, 1);
    if (!pdpt) return -1;

    pt_entry_t* pd = get_next_table(pdpt, pdpt_idx, 1);
    if (!pd) return -1;

    pt_entry_t* pt = get_next_table(pd, pd_idx, 1);
    if (!pt) return -1;

    /* Configurar la entrada de la tabla de páginas (PT Entry) */
    pt[pt_idx] = (phys_addr & PAGE_ADDR_MASK) | flags;

    /* Invalidar TLB para esta dirección */
    invlpg(virt_addr);

    return 0;
}

int vmm_map_demand_page(uint64_t virt_addr, uint64_t flags) {
    /* Use current active PML4 */
    pt_entry_t* pml4 = (pt_entry_t*)read_cr3();

    uint64_t pml4_idx = PML4_INDEX(virt_addr);
    uint64_t pdpt_idx = PDPT_INDEX(virt_addr);
    uint64_t pd_idx   = PD_INDEX(virt_addr);
    uint64_t pt_idx   = PT_INDEX(virt_addr);

    pt_entry_t* pdpt = get_next_table(pml4, pml4_idx, 1);
    if (!pdpt) return -1;
    pt_entry_t* pd = get_next_table(pdpt, pdpt_idx, 1);
    if (!pd) return -1;
    pt_entry_t* pt = get_next_table(pd, pd_idx, 1);
    if (!pt) return -1;

    /* Entry = 0 (No Phys Address) | Flags | PAGE_DEMAND */
    /* Ensure PAGE_PRESENT is NOT set */
    pt[pt_idx] = (flags & ~PAGE_PRESENT) | PAGE_DEMAND;

    invlpg(virt_addr);
    return 0;
}

void vmm_unmap_page(uint64_t virt_addr) {
    /* Use current active PML4 */
    pt_entry_t* pml4 = (pt_entry_t*)read_cr3();

    uint64_t pml4_idx = PML4_INDEX(virt_addr);
    uint64_t pdpt_idx = PDPT_INDEX(virt_addr);
    uint64_t pd_idx   = PD_INDEX(virt_addr);
    uint64_t pt_idx   = PT_INDEX(virt_addr);

    pt_entry_t* pdpt = get_next_table(pml4, pml4_idx, 0);
    if (!pdpt) return;

    pt_entry_t* pd = get_next_table(pdpt, pdpt_idx, 0);
    if (!pd) return;

    pt_entry_t* pt = get_next_table(pd, pd_idx, 0);
    if (!pt) return;

    /* Marcar como no presente */
    pt[pt_idx] = 0;
    
    invlpg(virt_addr);
}

uint64_t vmm_virt_to_phys(uint64_t virt_addr) {
    /* Use current active PML4 */
    pt_entry_t* pml4 = (pt_entry_t*)read_cr3();

    uint64_t pml4_idx = PML4_INDEX(virt_addr);
    uint64_t pdpt_idx = PDPT_INDEX(virt_addr);
    uint64_t pd_idx   = PD_INDEX(virt_addr);
    uint64_t pt_idx   = PT_INDEX(virt_addr);

    pt_entry_t* pdpt = get_next_table(pml4, pml4_idx, 0);
    if (!pdpt) return 0;

    pt_entry_t* pd = get_next_table(pdpt, pdpt_idx, 0);
    if (!pd) return 0;

    pt_entry_t* pt = get_next_table(pd, pd_idx, 0);
    if (!pt) return 0;
    
    /* Verificar si la página está presente */
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
        arch_flags |= 0x10;
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
    return read_cr3();
}

static void clone_pt_recursive(pt_entry_t* dest, pt_entry_t* src, int level, int cow) {
    for (int i = 0; i < 512; i++) {
        /* Handle Not Present Entries */
        if (!(src[i] & PAGE_PRESENT)) {
            /* If Demand Paging (PAGE_DEMAND set), clone it */
            if (src[i] & PAGE_DEMAND) {
                dest[i] = src[i];
            }
            continue;
        }

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
                /* Check Refcount Limit */
                /* If > 250, safer to Deep Copy to avoid overflow */
                if (pmm_get_ref_count((void*)phys) >= 250) {
                     /* Fallback to Deep Copy */
                     void* new_page = pmm_alloc_page();
                     if (new_page) {
                        /* CAUTION: Assumes Identity Map for phys and new_page! */
                        /* If this fails (high memory), we need a better copy mechanism. */
                        memcpy(new_page, (void*)phys, PAGE_SIZE);
                        dest[i] = (uint64_t)new_page | flags;
                     }
                } else {
                    /* CoW Logic */
                    /* Update Source: Read-Only, CoW */
                    src[i] |= PAGE_COW;
                    src[i] &= ~PAGE_WRITE;

                    /* Dest: Same phys, same flags */
                    dest[i] = src[i];

                    /* Increment Ref */
                    pmm_ref_page((void*)phys);
                }
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
    /* Use static BOOT_PML4_ADDR as source for cloning the base kernel mappings? */
    /* No, we should clone the CURRENT pml4 (src)? */
    /* But current pml4 might contain User mappings. */
    /* Usually fork clones Current User Mappings. */
    /* So src should be read_cr3(). */

    pt_entry_t* src_pml4 = (pt_entry_t*)read_cr3();
    pt_entry_t* new_pml4 = (pt_entry_t*)pmm_alloc_page();
    if (!new_pml4) return 0;
    memset(new_pml4, 0, PAGE_SIZE);

    /* Start recursion from Level 4 */
    clone_pt_recursive(new_pml4, src_pml4, 4, cow);

    /* If we modified current tables (CoW), we must flush TLB */
    if (cow) {
        uint64_t current_cr3 = (uint64_t)src_pml4;
        __asm__ volatile("mov %0, %%cr3" : : "r"(current_cr3) : "memory");
    }

    return (uint64_t)new_pml4;
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

    /* Use current active PML4 */
    pt_entry_t* pml4 = (pt_entry_t*)read_cr3();

    /* Handle Demand Paging: Not Present */
    if (!present) {
        /* Walk to find the entry */
        uint64_t pml4_idx = PML4_INDEX(addr);
        uint64_t pdpt_idx = PDPT_INDEX(addr);
        uint64_t pd_idx   = PD_INDEX(addr);
        uint64_t pt_idx   = PT_INDEX(addr);

        pt_entry_t* pdpt = get_next_table(pml4, pml4_idx, 0);
        if (!pdpt) return 0;
        pt_entry_t* pd = get_next_table(pdpt, pdpt_idx, 0);
        if (!pd) return 0;
        pt_entry_t* pt = get_next_table(pd, pd_idx, 0);
        if (!pt) return 0;

        /* Check if entry is marked Demand Paging */
        if (pt[pt_idx] & PAGE_DEMAND) {
              /* Demand Paging Fault */
              /* Enable interrupts to allow IO */
              __asm__ volatile("sti");

              task_t* current = task_get_current();
              if (current && current->mmap_list) {
                  vm_area_t* vma = current->mmap_list;
                  while (vma) {
                      if (addr >= vma->start && addr < vma->end) {
                           /* Found VMA */
                           void* new_phys = pmm_alloc_page();
                           if (!new_phys) { /* OOM */
                               __asm__ volatile("cli");
                               return 0;
                           }

                           uint64_t map_flags = PAGE_PRESENT | PAGE_USER;
                           if (vma->flags & HAL_MEM_WRITE) map_flags |= PAGE_WRITE;

                           /* Map temporarily as WRITE to allow loading content */
                           uint64_t temp_flags = map_flags | PAGE_WRITE;

                           /* Disable interrupts to update PT atomically */
                           __asm__ volatile("cli");

                           /* Map first! */
                           pt[pt_idx] = (uint64_t)new_phys | temp_flags;
                           invlpg(addr);

                           /* Enable interrupts for IO */
                           __asm__ volatile("sti");

                           /* Use Virtual Address to access page */
                           uint64_t page_virt = addr & PAGE_ADDR_MASK;

                           /* Clean page */
                           memset((void*)page_virt, 0, PAGE_SIZE);

                           /* Read from file */
                           uint64_t page_offset_in_vma = page_virt - vma->start;
                           uint64_t page_file_offset = vma->offset + page_offset_in_vma;

                           if (page_file_offset < vma->file_size) {
                               uint64_t remaining = vma->file_size - page_file_offset;
                               uint64_t read_len = (remaining > PAGE_SIZE) ? PAGE_SIZE : remaining;

                               if (vma->node) {
                                    /* Read into Virtual Address */
                                    read_fs(vma->node, page_file_offset, read_len, (uint8_t*)page_virt);
                               }
                           }

                           /* Restore correct flags if needed (e.g. remove WRITE) */
                           if (!(map_flags & PAGE_WRITE)) {
                               __asm__ volatile("cli");
                               pt[pt_idx] = (uint64_t)new_phys | map_flags;
                               invlpg(addr);
                           } else {
                               __asm__ volatile("cli");
                           }

                           return 1;
                      }
                      vma = vma->next;
                  }
              }
              __asm__ volatile("cli");
        }
    }

    /* Handle CoW: Write + Present + User */
    if (present && write && user) {
        /* Walk to find the entry */
        uint64_t pml4_idx = PML4_INDEX(addr);
        uint64_t pdpt_idx = PDPT_INDEX(addr);
        uint64_t pd_idx   = PD_INDEX(addr);
        uint64_t pt_idx   = PT_INDEX(addr);

        pt_entry_t* pdpt = get_next_table(pml4, pml4_idx, 0);
        if (!pdpt) return 0;
        pt_entry_t* pd = get_next_table(pdpt, pdpt_idx, 0);
        if (!pd) return 0;
        pt_entry_t* pt = get_next_table(pd, pd_idx, 0);
        if (!pt) return 0;

        if (pt[pt_idx] & PAGE_COW) {
             /* CoW Fault! */
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
                 /* Issue: accessing old_phys and new_phys directly. */
                 /* We are in Page Fault. 'addr' is valid (it faulted because of permissions). */
                 /* So we can read from 'addr' (Virtual) which points to 'old_phys'. */
                 /* But we need to write to 'new_phys'. 'new_phys' is NOT mapped yet at 'addr'. */

                 /* Strategy:
                    1. Read 'addr' into a temp buffer (stack? 4KB is big for kernel stack).
                    2. Map 'new_phys' to 'addr'.
                    3. Write temp buffer to 'addr'.
                 */
                 /* Or rely on Identity Map if available. If high memory, unsafe. */

                 /* Better Strategy (without temp buffer):
                    1. Map 'new_phys' to a temporary virtual address (e.g. some reserved slot).
                    2. memcpy(temp_virt, addr, PAGE_SIZE).
                    3. Unmap temp_virt.
                    4. Remap 'addr' to 'new_phys'.
                 */

                 /* Since we don't have a sophisticated temp map system, and 4KB on stack is risky (stack size 4KB? kernel stack is 4KB per task). */
                 /* task_create allocs 32KB stack (TASK_STACK_SIZE). */
                 /* So 4KB on stack IS safe! */

                 uint8_t buffer[PAGE_SIZE];
                 memcpy(buffer, (void*)(addr & PAGE_ADDR_MASK), PAGE_SIZE);

                 /* Now map new page */
                 pt[pt_idx] = (uint64_t)new_phys | (flags & ~PAGE_COW) | PAGE_WRITE;
                 invlpg(addr);

                 /* Write back */
                 memcpy((void*)(addr & PAGE_ADDR_MASK), buffer, PAGE_SIZE);

                 /* Decrement old ref */
                 pmm_unref_page((void*)old_phys);
             }

             invlpg(addr);
             return 1; /* Handled */
        }
    }
    return 0; /* Not handled */
}

int vmm_is_user_page(uint64_t virt_addr) {
    /* Use current active PML4 */
    pt_entry_t* pml4 = (pt_entry_t*)read_cr3();

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
