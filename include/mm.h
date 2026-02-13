/**
 * éterOS - Memory Management Header
 * 
 * Interfaz para el gestor de memoria dinámica del kernel.
 * Provee funciones estilo malloc/free.
 */

#ifndef ETEROS_MM_H
#define ETEROS_MM_H

#include "types.h"
#include "boot.h"

/* ========================================================================= */
/* API de Memoria Dinámica                                                   */
/* ========================================================================= */

/**
 * Inicializa el sistema de gestión de memoria.
 * Debe ser llamado al inicio del kernel.
 *
 * @param boot_info Puntero a la estructura de información del bootloader
 *                  (necesario para leer el mapa de memoria).
 */
void mm_init(boot_info_t* boot_info);

/**
 * Asigna un bloque de memoria de al menos 'size' bytes.
 * Retorna NULL si no hay memoria disponible.
 */
void* kmalloc(size_t size);

/**
 * Asigna un bloque de memoria para 'num' elementos de 'size' bytes cada uno,
 * inicializando la memoria a cero.
 */
void* kcalloc(size_t num, size_t size);

/**
 * Libera un bloque de memoria previamente asignado por kmalloc o kcalloc.
 * Si ptr es NULL, no hace nada.
 */
void kfree(void* ptr);

/* ========================================================================= */
/* Diagnóstico y Estadísticas                                                */
/* ========================================================================= */

/**
 * Retorna la cantidad total de memoria gestionada por el heap (en bytes).
 */
size_t mm_get_total_memory(void);

/**
 * Retorna la cantidad de memoria actualmente en uso (ocupada) en bytes.
 * Incluye el overhead de las estructuras de control.
 */
size_t mm_get_used_memory(void);

#endif /* ETEROS_MM_H */
