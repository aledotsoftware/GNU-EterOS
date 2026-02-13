#ifndef UI_WINDOW_H
#define UI_WINDOW_H

#include <ui/primitives.h>

#define MAX_WINDOWS 10

typedef struct {
    int id;
    rect_t bounds;
    char title[32];
    uint32_t bg_color;
    uint32_t fg_color;
    bool active;
    /* Por simplicidad inicial: buffer directo a pantalla */
} window_t;

/* Gestión de ventanas */
void wm_init(void);
window_t* wm_create_window(int32_t x, int32_t y, int32_t w, int32_t h, const char* title);
void wm_draw_all(void);
void wm_draw_window(window_t* win);

/* Helpers de dibujo contextual */
void wm_print_at(window_t* win, int32_t x, int32_t y, const char* text);
void wm_fill_rect(window_t* win, rect_t rect, uint32_t color);
void wm_move_window(window_t* win, int32_t dx, int32_t dy);
void wm_redraw_desktop(void);

#endif
