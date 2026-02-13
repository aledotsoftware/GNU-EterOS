/*
 * Primitivas gráficas para la UI del sistema.
 * Implementa clipping básico para dibujar dentro de ventanas.
 */
#include <ui/primitives.h>
#include <framebuffer.h>

/* Helper para pintar directo en memoria */
void ui_draw_pixel(int32_t x, int32_t y, uint32_t color) {
    if (x < 0 || y < 0) return;
    framebuffer_putpixel((uint32_t)x, (uint32_t)y, color);
}

void ui_draw_pixel_clipped(const rect_t* clip, int32_t x, int32_t y, uint32_t color) {
    if (!clip) {
        ui_draw_pixel(x, y, color);
        return;
    }
    
    if (x >= clip->x && x < clip->x + clip->w &&
        y >= clip->y && y < clip->y + clip->h) {
        ui_draw_pixel(x, y, color);
    }
}

void ui_fill_rect(const rect_t* rect, uint32_t color) {
    if (!rect) return;

    /* Optimización: fill_rect de framebuffer si no hay clipping (o si el rect YA está clippeado) */
    /* Por simplicidad, llamamos al framebufffer_rect directo, asumiendo coordenadas globales */
    framebuffer_rect((uint32_t)rect->x, (uint32_t)rect->y, (uint32_t)rect->w, (uint32_t)rect->h, color);
}

void ui_draw_rect(const rect_t* rect, uint32_t color) {
    if (!rect) return;
    
    /* Dibujar bordes */
    /* Arriba */
    framebuffer_rect(rect->x, rect->y, rect->w, 1, color);
    /* Abajo */
    framebuffer_rect(rect->x, rect->y + rect->h - 1, rect->w, 1, color);
    /* Izquierda */
    framebuffer_rect(rect->x, rect->y, 1, rect->h, color);
    /* Derecha */
    framebuffer_rect(rect->x + rect->w - 1, rect->y, 1, rect->h, color);
}

void ui_draw_char(const rect_t* clip, int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg) {
    /* Por ahora usamos el putchar global, pero con coordenadas relativas */
    /* Para implementar clipping real, necesitamos acceso a font8x16 aquí o una función putchar_clipped */
    /* Usaremos framebuffer_putchar que NO TIENE CLIPPING todavía, así que cuidado */
    
    if (clip) {
        if (x < clip->x || y < clip->y || x + 8 > clip->x + clip->w || y + 16 > clip->y + clip->h) {
            return; /* Fuera de limites */
        }
    }
    
    framebuffer_putchar(c, (uint32_t)x, (uint32_t)y, fg, bg);
}

void ui_draw_string(const rect_t* clip, int32_t x, int32_t y, const char* text, uint32_t fg, uint32_t bg) {
    if (!text) return;
    int32_t cur_x = x;
    while (*text) {
        ui_draw_char(clip, cur_x, y, *text, fg, bg);
        cur_x += 8;
        text++;
    }
}
