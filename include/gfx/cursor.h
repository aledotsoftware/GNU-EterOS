#ifndef GFX_CURSOR_H
#define GFX_CURSOR_H

#include <types.h>

/**
 * Inicializa el sistema de cursor gráfico.
 */
void gfx_cursor_init(void);

/**
 * Actualiza la posición del cursor basada en deltas del mouse.
 */
void gfx_cursor_move(int32_t dx, int32_t dy);

/**
 * Obtiene la posición actual del cursor.
 */
void gfx_cursor_get_pos(int32_t* x, int32_t* y);

/**
 * Dibuja el cursor en el buffer actual.
 * Debe llamarse antes de gfx_present().
 */
void gfx_cursor_draw(void);

#endif
