/**
 * éterOS — Marea Shell (Wayland-inspired Desktop Environment)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Proceso userspace que actúa como compositor y window manager.
 * Se conecta al framebuffer via /dev/fb0 mmap y lee input de
 * /dev/tty y /dev/input/mouse0.
 *
 * Diseño: Dark-mode premium con glassmorphism, inspirado en
 * la estética moderna de GNOME/KDE Plasma sobre Wayland.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>

#include "font.h"

/* ========================================================================= */
/* Build Config                                                              */
/* ========================================================================= */

#define O_RDWR   0x0002
#define O_CREAT  0x0040
#define O_RDONLY 0x0000

#define FBIOGET_VSCREENINFO 0x4600

/* ========================================================================= */
/* Color Palette — Marea Dark Theme                                          */
/* ========================================================================= */

#define COL_BG_PRIMARY    0xFF0D1117   /* Desktop background */
#define COL_BG_SURFACE    0xFF161B22   /* Window body */
#define COL_BG_ELEVATED   0xFF1C2128   /* Taskbar, panels */
#define COL_BG_TITLEBAR   0xFF21262D   /* Window titlebar */
#define COL_ACCENT        0xFF58A6FF   /* Active elements */
#define COL_ACCENT_GLOW   0xFF1F6FEB   /* Selection, focus */
#define COL_TEXT_PRIMARY   0xFFE6EDF3   /* Main text */
#define COL_TEXT_SECONDARY 0xFF8B949E   /* Muted text */
#define COL_BORDER        0xFF30363D   /* Borders */
#define COL_SUCCESS       0xFF3FB950   /* Positive */
#define COL_DANGER        0xFFF85149   /* Close button */
#define COL_WARNING       0xFFD29922   /* Minimize */
#define COL_GLASS_BG      0xD9161B22   /* 85% opaque glass */
#define COL_CURSOR_FILL   0xFFFFFFFF
#define COL_CURSOR_BORDER 0xFF000000
#define COL_TASKBAR_BG    0xF00D1117   /* 94% opaque */
#define COL_MENU_BG       0xF5161B22   /* 96% opaque */
#define COL_MENU_HOVER    0xFF21262D
#define COL_TERM_BG       0xFF0D1117
#define COL_TERM_FG       0xFFE6EDF3
#define COL_TERM_PROMPT_USER 0xFF3FB950
#define COL_TERM_PROMPT_DIR  0xFF58A6FF

/* Gradient stops for desktop wallpaper */
#define GRAD_TOP    0xFF0D1117
#define GRAD_MID    0xFF161B22
#define GRAD_BOTTOM 0xFF1A1E2E

/* ========================================================================= */
/* Framebuffer & System State                                                */
/* ========================================================================= */

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
} fb_info_t;

static uint8_t* fb_ptr = NULL;
static fb_info_t fb_info;

#define TASKBAR_HEIGHT 44
#define TITLEBAR_HEIGHT 32
#define BORDER_RADIUS 8
#define MENU_WIDTH 260
#define MENU_ITEM_HEIGHT 36
#define MENU_PADDING 8

/* ========================================================================= */
/* Window Management                                                         */
/* ========================================================================= */

#define MAX_WINDOWS 8

typedef struct {
    int id;
    int x, y, w, h;
    int visible;
    int focused;
    int minimized;
    char title[32];

    /* Terminal state (inline for demo) */
    int term_cx, term_cy;
    int term_cols, term_rows;
    char input_buf[256];
    int input_len;
} marea_window_t;

static marea_window_t windows[MAX_WINDOWS];
static int window_count = 0;
static int focused_window = -1;

/* ========================================================================= */
/* Mouse State                                                               */
/* ========================================================================= */

typedef struct {
    uint16_t type;
    uint16_t code;
    int32_t  value;
} input_event_t;

#define EV_REL   0x02
#define EV_KEY   0x01
#define REL_X    0x00
#define REL_Y    0x01
#define BTN_LEFT 0x110

static int mouse_x, mouse_y;
static int mouse_btn = 0;

/* Cursor save/restore */
#define CURSOR_W 12
#define CURSOR_H 18
static uint32_t cursor_save[CURSOR_W * CURSOR_H];

/* ========================================================================= */
/* Menu State                                                                */
/* ========================================================================= */

static int menu_open = 0;
static int menu_x, menu_y;

typedef struct {
    const char* label;
    const char* icon;   /* Emoji-style single char */
} menu_item_t;

static const menu_item_t menu_items[] = {
    { "Terminal",       ">" },
    { "Archivos",       "D" },
    { "Configuracion",  "S" },
    { "Acerca de",      "i" },
    { "",               ""  },       /* separator */
    { "Reiniciar",      "R" },
    { "Apagar",         "P" },
};

#define MENU_ITEM_COUNT (sizeof(menu_items) / sizeof(menu_items[0]))

/* ========================================================================= */
/* Low-Level Drawing                                                         */
/* ========================================================================= */

static inline void put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)fb_info.width || y < 0 || y >= (int)fb_info.height) return;
    if (fb_info.bpp == 32) {
        uint32_t* p = (uint32_t*)(fb_ptr + y * fb_info.pitch + x * 4);
        *p = color;
    } else if (fb_info.bpp == 24) {
        uint8_t* p = fb_ptr + y * fb_info.pitch + x * 3;
        p[0] = color & 0xFF;
        p[1] = (color >> 8) & 0xFF;
        p[2] = (color >> 16) & 0xFF;
    }
}

