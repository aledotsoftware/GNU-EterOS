/*
 * Window Manager muy simple.
 * Maneja ventanas flotantes (direct-draw).
 */
#include <ui/window.h>
#include <types.h>
#include <string.h>
#include <framebuffer.h>

#define MAX_WINDOWS 10

static window_t current_windows[MAX_WINDOWS];
static int window_count = 0;

void wm_init(void) {
    memset(current_windows, 0, sizeof(current_windows));
    window_count = 0;
}

window_t* wm_create_window(int32_t x, int32_t y, int32_t w, int32_t h, const char* title) {
    if (window_count >= MAX_WINDOWS) return NULL;
    
    window_t* win = &current_windows[window_count++];
    win->active = true;
    win->bounds.x = x;
    win->bounds.y = y;
    win->bounds.w = w;
    win->bounds.h = h;
    win->bg_color = UI_COLOR_DARK;
    win->fg_color = UI_COLOR_WHITE;
    strlcpy(win->title, title, sizeof(win->title));
    
    /* Dibujar inicialmente */
    wm_draw_window(win);
    return win;
}

void wm_draw_window(window_t* win) {
    if (!win->active) return;
    
    /* 1. Fondo */
    /* rect local -> global */
    framebuffer_rect((uint32_t)win->bounds.x, (uint32_t)win->bounds.y,
                     (uint32_t)win->bounds.w, (uint32_t)win->bounds.h,
                     win->bg_color);
                     
    /* 2. Barra título */
    framebuffer_rect((uint32_t)win->bounds.x, (uint32_t)win->bounds.y,
                     (uint32_t)win->bounds.w, 20,
                     UI_COLOR_GREY);
                     
    /* Título */
    /* Ajuste: offset 4px, centrado 2px */
    ui_draw_string(NULL, win->bounds.x + 4, win->bounds.y + 2, win->title, UI_COLOR_BLACK, UI_COLOR_GREY);
    
    /* 3. Borde */
    ui_draw_rect(&win->bounds, UI_COLOR_CYAN);
}

void wm_draw_all(void) {
    /* Redibujar ordenado (primero 0, último encima) */
    for (int i = 0; i < window_count; i++) {
        wm_draw_window(&current_windows[i]);
    }
}

/* Draws text relative to window content area (below title bar) */
void wm_print_at(window_t* win, int32_t x, int32_t y, const char* text) {
    if (!win->active) return;

    /* Offset por barra de título (20px) */
    int32_t abs_x = win->bounds.x + x;
    int32_t abs_y = win->bounds.y + 20 + y;
    
    /* Clipping básico */
    if (abs_x < win->bounds.x || abs_y < win->bounds.y + 20 ||
        abs_x + (int32_t)(strlen(text) * 8) > win->bounds.x + win->bounds.w ||
        abs_y + 16 > win->bounds.y + win->bounds.h) {
        return; /* Fuera de limites */
    }
    
    ui_draw_string(NULL, abs_x, abs_y, text, win->fg_color, win->bg_color);
}

void wm_fill_rect(window_t* win, rect_t rect, uint32_t color) {
    if (!win->active) return;
    
     /* Offset por barra de título (20px) */
    int32_t abs_x = win->bounds.x + rect.x;
    int32_t abs_y = win->bounds.y + 20 + rect.y;

    /* Clipping muy básico */
    if (abs_x < win->bounds.x) abs_x = win->bounds.x;
    if (abs_y < win->bounds.y + 20) abs_y = win->bounds.y + 20;
    
    /* Implementar clipping real sería mejor, pero por ahora... */
    framebuffer_rect((uint32_t)abs_x, (uint32_t)abs_y, (uint32_t)rect.w, (uint32_t)rect.h, color);
}
