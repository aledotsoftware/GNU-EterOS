#ifndef UI_WINDOW_H
#define UI_WINDOW_H

#include <ui/omni.h>

#define MAX_WINDOWS 32

/* Forward declaration for callbacks */
struct window;

typedef struct window {
    int id;
    rect_t bounds;
    char title[32];
    uint32_t bg_color;
    uint32_t fg_color;
    bool active;

    /* Z-Order Linked List */
    struct window* next; /* Above this window */
    struct window* prev; /* Below this window */

    /* Callbacks */
    void (*on_paint)(struct window* win);
    void (*on_key)(struct window* win, char key);
    void (*on_mouse)(struct window* win, int x, int y, int buttons);
} window_t;

/* Gestión de ventanas */
void wm_init(void);
window_t* wm_create_window(int32_t x, int32_t y, int32_t w, int32_t h, const char* title);
void wm_destroy_window(window_t* win);

void wm_draw_all(void);
void wm_draw_window(window_t* win);
void wm_draw_window_frame(window_t* win); /* Helper to draw standard frame */

/* Input & Event Loop */
void wm_handle_input(int x, int y, int buttons, char key); /* Called by ISR/Driver */
void wm_pump_events(void); /* Main loop helper */

/* Z-Order Management */
void wm_bring_to_front(window_t* win);

/* Helpers de dibujo contextual */
void wm_print_at(window_t* win, int32_t x, int32_t y, const char* text);
void wm_fill_rect(window_t* win, rect_t rect, uint32_t color);
void wm_move_window(window_t* win, int32_t dx, int32_t dy);
void wm_redraw_desktop(void);

#endif
