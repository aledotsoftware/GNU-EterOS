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
void framebuffer_flush_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void framebuffer_enable_double_buffer(void);

/* Dibuja un rectángulo */
void framebuffer_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

uint32_t framebuffer_get_width(void);
uint32_t framebuffer_get_height(void);

/* Dibuja un caracter (usando fuente interna) */
void framebuffer_putchar(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);

/* Backend para la terminal */
void terminal_framebuffer_write(const char* data, size_t size);

/* Acceso directo al buffer (para optimizaciones Omni) */
uint32_t* framebuffer_get_buffer(void);
uint32_t framebuffer_get_pitch(void);
uint32_t framebuffer_get_bpp(void);

#endif
