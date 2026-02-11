/**
 * éterOS - Driver de Puerto Serie (UART 16550)
 * 
 * Proporciona funciones para depuración a través del
 * puerto serie COM1 (0x3F8).
 */

#ifndef ETEROS_SERIAL_H
#define ETEROS_SERIAL_H

#include "types.h"

/* ========================================================================= */
/* Constantes del Puerto Serie                                               */
/* ========================================================================= */
#define COM1_PORT   0x3F8
#define COM2_PORT   0x2F8

/* ========================================================================= */
/* API del Puerto Serie                                                       */
/* ========================================================================= */

/**
 * Inicializa el puerto serie COM1 a 38400 baud.
 * Retorna 0 si la inicialización fue exitosa.
 */
int serial_init(void);

/**
 * Escribe un carácter al puerto serie.
 */
void serial_putchar(char c);

/**
 * Escribe una cadena al puerto serie.
 */
void serial_write_string(const char* str);

/**
 * Lee un carácter del puerto serie (bloqueante).
 */
char serial_read(void);

/**
 * Verifica si hay datos disponibles para leer.
 */
int serial_received(void);

/**
 * Manejador de interrupciones del puerto serie.
 * Debe ser llamado desde el ISR correspondiente.
 */
void serial_irq_handler(void);

#endif /* ETEROS_SERIAL_H */
