/**
 * éterOS - Driver de Mouse PS/2
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Driver básico para mouse PS/2 (IRQ 12).
 * Soporta protocolo estándar de 3 bytes (X, Y, Botones).
 */

#ifndef ETEROS_MOUSE_H
#define ETEROS_MOUSE_H

#include <types.h>

/* Estructura para almacenar el estado del mouse */
typedef struct {
    int32_t x;          /* Posición X absoluta o acumulada */
    int32_t y;          /* Posición Y absoluta o acumulada */
    uint8_t buttons;    /* Estado de botones (Bit 0: Left, 1: Right, 2: Middle) */
} mouse_state_t;

/* Callback para notificar eventos de movimiento/click */
typedef void (*mouse_callback_t)(int8_t dx, int8_t dy, uint8_t buttons);

/**
 * Inicializa el controlador PS/2 para el mouse.
 */
void mouse_init(void);

/**
 * Instala un callback para recibir actualizaciones del mouse.
 */
void mouse_set_callback(mouse_callback_t callback);

/**
 * Procesa un byte recibido de la IRQ 12.
 */
void mouse_process_byte(uint8_t byte);

/**
 * Establece la sensibilidad del mouse (multiplicador).
 */
void mouse_set_sensitivity(uint8_t sens);

/**
 * Establece si el mouse esta configurado para zurdos (intercambia botones).
 */
void mouse_set_handedness(bool left_handed);

#endif /* ETEROS_MOUSE_H */