static inline uint32_t get_pixel(int x, int y) {
    if (x < 0 || x >= (int)fb_info.width || y < 0 || y >= (int)fb_info.height) return 0;
    if (fb_info.bpp == 32) {
        return *(uint32_t*)(fb_ptr + y * fb_info.pitch + x * 4);
    } else if (fb_info.bpp == 24) {
        uint8_t* p = fb_ptr + y * fb_info.pitch + x * 3;
        return 0xFF000000 | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
    }
    return 0;
}

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    /* Clip */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb_info.width) w = fb_info.width - x;
    if (y + h > (int)fb_info.height) h = fb_info.height - y;
    if (w <= 0 || h <= 0) return;

    if (fb_info.bpp == 32) {
        /* ⚡ BOLT Optimization: Build the first row segment once, then use memcpy
           to blast it to the remaining rows, drastically reducing inner loop overhead. */
        uint32_t* first_row = (uint32_t*)(fb_ptr + y * fb_info.pitch + x * 4);
        for (int j = 0; j < w; j++) {
            first_row[j] = color;
        }
        size_t row_bytes = w * 4;
        uint8_t* dest_row = (uint8_t*)first_row + fb_info.pitch;
        for (int i = 1; i < h; i++) {
            memcpy(dest_row, first_row, row_bytes);
            dest_row += fb_info.pitch;
        }
    } else if (fb_info.bpp == 24) {
        uint8_t b = color & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t r = (color >> 16) & 0xFF;
        /* ⚡ BOLT Optimization: Build the first row segment once, then use memcpy
           to blast it to the remaining rows, drastically reducing inner loop overhead. */
        uint8_t* first_row = fb_ptr + y * fb_info.pitch + x * 3;
        for (int j = 0; j < w; j++) {
            first_row[j * 3]     = b;
            first_row[j * 3 + 1] = g;
            first_row[j * 3 + 2] = r;
        }
        size_t row_bytes = w * 3;
        uint8_t* dest_row = first_row + fb_info.pitch;
        for (int i = 1; i < h; i++) {
            memcpy(dest_row, first_row, row_bytes);
            dest_row += fb_info.pitch;
        }
    }
}

/* Alpha-blended rectangle (ARGB) */
static void fill_rect_alpha(int x, int y, int w, int h, uint32_t color) {
    uint32_t a = (color >> 24) & 0xFF;
    if (a == 0xFF) { fill_rect(x, y, w, h, color); return; }
    if (a == 0) return;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb_info.width) w = fb_info.width - x;
    if (y + h > (int)fb_info.height) h = fb_info.height - y;
    if (w <= 0 || h <= 0) return;

    uint32_t inv_a = 255 - a;
    uint32_t color_rb = (color & 0xFF00FF) * a;
    uint32_t color_g = (color & 0x00FF00) * a;

    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            uint32_t dc = get_pixel(x + j, y + i);
            uint32_t rb = ((color_rb + (dc & 0xFF00FF) * inv_a) >> 8) & 0xFF00FF;
            uint32_t g  = ((color_g + (dc & 0x00FF00) * inv_a) >> 8) & 0x00FF00;
            put_pixel(x + j, y + i, 0xFF000000 | rb | g);
        }
    }
}

static void draw_hline(int x, int y, int w, uint32_t color) {
    fill_rect(x, y, w, 1, color);
}

static void draw_vline(int x, int y, int h, uint32_t color) {
    fill_rect(x, y, 1, h, color);
}

/* Outline rectangle */
static void stroke_rect(int x, int y, int w, int h, uint32_t color) {
    draw_hline(x, y, w, color);
    draw_hline(x, y + h - 1, w, color);
    draw_vline(x, y, h, color);
    draw_vline(x + w - 1, y, h, color);
}

/* Rounded rect (simple corner masking) */
static void fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    /* Body minus corners */
    fill_rect(x + r, y, w - 2 * r, h, color);         /* Center */
    fill_rect(x, y + r, r, h - 2 * r, color);         /* Left */
    fill_rect(x + w - r, y + r, r, h - 2 * r, color); /* Right */

    /* Simple round corners via filled circles (quarter arcs) */
    for (int cy = 0; cy < r; cy++) {
        for (int cx = 0; cx < r; cx++) {
            if (cx * cx + cy * cy <= r * r) {
                /* Top-left */
                put_pixel(x + r - 1 - cx, y + r - 1 - cy, color);
                /* Top-right */
                put_pixel(x + w - r + cx, y + r - 1 - cy, color);
                /* Bottom-left */
                put_pixel(x + r - 1 - cx, y + h - r + cy, color);
                /* Bottom-right */
                put_pixel(x + w - r + cx, y + h - r + cy, color);
            }
        }
    }
}

static void fill_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color) {
    uint32_t a = (color >> 24) & 0xFF;
    if (a == 0xFF) { fill_rounded_rect(x, y, w, h, r, color); return; }

    /* Body minus corners */
    fill_rect_alpha(x + r, y, w - 2 * r, h, color);
    fill_rect_alpha(x, y + r, r, h - 2 * r, color);
    fill_rect_alpha(x + w - r, y + r, r, h - 2 * r, color);

    /* Corner pixels with alpha */
    uint32_t inv_a = 255 - a;
    uint32_t color_rb = (color & 0xFF00FF) * a;
    uint32_t color_g = (color & 0x00FF00) * a;

    for (int cy = 0; cy < r; cy++) {
        for (int cx = 0; cx < r; cx++) {
            if (cx * cx + cy * cy <= r * r) {
                int pts[4][2] = {
                    { x + r - 1 - cx, y + r - 1 - cy },
                    { x + w - r + cx, y + r - 1 - cy },
                    { x + r - 1 - cx, y + h - r + cy },
                    { x + w - r + cx, y + h - r + cy },
                };
                for (int p = 0; p < 4; p++) {
                    int px = pts[p][0], py = pts[p][1];
                    if (px < 0 || px >= (int)fb_info.width || py < 0 || py >= (int)fb_info.height) continue;
                    uint32_t dc = get_pixel(px, py);
                    uint32_t rb = ((color_rb + (dc & 0xFF00FF) * inv_a) >> 8) & 0xFF00FF;
                    uint32_t g  = ((color_g + (dc & 0x00FF00) * inv_a) >> 8) & 0x00FF00;
                    put_pixel(px, py, 0xFF000000 | rb | g);
                }
            }
        }
    }
}

