/**
 * éterOS - Funciones de E/S de Puertos
 * 
 * Funciones inline para acceso directo a puertos de E/S
 * del hardware x86_64.
 */

#ifndef ETEROS_IO_H
#define ETEROS_IO_H

#include "types.h"

#ifndef __ETEROS_HOST_TEST__

/* ========================================================================= */
/* Escritura en puerto                                                       */
/* ========================================================================= */

/**
 * Escribe un byte en un puerto de E/S.
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * Escribe una palabra (16 bits) en un puerto de E/S.
 */
static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * Escribe una doble palabra (32 bits) en un puerto de E/S.
 */
static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

/* ========================================================================= */
/* Lectura de puerto                                                         */
/* ========================================================================= */

/**
 * Lee un byte de un puerto de E/S.
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/**
 * Lee una palabra (16 bits) de un puerto de E/S.
 */
static inline uint16_t inw(uint16_t port) {
    uint16_t result;
    __asm__ volatile ("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/**
 * Lee una doble palabra (32 bits) de un puerto de E/S.
 */
static inline uint32_t inl(uint16_t port) {
    uint32_t result;
    __asm__ volatile ("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/* ========================================================================= */
/* MSR Access                                                                */
/* ========================================================================= */

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = val & 0xFFFFFFFF;
    uint32_t high = val >> 32;
    __asm__ volatile("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

/* ========================================================================= */
/* Utilidades                                                                */
/* ========================================================================= */

/**
 * Espera un ciclo de E/S (usado para dar tiempo al hardware).
 */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/**
 * Detiene la CPU hasta la próxima interrupción.
 */
static inline void cpu_halt(void) {
    __asm__ volatile ("hlt");
}

/* ========================================================================= */
/* Control de Interrupciones                                                 */
/* ========================================================================= */

/**
 * Verifica si las interrupciones están habilitadas (Flag IF).
 */
static inline int interrupts_enabled(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0" : "=r"(flags));
    return (flags & 0x200) != 0;
}

/**
 * Guarda el estado de las interrupciones y las deshabilita.
 * Retorna los flags para ser restaurados luego.
 */
static inline uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

/**
 * Restaura el estado de las interrupciones.
 */
static inline void irq_restore(uint64_t flags) {
    if (flags & 0x200) {
        __asm__ volatile ("sti");
    }
}

#else

/* Mock function declarations for host testing */
void outb(uint16_t port, uint8_t value);
void outw(uint16_t port, uint16_t value);
void outl(uint16_t port, uint32_t value);

uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);

void io_wait(void);
void cpu_halt(void);

int interrupts_enabled(void);
uint64_t irq_save(void);
void irq_restore(uint64_t flags);

#endif /* !__ETEROS_HOST_TEST__ */

#endif /* ETEROS_IO_H */
