/*
 * éterOS - Window Manager (Compositor & Event System)
 * 
 * Implements a Z-ordered windowing system with event dispatching.
 */

#include <ui/window.h>
#include <ui/omni.h>
#include <types.h>
#include <string.h>
#include <framebuffer.h>
#include <keyboard.h>
#include <mouse.h>
#include <timer.h>
#include <hal.h>
#include <serial.h>

/* Window Pool & List */
static window_t window_pool[MAX_WINDOWS];
static window_t* windows_head = NULL; /* Bottom (Background) */
static window_t* windows_tail = NULL; /* Top (Active) */
static window_t* focused_window = NULL;

/* Input State */
static int32_t mouse_x = 512;
static int32_t mouse_y = 384;
static int32_t mouse_buttons = 0;

/* Event Queue */
typedef enum {
    WM_EVENT_MOUSE,
    WM_EVENT_KEY
} wm_event_type_t;

typedef struct {
    wm_event_type_t type;
    int32_t dx;
    int32_t dy;
    int32_t buttons;
    char key;
} wm_event_t;

#define WM_EVENT_QUEUE_SIZE 128
static wm_event_t wm_queue[WM_EVENT_QUEUE_SIZE];
static volatile int wm_q_head = 0;
static volatile int wm_q_tail = 0;

void wm_init(void) {
    memset(window_pool, 0, sizeof(window_pool));
    windows_head = NULL;
    windows_tail = NULL;
    focused_window = NULL;

    mouse_x = omni_get_width() / 2;
    mouse_y = omni_get_height() / 2;

    /* Register ISR callback */
    extern void wm_push_mouse_packet(int8_t dx, int8_t dy, uint8_t buttons);
    mouse_set_callback(wm_push_mouse_packet);
}

/* Internal Linked List Helpers */
static void list_append(window_t* win) {
    if (!windows_head) {
        windows_head = win;
        windows_tail = win;
        win->prev = NULL;
        win->next = NULL;
    } else {
        windows_tail->next = win;
        win->prev = windows_tail;
        win->next = NULL;
        windows_tail = win;
    }
}

static void list_remove(window_t* win) {
    if (win->prev) win->prev->next = win->next;
    else windows_head = win->next;

    if (win->next) win->next->prev = win->prev;
    else windows_tail = win->prev;

    win->next = NULL;
    win->prev = NULL;
}

window_t* wm_create_window(int32_t x, int32_t y, int32_t w, int32_t h, const char* title) {
    /* Find free slot */
    window_t* win = NULL;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!window_pool[i].active && window_pool[i].id == 0) { /* ID 0 implies free in this logic if we use 1-based IDs */
             /* Actually just check active flag? gui_demo set active=true manually. */
             /* Let's assume !active is free */
             /* Better: clear memory on destroy */
             win = &window_pool[i];
             win->id = i + 1;
             break;
        }
    }

    if (!win) return NULL;

    int new_id = win->id; /* Preserve ID found in loop */
    memset(win, 0, sizeof(window_t));
    win->id = new_id;
    
    win->active = true;
    win->bounds.x = x;
    win->bounds.y = y;
    win->bounds.w = w;
    win->bounds.h = h;
    win->bg_color = UI_COLOR_DARK;
    win->fg_color = UI_COLOR_WHITE;
    strlcpy(win->title, title, sizeof(win->title));
    
    list_append(win);
    wm_bring_to_front(win);

    return win;
}

void wm_destroy_window(window_t* win) {
    if (!win) return;
    list_remove(win);
    win->active = false;
    win->id = 0;
    if (focused_window == win) focused_window = windows_tail;
}

void wm_bring_to_front(window_t* win) {
    if (!win || win == windows_tail) {
        focused_window = win;
        return;
    }
    list_remove(win);
    list_append(win);
    focused_window = win;
}

/* Default frame drawing */
void wm_draw_window_frame(window_t* win) {
    if (!win->active) return;
    const int TITLE_H = 30;
    
    /* Shadow */
    omni_fill_rect_alpha(win->bounds.x + 4, win->bounds.y + 4, win->bounds.w, win->bounds.h, 0x000000, 0x60);
    
    /* Background */
    omni_fill_rect(win->bounds.x, win->bounds.y, win->bounds.w, win->bounds.h, win->bg_color);
    
    /* Title Bar */
    omni_fill_rect(win->bounds.x, win->bounds.y, win->bounds.w, TITLE_H, 0x303030);
    omni_draw_string(NULL, win->bounds.x + 8, win->bounds.y + 8, win->title, 0xFFFFFF, 0x303030);
    
    /* Border */
    omni_draw_rect(win->bounds.x, win->bounds.y, win->bounds.w, win->bounds.h, 0x505050);
}

