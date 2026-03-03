#ifndef GFX_WINDOW_H
#define GFX_WINDOW_H

#include <types.h>

typedef struct window {
    int32_t id;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    uint32_t* buffer; /* RGBA or ARGB buffer */
    int32_t z_order;  /* Higher = on top */
    uint32_t flags;
    struct window* next;
} window_t;

/* Window Flags */
#define WIN_VISIBLE     (1 << 0)
#define WIN_FOCUSED     (1 << 1)
#define WIN_OPAQUE      (1 << 2)

/* API */
void compositor_init(void);
window_t* window_create(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t flags);
void window_destroy(window_t* win);
void window_invalidate(window_t* win);
void compositor_add_window(window_t* win);
void compositor_remove_window(window_t* win);
void compositor_render(void);

#endif
