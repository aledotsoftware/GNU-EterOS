#include <gfx/gfx.h>
#include <shell.h>
#include <vga.h>

/* Dummy data for simulation */
static gfx_point_t constellation_stars[64];

static void init_stars(void) {
    for (int i = 0; i < 64; i++) {
        constellation_stars[i].x = (i * 10) % 320;
        constellation_stars[i].y = (i * 5) % 200;
        constellation_stars[i].color = 0xFFFFFFFF; /* White */
    }
}

void gui_demo_run(void) {
    init_stars();

    terminal_write_string("Drawing stars with batch optimization...\n");

    /* Capa 0: Neural Ambient (Stars) */
    /* ⚡ BOLT OPTIMIZATION: Use batch drawing to minimize dirty rect updates */
    gfx_draw_pixels(constellation_stars, 64);

    /* Force present to ensure it's drawn */
    gfx_present();

    terminal_write_string("Done.\n");
}
