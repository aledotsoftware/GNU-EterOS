/**
 * éterOS - Implementación del Driver VGA
 * 
 * Driver para el modo texto VGA legacy.
 * Escribe directamente al buffer de video mapeado en 0xB8000.
 * Soporta scroll automático, cursor hardware, y colores.
 */

#include <vga.h>
#include <io.h>
#include <string.h>

/* ========================================================================= */
/* Estado interno del terminal                                               */
/* ========================================================================= */
static size_t   terminal_row;
static size_t   terminal_col;
static uint8_t  terminal_color;
static uint16_t* terminal_buffer;

/* ========================================================================= */
/* Funciones de cursor hardware VGA                                          */
/* ========================================================================= */

/**
 * Actualiza la posición del cursor hardware VGA.
 * Usa los puertos 0x3D4 (índice) y 0x3D5 (datos) del controlador CRT.
 */
static void terminal_update_cursor(void) {
    uint16_t pos = (uint16_t)(terminal_row * VGA_WIDTH + terminal_col);

    outb(0x3D4, 0x0F);                 /* Byte bajo de la posición */
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);                 /* Byte alto de la posición */
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/**
 * Habilita el cursor hardware VGA con forma de bloque.
 */
static void terminal_enable_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 0);     /* Scanline inicio = 0 */
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);     /* Scanline fin = 15 */
}

/* ========================================================================= */
/* Implementación de la API pública                                          */
/* ========================================================================= */

void terminal_initialize(void) {
    terminal_row    = 0;
    terminal_col    = 0;
    terminal_color  = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = (uint16_t*)VGA_BUFFER_ADDR;

    /* Limpiar toda la pantalla */
    memset16(terminal_buffer, vga_entry(' ', terminal_color), VGA_SIZE);

    terminal_enable_cursor();
    terminal_update_cursor();
}

void terminal_set_color(uint8_t color) {
    terminal_color = color;
}

void terminal_scroll(void) {
    /* Move all lines one position up */
    memmove(terminal_buffer, terminal_buffer + VGA_WIDTH, (VGA_SIZE - VGA_WIDTH) * sizeof(uint16_t));

    /* Clear the last line */
    const size_t last_line_offset = VGA_SIZE - VGA_WIDTH;
    memset16(terminal_buffer + last_line_offset, vga_entry(' ', terminal_color), VGA_WIDTH);

    terminal_row = VGA_HEIGHT - 1;
}

static void _terminal_putchar(char c) {
    switch (c) {
        case '\n':
            /* Nueva línea */
            terminal_col = 0;
            terminal_row++;
            break;
        
        case '\r':
            /* Retorno de carro */
            terminal_col = 0;
            break;
        
        case '\t':
            /* Tabulación (4 espacios) */
            terminal_col = (terminal_col + 4) & ~3;
            if (terminal_col >= VGA_WIDTH) {
                terminal_col = 0;
                terminal_row++;
            }
            break;
        
        case '\b':
            /* Retroceso */
            if (terminal_col > 0) {
                terminal_col--;
                size_t index = terminal_row * VGA_WIDTH + terminal_col;
                terminal_buffer[index] = vga_entry(' ', terminal_color);
            }
            break;
        
        default:
            /* Carácter normal */
            {
                size_t index = terminal_row * VGA_WIDTH + terminal_col;
                terminal_buffer[index] = vga_entry(c, terminal_color);
                terminal_col++;
                if (terminal_col >= VGA_WIDTH) {
                    terminal_col = 0;
                    terminal_row++;
                }
            }
            break;
    }

    /* Scroll si llegamos al final de la pantalla */
    if (terminal_row >= VGA_HEIGHT) {
        terminal_scroll();
    }
}

void terminal_putchar(char c) {
    _terminal_putchar(c);
    terminal_update_cursor();
}

void terminal_write_string(const char* str) {
    while (*str) {
        _terminal_putchar(*str++);
    }
    terminal_update_cursor();
}

void terminal_write_colored(const char* str, vga_color_t fg, vga_color_t bg) {
    uint8_t saved_color = terminal_color;
    terminal_color = vga_entry_color(fg, bg);
    terminal_write_string(str);
    terminal_color = saved_color;
}

void terminal_clear(void) {
    memset16(terminal_buffer, vga_entry(' ', terminal_color), VGA_SIZE);
    terminal_row = 0;
    terminal_col = 0;
    terminal_update_cursor();
}

void terminal_set_cursor(size_t x, size_t y) {
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        terminal_col = x;
        terminal_row = y;
        terminal_update_cursor();
    }
}
