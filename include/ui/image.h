#ifndef UI_IMAGE_H
#define UI_IMAGE_H

#include <types.h>

/**
 * Dibuja una imagen desde el initrd en las coordenadas (x, y).
 * Soporta formato ETER-IMG (8 bytes header + RAW BGRA).
 */
void ui_draw_image(const char* filename, int x, int y);

#endif