/* ========================================================================= */
/* Text Rendering (Bitmap 8x16)                                              */
/* ========================================================================= */

static void draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    int index = (unsigned char)c * 16;
    for (int row = 0; row < 16; row++) {
        uint8_t bits = font8x16[index + row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                put_pixel(x + col, y + row, fg);
            } else if (bg != 0x00000000) {
                put_pixel(x + col, y + row, bg);
            }
        }
    }
}

static void draw_text(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    while (*str) {
        if (*str == '\n') {
            y += 16;
            /* reset x? depends on context */
        } else {
            draw_char(x, y, *str, fg, bg);
            x += 8;
        }
        str++;
    }
}

static int text_width(const char* str) {
    int w = 0;
    while (*str) { w += 8; str++; }
    return w;
}

/* ========================================================================= */
/* Desktop Rendering                                                         */
/* ========================================================================= */

static uint32_t lerp_color(uint32_t c1, uint32_t c2, int t, int max) {
    if (max == 0) return c1;
    uint32_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    uint32_t r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    uint32_t r = r1 + (r2 - r1) * t / max;
    uint32_t g = g1 + (g2 - g1) * t / max;
    uint32_t b = b1 + (b2 - b1) * t / max;
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

static void draw_desktop_gradient(void) {
    int h = fb_info.height - TASKBAR_HEIGHT;
    int mid = h / 2;
    int bytes_pp = fb_info.bpp / 8;

    for (int y = 0; y < h; y++) {
        uint32_t color;
        if (y < mid) {
            color = lerp_color(GRAD_TOP & 0xFFFFFF, GRAD_MID & 0xFFFFFF, y, mid);
        } else {
            color = lerp_color(GRAD_MID & 0xFFFFFF, GRAD_BOTTOM & 0xFFFFFF, y - mid, h - mid);
        }

        uint8_t* row = fb_ptr + y * fb_info.pitch;
        uint8_t b = color & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t r = (color >> 16) & 0xFF;

        if (bytes_pp == 4) {
            uint32_t* row32 = (uint32_t*)row;
            for (int x = 0; x < (int)fb_info.width; x++) {
                row32[x] = color;
            }
        } else if (bytes_pp == 3) {
            for (int x = 0; x < (int)fb_info.width; x++) {
                row[x * 3]     = b;
                row[x * 3 + 1] = g;
                row[x * 3 + 2] = r;
            }
        }
    }

    /* Subtle grid pattern overlay (very faint dots) */
    for (int y = 0; y < h; y += 32) {
        for (int x = 0; x < (int)fb_info.width; x += 32) {
            put_pixel(x, y, 0xFF1A1E28);
        }
    }
}

/* ========================================================================= */
/* Taskbar                                                                   */
/* ========================================================================= */

static void draw_taskbar(void) {
    int ty = fb_info.height - TASKBAR_HEIGHT;

    /* Glassmorphism taskbar */
    fill_rect_alpha(0, ty, fb_info.width, TASKBAR_HEIGHT, COL_TASKBAR_BG);

    /* Top border line */
    draw_hline(0, ty, fb_info.width, COL_BORDER);

    /* Start button (Marea logo) */
    int btn_w = 40, btn_h = 32;
    int btn_x = 8, btn_y = ty + (TASKBAR_HEIGHT - btn_h) / 2;

    if (menu_open) {
        fill_rounded_rect(btn_x, btn_y, btn_w, btn_h, 6, COL_ACCENT_GLOW);
    } else {
        fill_rounded_rect(btn_x, btn_y, btn_w, btn_h, 6, COL_BG_TITLEBAR);
    }

    /* Draw "M" logo */
    draw_char(btn_x + (btn_w - 8) / 2, btn_y + (btn_h - 16) / 2, 'M', COL_ACCENT, 0);

    /* Window tray buttons */
    int tray_x = 60;
    for (int i = 0; i < window_count; i++) {
        if (!windows[i].visible) continue;

        int tw = text_width(windows[i].title) + 16;
        if (tw < 80) tw = 80;
        if (tw > 160) tw = 160;

        uint32_t tray_bg = (focused_window == i) ? COL_ACCENT_GLOW : COL_BG_TITLEBAR;
        fill_rounded_rect(tray_x, btn_y, tw, btn_h, 6, tray_bg);

        /* Title text */
        int tx = tray_x + 8;
        int max_chars = (tw - 16) / 8;
        char truncated[24];
        int tlen = strlen(windows[i].title);
        if (tlen > max_chars) tlen = max_chars;
        memcpy(truncated, windows[i].title, tlen);
        truncated[tlen] = '\0';
        draw_text(tx, btn_y + (btn_h - 16) / 2, truncated, COL_TEXT_PRIMARY, 0);

        /* Focused indicator dot */
        if (focused_window == i) {
            fill_rect(tray_x + tw / 2 - 2, ty + TASKBAR_HEIGHT - 4, 4, 2, COL_ACCENT);
        }

        tray_x += tw + 6;
    }

    /* Clock (right side) */
    /* We don't have clock_gettime in this demo, so show a static clock */
    const char* clock_str = "12:30";
    int clock_x = fb_info.width - text_width(clock_str) - 16;
    draw_text(clock_x, ty + (TASKBAR_HEIGHT - 16) / 2, clock_str, COL_TEXT_SECONDARY, 0);

    /* System tray indicators before clock */
    const char* tray_str = "etOS";
    int sys_x = clock_x - text_width(tray_str) - 20;
    draw_text(sys_x, ty + (TASKBAR_HEIGHT - 16) / 2, tray_str, COL_TEXT_SECONDARY, 0);
}

/* ========================================================================= */
/* App Menu                                                                  */
/* ========================================================================= */

static void draw_menu(void) {
    if (!menu_open) return;

    menu_x = 8;
    menu_y = fb_info.height - TASKBAR_HEIGHT - (MENU_ITEM_COUNT * MENU_ITEM_HEIGHT) - MENU_PADDING * 2 - 40;

    int total_h = MENU_ITEM_COUNT * MENU_ITEM_HEIGHT + MENU_PADDING * 2 + 40;

    /* Glass background */
    fill_rounded_rect_alpha(menu_x, menu_y, MENU_WIDTH, total_h, BORDER_RADIUS, COL_MENU_BG);
    stroke_rect(menu_x, menu_y, MENU_WIDTH, total_h, COL_BORDER);

    /* Header */
    draw_text(menu_x + MENU_PADDING + 4, menu_y + MENU_PADDING + 4, "Marea Shell", COL_ACCENT, 0);
    draw_text(menu_x + MENU_PADDING + 4, menu_y + MENU_PADDING + 22, "v1.0 - eterOS", COL_TEXT_SECONDARY, 0);

    /* Separator */
    int sep_y = menu_y + 40 + MENU_PADDING;
    draw_hline(menu_x + MENU_PADDING, sep_y, MENU_WIDTH - MENU_PADDING * 2, COL_BORDER);

    /* Items */
    int item_y = sep_y + 4;
    for (int i = 0; i < (int)MENU_ITEM_COUNT; i++) {
        if (menu_items[i].label[0] == '\0') {
            /* Separator */
            draw_hline(menu_x + MENU_PADDING, item_y + MENU_ITEM_HEIGHT / 2, MENU_WIDTH - MENU_PADDING * 2, COL_BORDER);
        } else {
            /* Check hover */
            int hovered = (mouse_x >= menu_x && mouse_x < menu_x + MENU_WIDTH &&
                          mouse_y >= item_y && mouse_y < item_y + MENU_ITEM_HEIGHT);

            if (hovered) {
                fill_rounded_rect(menu_x + 4, item_y + 2, MENU_WIDTH - 8, MENU_ITEM_HEIGHT - 4, 4, COL_MENU_HOVER);
            }

            /* Icon placeholder */
            draw_char(menu_x + MENU_PADDING + 8, item_y + (MENU_ITEM_HEIGHT - 16) / 2,
                     menu_items[i].icon[0], COL_ACCENT, 0);

            /* Label */
            draw_text(menu_x + MENU_PADDING + 28, item_y + (MENU_ITEM_HEIGHT - 16) / 2,
                     menu_items[i].label, hovered ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY, 0);
        }
        item_y += MENU_ITEM_HEIGHT;
    }
}

/* ========================================================================= */
/* Window Drawing                                                            */
/* ========================================================================= */

/* Check if click is on a window's close button */
static int hit_close_button(marea_window_t* win, int mx, int my) {
    int btn_cx = win->x + 14;
    int btn_cy = win->y + TITLEBAR_HEIGHT / 2;
    int dx = mx - btn_cx, dy = my - btn_cy;
    return (dx * dx + dy * dy <= 8 * 8);
}

static void draw_window_chrome(marea_window_t* win) {
    int x = win->x, y = win->y, w = win->w, h = win->h;
    int is_focused = win->focused;

    /* Window shadow (subtle) */
    fill_rect_alpha(x + 4, y + 4, w, h, 0x40000000);

    /* Window background */
    fill_rounded_rect(x, y, w, h, BORDER_RADIUS, COL_BG_SURFACE);

    /* Titlebar */
    fill_rounded_rect(x, y, w, TITLEBAR_HEIGHT, BORDER_RADIUS, COL_BG_TITLEBAR);
    /* Fix the bottom corners of titlebar (should be square) */
    fill_rect(x, y + TITLEBAR_HEIGHT - BORDER_RADIUS, w, BORDER_RADIUS, COL_BG_TITLEBAR);

    /* Titlebar separator */
    draw_hline(x, y + TITLEBAR_HEIGHT, w, COL_BORDER);

    /* Window border */
    uint32_t border_col = is_focused ? COL_ACCENT_GLOW : COL_BORDER;
    stroke_rect(x, y, w, h, border_col);

    /* Traffic light buttons (macOS-style) */
    int btn_r = 6;
    int btn_spacing = 20;
    int btn_base_x = x + 14;
    int btn_base_y = y + TITLEBAR_HEIGHT / 2;

    /* Check hover states */
    int hover_close = hit_close_button(win, mouse_x, mouse_y);
    int hover_min = (mouse_x >= btn_base_x + btn_spacing - btn_r && mouse_x <= btn_base_x + btn_spacing + btn_r &&
                     mouse_y >= btn_base_y - btn_r && mouse_y <= btn_base_y + btn_r);
    int hover_max = (mouse_x >= btn_base_x + btn_spacing * 2 - btn_r && mouse_x <= btn_base_x + btn_spacing * 2 + btn_r &&
                     mouse_y >= btn_base_y - btn_r && mouse_y <= btn_base_y + btn_r);

    /* Close (red) */
    for (int dy = -btn_r; dy <= btn_r; dy++) {
        for (int dx = -btn_r; dx <= btn_r; dx++) {
            if (dx*dx + dy*dy <= btn_r*btn_r) {
                put_pixel(btn_base_x + dx, btn_base_y + dy, COL_DANGER);
            }
        }
    }
    if (hover_close) {
        /* Draw 'x' inside close button */
        for (int i = -2; i <= 2; i++) {
            put_pixel(btn_base_x + i, btn_base_y + i, 0xFF4C0000);
            put_pixel(btn_base_x + i, btn_base_y - i, 0xFF4C0000);
        }
    }

    /* Minimize (yellow) */
    for (int dy = -btn_r; dy <= btn_r; dy++) {
        for (int dx = -btn_r; dx <= btn_r; dx++) {
            if (dx*dx + dy*dy <= btn_r*btn_r) {
                put_pixel(btn_base_x + btn_spacing + dx, btn_base_y + dy, COL_WARNING);
            }
        }
    }
    if (hover_min) {
        /* Draw '-' inside minimize button */
        for (int i = -2; i <= 2; i++) {
            put_pixel(btn_base_x + btn_spacing + i, btn_base_y, 0xFF4C4C00);
        }
    }

    /* Maximize (green) */
    for (int dy = -btn_r; dy <= btn_r; dy++) {
        for (int dx = -btn_r; dx <= btn_r; dx++) {
            if (dx*dx + dy*dy <= btn_r*btn_r) {
                put_pixel(btn_base_x + btn_spacing * 2 + dx, btn_base_y + dy, COL_SUCCESS);
            }
        }
    }
    if (hover_max) {
        /* Draw '+' inside maximize button */
        for (int i = -2; i <= 2; i++) {
            put_pixel(btn_base_x + btn_spacing * 2 + i, btn_base_y, 0xFF004C00);
            put_pixel(btn_base_x + btn_spacing * 2, btn_base_y + i, 0xFF004C00);
        }
    }

    /* Window title */
    int title_x = btn_base_x + btn_spacing * 3 + 8;
    uint32_t title_color = is_focused ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY;
    draw_text(title_x, y + (TITLEBAR_HEIGHT - 16) / 2, win->title, title_color, 0);
}

/* ========================================================================= */
/* Terminal in Window                                                        */
/* ========================================================================= */

#define TERM_MARGIN_L 8
#define TERM_MARGIN_T (TITLEBAR_HEIGHT + 4)

static void term_init_window(marea_window_t* win) {
    win->term_cx = 0;
    win->term_cy = 0;
    win->term_cols = (win->w - TERM_MARGIN_L * 2) / 8;
    win->term_rows = (win->h - TERM_MARGIN_T - 8) / 16;
    win->input_len = 0;
    memset(win->input_buf, 0, sizeof(win->input_buf));
}

static void term_clear(marea_window_t* win) {
    fill_rect(win->x + 1, win->y + TITLEBAR_HEIGHT + 1,
              win->w - 2, win->h - TITLEBAR_HEIGHT - 2, COL_TERM_BG);
    win->term_cx = 0;
    win->term_cy = 0;
}

static void term_scroll(marea_window_t* win) {
    int bx = win->x + TERM_MARGIN_L;
    int by = win->y + TERM_MARGIN_T;
    int bw = win->w - TERM_MARGIN_L * 2;
    int bh = win->h - TERM_MARGIN_T - 8;

    /* Move lines up by 16px */
    int bytes_per_pixel = fb_info.bpp / 8;
    int row_bytes = bw * bytes_per_pixel;

    for (int row = by; row < by + bh - 16; row++) {
        uint8_t* dst = fb_ptr + row * fb_info.pitch + bx * bytes_per_pixel;
        uint8_t* src = fb_ptr + (row + 16) * fb_info.pitch + bx * bytes_per_pixel;
        memmove(dst, src, row_bytes);
    }

    /* Clear bottom line */
    fill_rect(bx, by + bh - 16, bw, 16, COL_TERM_BG);
}

static void term_newline(marea_window_t* win) {
    win->term_cx = 0;
    win->term_cy++;
    if (win->term_cy >= win->term_rows) {
        win->term_cy = win->term_rows - 1;
        term_scroll(win);
    }
}

static void term_putchar(marea_window_t* win, char c, uint32_t fg) {
    if (c == '\n') {
        term_newline(win);
    } else if (c == '\b' || c == 0x08 || c == 0x7F) {
        if (win->term_cx > 0) {
            win->term_cx--;
            draw_char(win->x + TERM_MARGIN_L + win->term_cx * 8,
                     win->y + TERM_MARGIN_T + win->term_cy * 16,
                     ' ', fg, COL_TERM_BG);
        }
    } else if (c >= 32 && c <= 126) {
        if (win->term_cx >= win->term_cols) {
            term_newline(win);
        }
        draw_char(win->x + TERM_MARGIN_L + win->term_cx * 8,
                 win->y + TERM_MARGIN_T + win->term_cy * 16,
                 c, fg, COL_TERM_BG);
        win->term_cx++;
    }
}

static void term_print(marea_window_t* win, const char* str, uint32_t fg) {
    while (*str) {
        term_putchar(win, *str, fg);
        str++;
    }
}

static void term_draw_prompt(marea_window_t* win) {
    term_print(win, "user@eteros", COL_TERM_PROMPT_USER);
    term_print(win, " ~ $ ", COL_TERM_PROMPT_DIR);
}

static void term_execute(marea_window_t* win) {
    win->input_buf[win->input_len] = '\0';

    if (win->input_len > 0) {
        char cmd[256];
        int i = 0;
        while (win->input_buf[i] && win->input_buf[i] != ' ' && i < 255) {
            cmd[i] = win->input_buf[i];
            i++;
        }
        cmd[i] = '\0';

        if (strcmp(cmd, "help") == 0) {
            term_print(win, "\n", COL_TERM_FG);
            term_print(win, "Marea Shell Terminal\n", COL_ACCENT);
            term_print(win, "Comandos: help, clear, echo, uname,\n", COL_TERM_FG);
            term_print(win, "  ls, cat, run, exit\n", COL_TERM_FG);
        } else if (strcmp(cmd, "clear") == 0) {
            term_clear(win);
            win->input_len = 0;
            term_draw_prompt(win);
            return;
        } else if (strcmp(cmd, "uname") == 0) {
            term_print(win, "\n", COL_TERM_FG);
            term_print(win, "eterOS Marea Shell v1.0\n", COL_ACCENT);
            term_print(win, "Compositor: Wayland-like (SHM)\n", COL_TERM_FG);
            term_print(win, "Arch: x86_64 Long Mode\n", COL_TERM_FG);
        } else if (strcmp(cmd, "echo") == 0) {
            term_print(win, "\n", COL_TERM_FG);
            if (win->input_buf[i] == ' ') i++;
            term_print(win, win->input_buf + i, COL_TERM_FG);
            term_print(win, "\n", COL_TERM_FG);
        } else if (strcmp(cmd, "run") == 0) {
            if (win->input_buf[i] == ' ') i++;
            char* arg = win->input_buf + i;
            if (strlen(arg) > 0) {
                term_print(win, "\nEjecutando ", COL_TERM_FG);
                term_print(win, arg, COL_WARNING);
                term_print(win, "...\n", COL_TERM_FG);

                int child = fork();
                if (child == 0) {
                    char path[256] = "/";
                    strlcat(path, arg, sizeof(path));
                    char *argv[] = { path, NULL };
                    char *envp[] = { NULL };
                    execve(path, argv, envp);
                    exit(1);
                }
            } else {
                term_print(win, "\nUso: run <archivo.elf>\n", COL_WARNING);
            }
        } else if (strcmp(cmd, "exit") == 0) {
            term_print(win, "\nSaliendo...\n", COL_DANGER);
            exit(0);
        } else {
            term_print(win, "\nComando no encontrado: ", COL_DANGER);
            term_print(win, cmd, COL_DANGER);
            term_print(win, "\n", COL_TERM_FG);
        }
    } else {
        term_print(win, "\n", COL_TERM_FG);
    }

    win->input_len = 0;
    term_draw_prompt(win);
}

/* ========================================================================= */
/* Window Management                                                         */
/* ========================================================================= */

static int create_terminal_window(void) {
    if (window_count >= MAX_WINDOWS) return -1;

    marea_window_t* win = &windows[window_count];
    win->id = window_count;
    win->w = (int)fb_info.width > 700 ? 640 : (int)fb_info.width - 60;
    win->h = (int)fb_info.height > 540 ? 420 : (int)fb_info.height - 120;
    win->x = ((int)fb_info.width - win->w) / 2 + window_count * 20;
    win->y = ((int)fb_info.height - TASKBAR_HEIGHT - win->h) / 2 + window_count * 20;
    win->visible = 1;
    win->focused = 1;
    win->minimized = 0;
    strlcpy(win->title, "Terminal", sizeof(win->title));

    /* Unfocus previous */
    if (focused_window >= 0) {
        windows[focused_window].focused = 0;
    }
    focused_window = window_count;

    term_init_window(win);
    window_count++;

    return win->id;
}

/* ========================================================================= */
/* Cursor Rendering                                                          */
/* ========================================================================= */

static void cursor_save_bg(int cx, int cy) {
    for (int i = 0; i < CURSOR_H; i++) {
        for (int j = 0; j < CURSOR_W; j++) {
            cursor_save[i * CURSOR_W + j] = get_pixel(cx + j, cy + i);
        }
    }
}

static void cursor_restore_bg(int cx, int cy) {
    for (int i = 0; i < CURSOR_H; i++) {
        for (int j = 0; j < CURSOR_W; j++) {
            put_pixel(cx + j, cy + i, cursor_save[i * CURSOR_W + j]);
        }
    }
}

static void cursor_draw(int cx, int cy) {
    /* Modern arrow cursor with anti-aliased look */
    /* Arrow shape: 12x18 */
    static const char* cursor_shape[18] = {
        "X...........",
        "XX..........",
        "XOX.........",
        "XOOX........",
        "XOOOX.......",
        "XOOOOX......",
        "XOOOOOX.....",
        "XOOOOOOX....",
        "XOOOOOOOX...",
        "XOOOOOOOOX..",
        "XOOOOOOOOOX.",
        "XOOOOOXXXXXXX",
        "XOOXOOX.....",
        "XOXXOOX.....",
        "XX..XOOX....",
        "X...XOOX....",
        ".....XOOX...",
        "......XX....",
    };

    for (int row = 0; row < 18 && row < CURSOR_H; row++) {
        const char* line = cursor_shape[row];
        for (int col = 0; col < 12 && col < CURSOR_W && line[col]; col++) {
            if (line[col] == 'O') {
                put_pixel(cx + col, cy + row, COL_CURSOR_FILL);
            } else if (line[col] == 'X') {
                put_pixel(cx + col, cy + row, COL_CURSOR_BORDER);
            }
        }
    }
}

/* ========================================================================= */
/* Hit Testing                                                               */
/* ========================================================================= */

/* Check if click is on start button */
static int hit_start_button(int mx, int my) {
    int ty = fb_info.height - TASKBAR_HEIGHT;
    int btn_x = 8, btn_y = ty + (TASKBAR_HEIGHT - 32) / 2;
    return (mx >= btn_x && mx < btn_x + 40 && my >= btn_y && my < btn_y + 32);
}

/* Check if click is on titlebar (for drag) */
static int hit_titlebar(marea_window_t* win, int mx, int my) {
    return (mx >= win->x && mx < win->x + win->w &&
            my >= win->y && my < win->y + TITLEBAR_HEIGHT);
}

/* Find topmost window under point */
static int find_window_at(int mx, int my) {
    /* Search from top (highest index = last focused) */
    for (int i = window_count - 1; i >= 0; i--) {
        if (!windows[i].visible || windows[i].minimized) continue;
        if (mx >= windows[i].x && mx < windows[i].x + windows[i].w &&
            my >= windows[i].y && my < windows[i].y + windows[i].h) {
            return i;
        }
    }
    return -1;
}

/* Check if click is in the menu */
static int hit_menu_item(int mx, int my) {
    if (!menu_open) return -1;

    int sep_y = menu_y + 40 + MENU_PADDING + 4;
    for (int i = 0; i < (int)MENU_ITEM_COUNT; i++) {
        int item_y = sep_y + i * MENU_ITEM_HEIGHT;
        if (menu_items[i].label[0] == '\0') continue;
        if (mx >= menu_x && mx < menu_x + MENU_WIDTH &&
            my >= item_y && my < item_y + MENU_ITEM_HEIGHT) {
            return i;
        }
    }
    return -1;
}

/* ========================================================================= */
/* Full Screen Redraw                                                        */
/* ========================================================================= */

static void redraw_all(void) {
    draw_desktop_gradient();

    /* Draw all windows */
    for (int i = 0; i < window_count; i++) {
        if (!windows[i].visible || windows[i].minimized) continue;
        draw_window_chrome(&windows[i]);
        /* Redraw terminal content area */
        fill_rect(windows[i].x + 1, windows[i].y + TITLEBAR_HEIGHT + 1,
                  windows[i].w - 2, windows[i].h - TITLEBAR_HEIGHT - 2, COL_TERM_BG);

        /* Re-render terminal from scratch (simplified: just prompt) */
        windows[i].term_cx = 0;
        windows[i].term_cy = 0;
        term_print(&windows[i], "eterOS Marea Shell\n", COL_ACCENT);
        term_print(&windows[i], "Escribe 'help' para comandos.\n\n", COL_TEXT_SECONDARY);
        term_draw_prompt(&windows[i]);
    }

    draw_taskbar();
    draw_menu();
}

/* ========================================================================= */
/* Drag State                                                                */
/* ========================================================================= */

static int dragging = 0;
static int drag_win = -1;
static int drag_offset_x = 0, drag_offset_y = 0;

/* ========================================================================= */
/* Main Entry                                                                */
/* ========================================================================= */

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    printf("[Marea] Starting Marea Shell Desktop Environment...\n");

    /* 1. Open Framebuffer */
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        printf("[Marea] Error: Cannot open /dev/fb0\n");
        return 1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_info) != 0) {
        printf("[Marea] Error: ioctl FBIOGET_VSCREENINFO failed\n");
        return 1;
    }

    printf("[Marea] Display: %dx%d @ %dbpp (pitch=%d)\n",
           fb_info.width, fb_info.height, fb_info.bpp, fb_info.pitch);

    size_t fb_size = fb_info.height * fb_info.pitch;
    fb_ptr = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_ptr == MAP_FAILED) {
        printf("[Marea] Error: mmap failed\n");
        return 1;
    }

    /* 2. Initialize mouse position */
    mouse_x = fb_info.width / 2;
    mouse_y = fb_info.height / 2;

    /* 3. Draw initial desktop */
    redraw_all();

    /* Create initial terminal window */
    int term_id = create_terminal_window();
    if (term_id >= 0) {
        marea_window_t* win = &windows[term_id];
        draw_window_chrome(win);
        fill_rect(win->x + 1, win->y + TITLEBAR_HEIGHT + 1,
                  win->w - 2, win->h - TITLEBAR_HEIGHT - 2, COL_TERM_BG);
        term_print(win, "eterOS Marea Shell v1.0\n", COL_ACCENT);
        term_print(win, "Compositor: Wayland-like (SHM zero-copy)\n", COL_TEXT_SECONDARY);
        term_print(win, "Escribe 'help' para lista de comandos.\n\n", COL_TEXT_SECONDARY);
        term_draw_prompt(win);
    }

    draw_taskbar();

    /* Initial cursor */
    cursor_save_bg(mouse_x, mouse_y);
    cursor_draw(mouse_x, mouse_y);

    /* 4. Fork mouse reader */
    int mouse_pid = fork();
    if (mouse_pid == 0) {
        /* Child: Mouse input loop */
        sleep(1); /* Let parent render first */

        int mfd = open("/dev/input/mouse0", O_RDONLY);
        if (mfd < 0) {
            printf("[Marea] Critical Error: Cannot open /dev/input/mouse0\n");
            exit(1);
        }

        input_event_t ev;
        while (read(mfd, &ev, sizeof(ev)) > 0) {
            /* Visual debug: flick red square in top left when mouse events arrive */
            fill_rect(0, 0, 8, 8, 0xFF0000); 

            if (ev.type == EV_REL) {
                /* Erase old cursor */
                cursor_restore_bg(mouse_x, mouse_y);

                if (ev.code == REL_X) mouse_x += ev.value;
                if (ev.code == REL_Y) mouse_y += ev.value;

                /* Clamp */
                if (mouse_x < 0) mouse_x = 0;
                if (mouse_y < 0) mouse_y = 0;
                if (mouse_x >= (int)fb_info.width - CURSOR_W) mouse_x = fb_info.width - CURSOR_W;
                if (mouse_y >= (int)fb_info.height - CURSOR_H) mouse_y = fb_info.height - CURSOR_H;

                /* Dragging? */
                if (dragging && drag_win >= 0) {
                    windows[drag_win].x = mouse_x - drag_offset_x;
                    windows[drag_win].y = mouse_y - drag_offset_y;

                    /* Clamp window position */
                    if (windows[drag_win].y < 0) windows[drag_win].y = 0;
                    if (windows[drag_win].y + windows[drag_win].h > (int)fb_info.height - TASKBAR_HEIGHT)
                        windows[drag_win].y = fb_info.height - TASKBAR_HEIGHT - windows[drag_win].h;

                    /* Redraw everything (simple approach for now) */
                    redraw_all();
                } else {
                    /* Not dragging, check if we need to redraw window chrome for hover states */
                    static int last_hovered_win = -1;
                    int hovered_win = find_window_at(mouse_x, mouse_y);
                    if (hovered_win >= 0) {
                        marea_window_t* win = &windows[hovered_win];
                        if (mouse_y >= win->y && mouse_y <= win->y + TITLEBAR_HEIGHT) {
                            draw_window_chrome(win);
                        }
                    }
                    if (last_hovered_win >= 0 && last_hovered_win != hovered_win && windows[last_hovered_win].visible) {
                        draw_window_chrome(&windows[last_hovered_win]);
                    }
                    last_hovered_win = hovered_win;
                }

                /* Draw new cursor */
                cursor_save_bg(mouse_x, mouse_y);
                cursor_draw(mouse_x, mouse_y);

            } else if (ev.type == EV_KEY && ev.code == BTN_LEFT) {
                if (ev.value == 1) {
                    /* Mouse press */
                    mouse_btn = 1;

                    /* Check menu click first */
                    if (menu_open) {
                        int item = hit_menu_item(mouse_x, mouse_y);
                        if (item >= 0) {
                            /* Handle menu action */
                            if (strcmp(menu_items[item].label, "Terminal") == 0) {
                                create_terminal_window();
                                redraw_all();
                                cursor_save_bg(mouse_x, mouse_y);
                                cursor_draw(mouse_x, mouse_y);
                            }
                            /* Close menu after action */
                            menu_open = 0;
                            redraw_all();
                            cursor_save_bg(mouse_x, mouse_y);
                            cursor_draw(mouse_x, mouse_y);
                            continue;
                        }
                    }

                    /* Check start button */
                    if (hit_start_button(mouse_x, mouse_y)) {
                        menu_open = !menu_open;
                        redraw_all();
                        cursor_save_bg(mouse_x, mouse_y);
                        cursor_draw(mouse_x, mouse_y);
                        continue;
                    }

                    /* Close menu if clicking elsewhere */
                    if (menu_open) {
                        menu_open = 0;
                        redraw_all();
                        cursor_save_bg(mouse_x, mouse_y);
                        cursor_draw(mouse_x, mouse_y);
                    }

                    /* Check window interaction */
                    int win_idx = find_window_at(mouse_x, mouse_y);
                    if (win_idx >= 0) {
                        marea_window_t* win = &windows[win_idx];

                        /* Focus */
                        if (focused_window != win_idx) {
                            if (focused_window >= 0) windows[focused_window].focused = 0;
                            win->focused = 1;
                            focused_window = win_idx;
                            redraw_all();
                            cursor_save_bg(mouse_x, mouse_y);
                            cursor_draw(mouse_x, mouse_y);
                        }

                        /* Close button */
                        if (hit_close_button(win, mouse_x, mouse_y)) {
                            win->visible = 0;
                            win->focused = 0;
                            if (focused_window == win_idx) focused_window = -1;
                            redraw_all();
                            cursor_save_bg(mouse_x, mouse_y);
                            cursor_draw(mouse_x, mouse_y);
                            continue;
                        }

                        /* Titlebar drag */
                        if (hit_titlebar(win, mouse_x, mouse_y)) {
                            dragging = 1;
                            drag_win = win_idx;
                            drag_offset_x = mouse_x - win->x;
                            drag_offset_y = mouse_y - win->y;
                        }
                    }

                } else {
                    /* Mouse release */
                    mouse_btn = 0;
                    dragging = 0;
                    drag_win = -1;
                }
            }
        }
        exit(0);
    }

    /* 5. Parent: Keyboard input loop */
    int tfd = open("/dev/tty", O_RDWR);
    if (tfd < 0) {
        printf("[Marea] Warning: Cannot open /dev/tty\n");
    }

    while (1) {
        char c;
        if (tfd >= 0 && read(tfd, &c, 1) > 0) {
            /* Route to focused terminal window */
            if (focused_window >= 0 && windows[focused_window].visible) {
                marea_window_t* win = &windows[focused_window];

                /* Erase cursor briefly for redraw */
                cursor_restore_bg(mouse_x, mouse_y);

                if (c == '\r' || c == '\n') {
                    term_execute(win);
                } else if (c == '\b' || c == 0x08 || c == 0x7F) {
                    if (win->input_len > 0) {
                        win->input_len--;
                        term_putchar(win, '\b', COL_TERM_FG);
                    }
                } else if (c >= 32 && c <= 126) {
                    if (win->input_len < (int)sizeof(win->input_buf) - 1) {
                        win->input_buf[win->input_len++] = c;
                        term_putchar(win, c, COL_TERM_FG);
                    }
                }

                /* Restore cursor */
                cursor_save_bg(mouse_x, mouse_y);
                cursor_draw(mouse_x, mouse_y);
            }
        }
    }

    return 0;
}
