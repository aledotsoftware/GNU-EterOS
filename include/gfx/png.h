#ifndef GFX_PNG_H
#define GFX_PNG_H

#include <types.h>

/* Very simple and naive PNG chunk decoder that returns raw decoded pixels */

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t* pixels; /* ARGB format */
} png_image_t;

png_image_t* png_decode(const uint8_t* data, size_t size);
void png_free(png_image_t* img);

#endif
