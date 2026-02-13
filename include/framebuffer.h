#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "boot.h"
#include "types.h"

/* Inicializa el framebuffer con la info del bootloader */
void framebuffer_init(boot_info_t* info);

/* Limpia la pantalla con un color (0xAARRGGBB) */
void framebuffer_clear(uint32_t color);

/* Dibuja un pixel */
void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color);

void framebuffer_flush(void);
void framebuffer_enable_double_buffer(void);

/* Dibuja un rectángulo */
void framebuffer_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/* Dibuja un caracter (usando fuente interna) */
void framebuffer_putchar(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);

/* Backend para la terminal */
void terminal_framebuffer_write(const char* data, size_t size);

#endif
