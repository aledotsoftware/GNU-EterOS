/* éterOS - kernel/arch/x86_64/
 *
 * Capa de Abstracción de Hardware (HAL) para x86_64
 *
 * Este directorio contendrá:
 *   - gdt.c / gdt.h     : Global Descriptor Table & TSS
 *   - idt.c / idt.h     : Interrupt Descriptor Table & ISRs
 *   - paging.c / paging.h : Gestión de paginación (PML4/PDPT/PD/PT)
 *   - apic.c / apic.h   : Advanced Programmable Interrupt Controller
 *   - cpu.c / cpu.h      : Detección de features de CPU (CPUID)
 *
 * La HAL permite que el kernel sea agnóstico al hardware.
 * Para portar éterOS a RISC-V o ARM, solo esta capa necesita
 * ser reimplementada.
 */
