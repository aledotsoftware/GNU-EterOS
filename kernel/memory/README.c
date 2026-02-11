/* éterOS - kernel/memory/
 *
 * Subsistema de Gestión de Memoria
 *
 * Este directorio contendrá:
 *   - pmm.c / pmm.h   : Physical Memory Manager (mapa de bits, páginas 4KB)
 *   - vmm.c / vmm.h   : Virtual Memory Manager (paginación dinámica)
 *   - heap.c / heap.h  : Heap del kernel (kmalloc/kfree)
 *
 * Diseñado para:
 *   - Soporte de Huge Pages (2 MB) para Framebuffer
 *   - Separación de espacio kernel/usuario
 *   - Eficiencia en memoria (<10 MB total del sistema)
 */
