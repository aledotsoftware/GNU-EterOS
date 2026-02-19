#include <ui/window.h>
#include <ui/omni.h>
#include <string.h>

/* Doom Stub - Placeholder for when the real engine is missing */

static void doom_stub_on_paint(window_t* win) {
    if (!win) return;

    /* Clear Background */
    omni_fill_rect(win->bounds.x, win->bounds.y, win->bounds.w, win->bounds.h, 0x000000);

    /* Draw Header */
    omni_fill_rect(win->bounds.x, win->bounds.y, win->bounds.w, 30, 0x300000);
    omni_draw_string(NULL, win->bounds.x + 10, win->bounds.y + 8, "DOOM (Stub Mode)", 0xFF0000, 0x300000);

    /* Draw Message */
    omni_draw_string(NULL, win->bounds.x + 20, win->bounds.y + 60, "DOOM Engine Missing", 0xFF0000, 0x000000);
    omni_draw_string(NULL, win->bounds.x + 20, win->bounds.y + 80, "Please check installation.", 0xFFFFFF, 0x000000);

    /* Draw fake gun */
    int cx = win->bounds.x + win->bounds.w / 2;
    int by = win->bounds.y + win->bounds.h;
    omni_fill_rect(cx - 20, by - 60, 40, 60, 0x404040);
    omni_fill_rect(cx - 5, by - 60, 10, 30, 0x101010);
}

void doom_run(void) {
    window_t* win = wm_create_window(50, 50, 640, 400, "DOOM");
    if (win) {
        win->on_paint = doom_stub_on_paint;
    }
}
