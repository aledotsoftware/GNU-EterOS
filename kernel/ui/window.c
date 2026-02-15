/*
 * Window Manager muy simple.
 * Maneja ventanas flotantes (direct-draw).
 */
#include <ui/window.h>
#include <types.h>
#include <string.h>
#include <framebuffer.h>
#include <ui/omni.h>

/* #define MAX_WINDOWS 10 -- Now in ui/window.h */

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
    
    const int TITLE_H = 30;
    
    /* 0. Sombra (Offset +4, +4) */
    omni_fill_rect(win->bounds.x + 4, win->bounds.y + 4,
                   win->bounds.w, win->bounds.h,
                   0x000000); /* Black shadow */

    /* 1. Fondo */
    /* rect local -> global */
    omni_fill_rect(win->bounds.x, win->bounds.y,
                   win->bounds.w, win->bounds.h,
                   win->bg_color);
                     
    /* 2. Barra título (Más alta para Touch - 30px) */
    uint32_t title_color = UI_COLOR_CYAN; 
    
    omni_fill_rect(win->bounds.x, win->bounds.y,
                   win->bounds.w, TITLE_H,
                   title_color);
                     
    /* Título (Centrado verticalmente en 30px) */
    omni_draw_string(NULL, win->bounds.x + 8, win->bounds.y + 7, win->title, UI_COLOR_BLACK, title_color);
    
    /* 3. Botón Cerrar [X] (Touch Target grande - Magnet Effect) */
    /* El botón visual es 20x20, pero el área de click será mayor */
    int btn_size = 20;
    int btn_margin = (TITLE_H - btn_size) / 2; /* Centrado: (30-20)/2 = 5 */
    int btn_x = win->bounds.x + win->bounds.w - btn_size - btn_margin;
    int btn_y = win->bounds.y + btn_margin;
    
    omni_fill_rect(btn_x, btn_y, btn_size, btn_size, 0xFF4040); /* Rojo suave */
    
    /* Dibujar X */
    for(int i=4; i<=15; i++) {
        omni_draw_pixel(btn_x + i, btn_y + i, 0xFFFFFF);
        omni_draw_pixel(btn_x + 19 - i, btn_y + i, 0xFFFFFF);
    }
    
    /* 4. Borde */
    omni_draw_rect(win->bounds.x, win->bounds.y, win->bounds.w, win->bounds.h, 0xC0C0C0);
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

    const int TITLE_H = 30;

    /* Offset por barra de título */
    int32_t abs_x = win->bounds.x + x;
    int32_t abs_y = win->bounds.y + TITLE_H + y;
    
    /* Clipping Rect: Content Area */
    rect_t clip;
    clip.x = win->bounds.x;
    clip.y = win->bounds.y + TITLE_H;
    clip.w = win->bounds.w;
    clip.h = win->bounds.h - TITLE_H;

    omni_draw_string(&clip, abs_x, abs_y, text, win->fg_color, win->bg_color);
}

void wm_fill_rect(window_t* win, rect_t rect, uint32_t color) {
    if (!win->active) return;
    
    const int TITLE_H = 30;
    
    /* Offset por barra de título */
    int32_t abs_x = win->bounds.x + rect.x;
    int32_t abs_y = win->bounds.y + TITLE_H + rect.y;

    /* Límites del área de contenido */
    int32_t min_x = win->bounds.x;
    int32_t min_y = win->bounds.y + TITLE_H;
    int32_t max_x = win->bounds.x + win->bounds.w;
    int32_t max_y = win->bounds.y + win->bounds.h;

    /* Clipping Rectangle Intersection */
    if (abs_x < min_x) {
        rect.w -= (min_x - abs_x);
        abs_x = min_x;
    }
    if (abs_y < min_y) {
        rect.h -= (min_y - abs_y);
        abs_y = min_y;
    }

    if (abs_x + rect.w > max_x) {
        rect.w = max_x - abs_x;
    }
    if (abs_y + rect.h > max_y) {
        rect.h = max_y - abs_y;
    }

    /* Si no queda nada, salir */
    if (rect.w <= 0 || rect.h <= 0) return;
    
    omni_fill_rect(abs_x, abs_y, rect.w, rect.h, color);
}

void wm_move_window(window_t* win, int32_t dx, int32_t dy) {
    if (!win->active) return;
    win->bounds.x += dx;
    win->bounds.y += dy;
    
    /* Full Redraw (Expensive but safe for now) */
    wm_redraw_desktop();
}

void wm_redraw_desktop(void) {
    /* 1. Fondo "Desktop" (Dark Teal) */
    omni_fill_rect(0, 0, omni_get_width(), omni_get_height(), 0x002040);
    
    /* 2. Grid Effect */
    /* Draw horizontal lines */
    for (uint32_t y = 0; y < 768; y += 40) {
        omni_fill_rect(0, y, 1024, 1, 0x003050);
    }
    /* Draw vertical lines */
    for (uint32_t x = 0; x < 1024; x += 40) {
        omni_fill_rect(x, 0, 1, 768, 0x003050);
    }

    /* 3. Redraw all windows */
    wm_draw_all();
    
    /* 4. Flush backbuffer to screen */
    framebuffer_flush();
}
