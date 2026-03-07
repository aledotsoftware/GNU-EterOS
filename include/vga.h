/**
 * éterOS - Driver VGA en Modo Texto
 * 
 * Interfaz para el subsistema de video VGA legacy.
 * Utiliza el buffer de texto mapeado en memoria en 0xB8000.
 */

#ifndef ETEROS_VGA_H
#define ETEROS_VGA_H

#include "types.h"
#include "boot.h"

/* ========================================================================= */
/* Constantes VGA                                                            */
/* ========================================================================= */
#ifndef VGA_BUFFER_ADDR
#define VGA_BUFFER_ADDR     0xB8000
#endif
#define VGA_WIDTH           80
#define VGA_HEIGHT          25
#define VGA_SIZE            (VGA_WIDTH * VGA_HEIGHT)

/* ========================================================================= */
/* Colores VGA                                                               */
/* ========================================================================= */
typedef enum {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_YELLOW        = 14,
    VGA_COLOR_WHITE         = 15,
} vga_color_t;

typedef void (*terminal_hook_t)(char c);
void terminal_set_hook(terminal_hook_t hook);

/* ========================================================================= */
/* Funciones inline para construir entradas VGA                              */
/* ========================================================================= */

/**
 * Crea un byte de atributo VGA combinando color de fondo y texto.
 */
static inline uint8_t vga_entry_color(vga_color_t fg, vga_color_t bg) {
    return (uint8_t)(fg | (bg << 4));
}

/**
 * Crea una entrada VGA de 16 bits (carácter + atributo).
 */
static inline uint16_t vga_entry(unsigned char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

/* ========================================================================= */
/* API del driver VGA                                                        */
/* ========================================================================= */

/**
 * Inicializa el terminal VGA (limpia la pantalla y posiciona el cursor).
 * Recibe informacion de arranque para configurar framebuffer si esta disponible.
 */
void terminal_initialize(boot_info_t* boot_info);

/**
 * Switch terminal to framebuffer mode (deferred init).
 * Must be called AFTER PMM, VMM, and mm_init are ready.
 */
void terminal_switch_to_framebuffer(boot_info_t* boot_info);

/**
 * Establece el color actual del terminal.
 */
void terminal_set_color(uint8_t color);

/**
 * Escribe un carácter en la posición actual del terminal.
 */
void terminal_putchar(char c);

/**
 * Escribe una cadena terminada en null en el terminal.
 */
void terminal_write_string(const char* str);

/**
 * Escribe una cadena con un color específico (sin modificar el color global).
 */
void terminal_write_colored(const char* str, vga_color_t fg, vga_color_t bg);

/**
 * Limpia la pantalla del terminal.
 */
void terminal_clear(void);

/**
 * Fuerza el volcado del buffer de terminal a la pantalla (Framebuffer flush).
 */
void terminal_flush(void);

/**
 * Mueve el cursor a una posición específica.
 */
void terminal_set_cursor(size_t x, size_t y);

/**
 * Desplaza el contenido de la pantalla hacia arriba una línea.
 */
void terminal_scroll(void);

/**
 * Habilita o deshabilita la salida silenciosa del terminal (para boot logo).
 */
void terminal_set_silent(bool silent);

#endif /* ETEROS_VGA_H */
