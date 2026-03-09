#ifndef GFX_H
#define GFX_H

#include <types.h>
#include <boot.h>

/* Estructura para Rectángulos */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} rect_t;

/* Estructura para Puntos (Batch Drawing) */
typedef struct {
    int32_t x;
    int32_t y;
    uint32_t color;
} gfx_point_t;

/* Inicialización */
void gfx_init(boot_info_t* boot_info);

/* Primitivas de Dibujado (escriben en Back Buffer) */
void gfx_put_pixel(int32_t x, int32_t y, uint32_t color);
void gfx_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void gfx_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void gfx_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void gfx_draw_pixels(const gfx_point_t* pixels, size_t count);

/* Damage Tracking (Dirty Rectangles) */
void gfx_add_dirty_rect(int32_t x, int32_t y, int32_t w, int32_t h);
void gfx_present(void);
int gfx_get_dirty_rect(rect_t* out_rect);

#endif /* GFX_H */
