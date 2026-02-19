#ifndef ETEROS_VMM_H
#define ETEROS_VMM_H

#include "types.h"

/* ========================================================================= */
/* Estructuras de Paginación x86_64 (4-Level Paging)                         */
/* ========================================================================= */

#define PAGE_SIZE       4096
#define PAGE_PRESENT    0x1
#define PAGE_WRITE      0x2
#define PAGE_USER       0x4
#define PAGE_HUGE       0x80
#define PAGE_COW        0x200 /* Bit 9: Copy-on-Write (OS Available) */
#define PAGE_NO_EXEC    0x8000000000000000

/* Máscara para obtener la dirección física de una entrada (bits 12-51) */
#define PAGE_ADDR_MASK  0x000FFFFFFFFFF000

/* Índices en la dirección virtual */
#define PML4_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr) (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)   (((addr) >> 12) & 0x1FF)

/* Dirección base del PML4 configurado por el bootloader */
/* Nota: En boot.asm configuramos esto en 0x70000 */
#define BOOT_PML4_ADDR  0x70000
/* Tamaño de las tablas de paginación iniciales (24 KB = PML4 + PDPT + 4*PD) */
#define BOOT_PAGE_TABLE_SIZE 0x6000

/* Límites del Espacio de Usuario (User Space) */
#define USER_BASE       0x200000000UL        /* Inicio en 8 GB */
#define USER_LIMIT      0x00007FFFFFFFFFFFUL /* Fin del espacio canónico inferior */

/* ========================================================================= */
/* API VMM                                                                   */
/* ========================================================================= */

/**
 * Inicializa el gestor de memoria virtual.
 * Configura la paginación del kernel y prepara el sistema para mapeos dinámicos.
 */
void vmm_init(void);

/**
 * Mapea una página física a una dirección virtual.
 * 
 * @param phys_addr Dirección física (debe estar alineada a 4KB)
 * @param virt_addr Dirección virtual (debe estar alineada a 4KB)
 * @param flags Flags de página (PAGE_PRESENT | PAGE_WRITE, etc.)
 * @return 0 en éxito, -1 si falla (ej. sin memoria física para tablas)
 */
int vmm_map_page(uint64_t phys_addr, uint64_t virt_addr, uint64_t flags);

/**
 * Desmapea una página virtual (la marca como no presente).
 * No libera la memoria física subyacente (usar pmm_free_page para eso).
 */
void vmm_unmap_page(uint64_t virt_addr);

/**
 * Obtiene la dirección física mapeada a una dirección virtual.
 * @return Dirección física o 0 si no está mapeada.
 */
uint64_t vmm_virt_to_phys(uint64_t virt_addr);

/**
 * Obtiene la dirección del PML4 actual (CR3).
 */
uint64_t vmm_get_pml4(void);

/**
 * Clona el espacio de direcciones actual (PML4).
 * @param cow Si true, usa Copy-on-Write para páginas de usuario.
 *            Si false, realiza una copia profunda (Deep Copy).
 * @return Dirección física del nuevo PML4.
 */
uint64_t vmm_clone_pml4(int cow);

/**
 * Manejador de Page Faults (llamado desde ISR 14).
 * @param addr Dirección virtual que causó el fallo (CR2).
 * @param error_code Código de error pusheado por CPU.
 * @return 1 si fue manejado, 0 si no.
 */
int vmm_handle_page_fault(uint64_t addr, uint64_t error_code);

/**
 * Verifica si una dirección virtual está mapeada como usuario.
 * @return 1 si es accesible por usuario (PAGE_USER set en todos los niveles), 0 si no.
 */
int vmm_is_user_page(uint64_t virt_addr);

/**
 * Valida un puntero de usuario (y su longitud) mediante chequeo de rangos.
 * Reemplaza la iteración de páginas para evitar DoS.
 *
 * @param addr Dirección de inicio del buffer.
 * @param size Tamaño del buffer.
 * @return 1 si es válido (dentro del rango de usuario), 0 si no.
 */
int vmm_validate_user_ptr(const void* addr, size_t size);

#endif /* ETEROS_VMM_H */
