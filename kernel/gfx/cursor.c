#include <gfx/cursor.h>
#include <gfx/gfx.h>
#include <framebuffer.h>
#include <mouse.h>

static int32_t cursor_x = 0;
static int32_t cursor_y = 0;
static uint32_t screen_w = 0;
static uint32_t screen_h = 0;

/* Puntero de ratón clásico 12x19 */
/* . = Transparente, # = Negro (Borde), @ = Blanco (Interior) */
static const char* cursor_bitmap[] = {
    "#...........",
    "##..........",
    "#@#.........",
    "#@@#........",
    "#@@@#.......",
    "#@@@@#......",
    "#@@@@@#.....",
    "#@@@@@@#....",
    "#@@@@@@@#...",
    "#@@@@@@@@#..",
    "#@@@@@@@@@#.",
    "#@@@@@@#####",
    "#@@#@#@#....",
    "#@#..#@#....",
    "##....#@#...",
    "#......#@#..",
    "........#@#.",
    ".........##.",
    "............"
};

#define CURSOR_W 12
#define CURSOR_H 19

/* Callback del driver del ratón */
static void on_mouse_event(int8_t dx, int8_t dy, uint8_t buttons) {
    (void)buttons; /* TODO: handle clicks */
    gfx_cursor_move(dx, dy);
}

void gfx_cursor_init(void) {
    screen_w = framebuffer_get_width();
    screen_h = framebuffer_get_height();
    
    /* Centrar inicialmente */
    cursor_x = screen_w / 2;
    cursor_y = screen_h / 2;
    
    /* Registrar el callback con el driver del ratón */
    mouse_set_callback(on_mouse_event);
}

void gfx_cursor_move(int32_t dx, int32_t dy) {
    cursor_x += dx;
    cursor_y += dy;
    
    /* Limitar a los bordes de la pantalla */
    if (cursor_x < 0) cursor_x = 0;
    if (cursor_y < 0) cursor_y = 0;
    if (cursor_x >= (int32_t)screen_w) cursor_x = screen_w - 1;
    if (cursor_y >= (int32_t)screen_h) cursor_y = screen_h - 1;
}

void gfx_cursor_get_pos(int32_t* x, int32_t* y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

void gfx_cursor_draw(void) {
    /* Dibujar el puntero píxel a píxel sobre el buffer */
    for (int j = 0; j < CURSOR_H; j++) {
        for (int i = 0; i < CURSOR_W; i++) {
            int32_t px = cursor_x + i;
            int32_t py = cursor_y + j;
            
            /* No dibujar fuera de pantalla */
            if (px >= (int32_t)screen_w || py >= (int32_t)screen_h) continue;
            
            char pixel = cursor_bitmap[j][i];
            if (pixel == '#') {
                framebuffer_putpixel(px, py, 0x000000); /* Negro */
            } else if (pixel == '@') {
                framebuffer_putpixel(px, py, 0xFFFFFF); /* Blanco */
            }
        }
    }
    
    /* Marcar el área del cursor como sucia para redibujar */
    gfx_add_dirty_rect(cursor_x, cursor_y, CURSOR_W, CURSOR_H);
}
