/**
 * éterOS - Driver de Teclado PS/2
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Maneja el teclado PS/2 mediante IRQ1.
 * Traduce scancodes (Set 1) a caracteres ASCII.
 */

#ifndef ETEROS_KEYBOARD_H
#define ETEROS_KEYBOARD_H

#include "types.h"

/* ========================================================================= */
/* Puertos del controlador PS/2                                              */
/* ========================================================================= */
#define KB_DATA_PORT    0x60
#define KB_STATUS_PORT  0x64

/* Teclas especiales (códigos internos, no ASCII) */
#define KB_KEY_NONE        0
#define KB_KEY_ENTER       '\n'
#define KB_KEY_BACKSPACE   '\b'
#define KB_KEY_TAB         '\t'
#define KB_KEY_ESCAPE      27

/* Teclas extendidas (valores > 127 para no confundir con ASCII) */
#define KB_KEY_UP          128
#define KB_KEY_DOWN        129
#define KB_KEY_LEFT        130
#define KB_KEY_RIGHT       131
#define KB_KEY_HOME        132
#define KB_KEY_END         133
#define KB_KEY_DELETE      134
#define KB_KEY_PGUP        135
#define KB_KEY_PGDOWN      136

/* ========================================================================= */
/* API                                                                       */
/* ========================================================================= */

/**
 * Inicializa el driver de teclado.
 */
void keyboard_init(void);

/**
 * Lee un carácter del buffer del teclado.
 * Bloquea (HLT) hasta que haya entrada disponible.
 * @return Carácter ASCII leído.
 */
char keyboard_getchar(void);

/**
 * Verifica si hay entrada disponible en el buffer.
 * @return true si hay caracteres pendientes.
 */
bool keyboard_has_input(void);

/**
 * Procesa un scancode recibido (llamada desde el handler de IRQ1).
 * @param scancode El scancode crudo del teclado.
 */
void keyboard_process_scancode(uint8_t scancode);

/**
 * Configura la velocidad de repeticion y el retraso del teclado (Typematic).
 * @param delay Retraso antes de repetir (0-3).
 * @param rate  Velocidad de repeticion (0-31).
 */
void keyboard_set_typematic(uint8_t delay, uint8_t rate);

#endif /* ETEROS_KEYBOARD_H */