void wm_draw_all(void) {
    omni_begin_frame();

    /* Iterate Bottom -> Top */
    window_t* curr = windows_head;
    while (curr) {
        if (curr->active) {
            if (curr->on_paint) {
                curr->on_paint(curr);
            } else {
                wm_draw_window_frame(curr);
            }
        }
        curr = curr->next;
    }

    /* Draw Cursor */
    omni_fill_rect(mouse_x, mouse_y, 4, 4, 0xFFFFFF);
    omni_draw_rect(mouse_x, mouse_y, 4, 4, 0x000000);

    framebuffer_flush();
}

void wm_draw_window(window_t* win) {
    /* For legacy calls or forced redraw of one window (not ideal in compositing) */
    /* In a proper compositor, we just mark dirty. Here we draw immediately. */
    if (win->on_paint) win->on_paint(win);
    else wm_draw_window_frame(win);
}

/* Event Queue Impl */
void wm_push_event(wm_event_t evt) {
    int next = (wm_q_head + 1) % WM_EVENT_QUEUE_SIZE;
    if (next != wm_q_tail) {
        wm_queue[wm_q_head] = evt;
        wm_q_head = next;
    }
}

void wm_push_mouse_packet(int8_t dx, int8_t dy, uint8_t buttons) {
    wm_event_t evt;
    evt.type = WM_EVENT_MOUSE;
    evt.dx = dx;
    evt.dy = dy;
    evt.buttons = buttons;
    wm_push_event(evt);
}

void wm_handle_input(int x, int y, int buttons, char key) {
    /* Not used directly anymore, pump_events handles it */
    (void)x; (void)y; (void)buttons; (void)key;
}

void wm_pump_events(void) {
    /* 1. Poll Keyboard */
    while (keyboard_has_input()) {
        char c = keyboard_getchar();
        wm_event_t evt;
        evt.type = WM_EVENT_KEY;
        evt.key = c;
        wm_push_event(evt);
    }
    
    /* 2. Process Queue */
    bool needs_redraw = false;
    
    while (wm_q_tail != wm_q_head) {
        wm_event_t evt = wm_queue[wm_q_tail];
        wm_q_tail = (wm_q_tail + 1) % WM_EVENT_QUEUE_SIZE;

        needs_redraw = true;

        if (evt.type == WM_EVENT_MOUSE) {
            mouse_x += evt.dx;
            mouse_y += evt.dy;

            /* Clamp */
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= (int)omni_get_width()) mouse_x = omni_get_width() - 1;
            if (mouse_y >= (int)omni_get_height()) mouse_y = omni_get_height() - 1;

            mouse_buttons = evt.buttons;

            /* Dispatch to Top-Most Window under cursor */
            window_t* target = windows_tail; /* Start at top */
            bool handled = false;

            while (target) {
                if (target->active &&
                    mouse_x >= target->bounds.x && mouse_x < target->bounds.x + target->bounds.w &&
                    mouse_y >= target->bounds.y && mouse_y < target->bounds.y + target->bounds.h) {

                    /* Hit! */
                    if (evt.buttons & 1) {
                         /* Click brings to front */
                         if (target != windows_tail) {
                             wm_bring_to_front(target);
                         }
                    }

                    if (target->on_mouse) {
                        /* Send local coordinates */
                        target->on_mouse(target, mouse_x - target->bounds.x, mouse_y - target->bounds.y, mouse_buttons);
                    }

                    handled = true;
                    break;
                }
                target = target->prev;
            }

        } else if (evt.type == WM_EVENT_KEY) {
            if (focused_window && focused_window->on_key) {
                focused_window->on_key(focused_window, evt.key);
            }
        }
    }

    if (needs_redraw) {
        wm_draw_all();
    } else {
        /* Idle */
        // task_sleep(10);
    }
}

/* Helpers */
void wm_print_at(window_t* win, int32_t x, int32_t y, const char* text) {
    if (!win) return;
    int tx = win->bounds.x + x;
    int ty = win->bounds.y + y;
    /* Clipping simplified for now */
    rect_t clip = win->bounds;
    omni_draw_string(&clip, tx, ty, text, win->fg_color, win->bg_color);
}

void wm_fill_rect(window_t* win, rect_t rect, uint32_t color) {
    if (!win) return;
    int rx = win->bounds.x + rect.x;
    int ry = win->bounds.y + rect.y;
    /* Clip against window bounds */
    int min_x = win->bounds.x;
    int min_y = win->bounds.y;
    int max_x = win->bounds.x + win->bounds.w;
    int max_y = win->bounds.y + win->bounds.h;
    
    if (rx < min_x) { rect.w -= (min_x - rx); rx = min_x; }
    if (ry < min_y) { rect.h -= (min_y - ry); ry = min_y; }
    if (rx + rect.w > max_x) rect.w = max_x - rx;
    if (ry + rect.h > max_y) rect.h = max_y - ry;

    if (rect.w > 0 && rect.h > 0)
        omni_fill_rect(rx, ry, rect.w, rect.h, color);
}

void wm_move_window(window_t* win, int32_t dx, int32_t dy) {
    if (win) {
        win->bounds.x += dx;
        win->bounds.y += dy;
    }
}

void wm_redraw_desktop(void) {
    wm_draw_all();
}
