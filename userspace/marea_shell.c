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
#include <poll.h>
#include <termios.h>
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
static uint8_t* backbuffer_ptr = NULL;
static fb_info_t fb_info;
static int dirty_valid = 0;
static int dirty_x1 = 0;
static int dirty_y1 = 0;
static int dirty_x2 = 0;
static int dirty_y2 = 0;

static inline uint8_t* active_surface(void) {
    return backbuffer_ptr ? backbuffer_ptr : fb_ptr;
}

static void mark_dirty_rect(int x, int y, int w, int h) {
    int x2;
    int y2;

    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= (int)fb_info.width || y >= (int)fb_info.height) return;
    if (x + w > (int)fb_info.width) w = (int)fb_info.width - x;
    if (y + h > (int)fb_info.height) h = (int)fb_info.height - y;
    if (w <= 0 || h <= 0) return;

    x2 = x + w;
    y2 = y + h;

    if (!dirty_valid) {
        dirty_x1 = x;
        dirty_y1 = y;
        dirty_x2 = x2;
        dirty_y2 = y2;
        dirty_valid = 1;
        return;
    }

    if (x < dirty_x1) dirty_x1 = x;
    if (y < dirty_y1) dirty_y1 = y;
    if (x2 > dirty_x2) dirty_x2 = x2;
    if (y2 > dirty_y2) dirty_y2 = y2;
}

static void present(void) {
    int x;
    int y;
    int w;
    int h;
    int bytes_per_pixel;
    size_t row_bytes;

    if (!fb_ptr || !backbuffer_ptr) {
        return;
    }

    if (!dirty_valid) {
        return;
    }

    x = dirty_x1;
    y = dirty_y1;
    w = dirty_x2 - dirty_x1;
    h = dirty_y2 - dirty_y1;
    bytes_per_pixel = (int)(fb_info.bpp / 8);
    row_bytes = (size_t)w * bytes_per_pixel;

    for (int row = 0; row < h; ++row) {
        uint8_t* src = backbuffer_ptr + (y + row) * fb_info.pitch + x * bytes_per_pixel;
        uint8_t* dst = fb_ptr + (y + row) * fb_info.pitch + x * bytes_per_pixel;
        memcpy(dst, src, row_bytes);
    }

    dirty_valid = 0;
}

#define TASKBAR_HEIGHT 44
#define TITLEBAR_HEIGHT 32
#define BORDER_RADIUS 8
#define MENU_WIDTH 260
#define MENU_ITEM_HEIGHT 36
#define MENU_PADDING 8
#define EDITOR_BUFFER_SIZE 4096

typedef enum {
    WINDOW_KIND_TERMINAL = 0,
    WINDOW_KIND_EDITOR = 1,
} window_kind_t;

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
    int kind;
    char title[32];

    /* Terminal state (inline for demo) */
    int term_cx, term_cy;
    int term_cols, term_rows;
    char input_buf[256];
    int input_len;
    char cwd[128];

    /* Simple text editor state */
    char editor_path[128];
    char editor_status[96];
    char editor_buf[EDITOR_BUFFER_SIZE];
    int editor_len;
    int editor_dirty;
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
#define MOUSE_SENSITIVITY 3

static int mouse_x, mouse_y;
static int mouse_btn = 0;

/* Cursor save/restore */
#define CURSOR_W 12
#define CURSOR_H 18
static uint32_t cursor_save[CURSOR_W * CURSOR_H];
static int dragging = 0;
static int drag_win = -1;
static int drag_offset_x = 0, drag_offset_y = 0;

static void handle_mouse_event(const input_event_t* ev);
static void handle_keyboard_char(char c);
static void drain_mouse_events(int fd);
static void draw_window_chrome(marea_window_t* win);
static int hit_start_button(int mx, int my);
static int hit_titlebar(marea_window_t* win, int mx, int my);
static int hit_editor_save_button(marea_window_t* win, int mx, int my);
static int find_window_at(int mx, int my);
static int hit_menu_item(int mx, int my);
static void redraw_all(void);

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
    uint8_t* surface = active_surface();

    if (x < 0 || x >= (int)fb_info.width || y < 0 || y >= (int)fb_info.height) return;
    if (fb_info.bpp == 32) {
        uint32_t* p = (uint32_t*)(surface + y * fb_info.pitch + x * 4);
        *p = color;
    } else if (fb_info.bpp == 24) {
        uint8_t* p = surface + y * fb_info.pitch + x * 3;
        p[0] = color & 0xFF;
        p[1] = (color >> 8) & 0xFF;
        p[2] = (color >> 16) & 0xFF;
    }
}

static inline uint32_t get_pixel(int x, int y) {
    uint8_t* surface = active_surface();

    if (x < 0 || x >= (int)fb_info.width || y < 0 || y >= (int)fb_info.height) return 0;
    if (fb_info.bpp == 32) {
        return *(uint32_t*)(surface + y * fb_info.pitch + x * 4);
    } else if (fb_info.bpp == 24) {
        uint8_t* p = surface + y * fb_info.pitch + x * 3;
        return 0xFF000000 | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
    }
    return 0;
}

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    uint8_t* surface = active_surface();

    /* Clip */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb_info.width) w = fb_info.width - x;
    if (y + h > (int)fb_info.height) h = fb_info.height - y;
    if (w <= 0 || h <= 0) return;

    if (fb_info.bpp == 32) {
        /* ⚡ BOLT Optimization: Build the first row once, then copy it with memcpy
           to subsequent rows instead of per-pixel assignments. */
        uint32_t* first_row = (uint32_t*)(surface + y * fb_info.pitch + x * 4);
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

        /* ⚡ BOLT Optimization: Build the first row segment once, then use memcpy */
        uint8_t* first_row = surface + y * fb_info.pitch + x * 3;
        uint8_t* p = first_row;
        for (int j = 0; j < w; j++) {
            *p++ = b;
            *p++ = g;
            *p++ = r;
        }

        size_t row_bytes = w * 3;
        uint8_t* dest_row = first_row + fb_info.pitch;
        for (int i = 1; i < h; i++) {
            memcpy(dest_row, first_row, row_bytes);
            dest_row += fb_info.pitch;
        }
    }
}

static void fill_rect_alpha(int x, int y, int w, int h, uint32_t color) {
    uint8_t* surface = active_surface();

    uint32_t a = (color >> 24) & 0xFF;
    if (a == 0xFF) { fill_rect(x, y, w, h, color); return; }
    if (a == 0) return;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb_info.width) w = fb_info.width - x;
    if (y + h > (int)fb_info.height) h = fb_info.height - y;
    if (w <= 0 || h <= 0) return;

    uint32_t inv_a = 255 - a;
    if (fb_info.bpp == 32) {
        for (int i = 0; i < h; i++) {
            uint32_t* row = (uint32_t*)(surface + (y + i) * fb_info.pitch + x * 4);
            for (int j = 0; j < w; j++) {
                uint32_t dc = row[j];
                uint32_t rb = (((color & 0xFF00FF) * a + (dc & 0xFF00FF) * inv_a) >> 8) & 0xFF00FF;
                uint32_t g  = (((color & 0x00FF00) * a + (dc & 0x00FF00) * inv_a) >> 8) & 0x00FF00;
                row[j] = 0xFF000000 | rb | g;
            }
        }
    } else {
        /* Fallback for other depths using get_pixel/put_pixel */
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                uint32_t dc = get_pixel(x + j, y + i);
                uint32_t rb = (((color & 0xFF00FF) * a + (dc & 0xFF00FF) * inv_a) >> 8) & 0xFF00FF;
                uint32_t g  = (((color & 0x00FF00) * a + (dc & 0x00FF00) * inv_a) >> 8) & 0x00FF00;
                put_pixel(x + j, y + i, 0xFF000000 | rb | g);
            }
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
    uint8_t* surface = active_surface();

    for (int y = 0; y < h; y++) {
        uint32_t color;
        if (y < mid) {
            color = lerp_color(GRAD_TOP & 0xFFFFFF, GRAD_MID & 0xFFFFFF, y, mid);
        } else {
            color = lerp_color(GRAD_MID & 0xFFFFFF, GRAD_BOTTOM & 0xFFFFFF, y - mid, h - mid);
        }

        uint8_t* row = surface + y * fb_info.pitch;
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
    if (!getcwd(win->cwd, sizeof(win->cwd))) {
        strlcpy(win->cwd, "/", sizeof(win->cwd));
    }
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
    uint8_t* surface = active_surface();

    /* Move lines up by 16px */
    int bytes_per_pixel = fb_info.bpp / 8;
    int row_bytes = bw * bytes_per_pixel;

    for (int row = by; row < by + bh - 16; row++) {
        uint8_t* dst = surface + row * fb_info.pitch + bx * bytes_per_pixel;
        uint8_t* src = surface + (row + 16) * fb_info.pitch + bx * bytes_per_pixel;
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
    term_print(win, " ", COL_TERM_FG);
    term_print(win, win->cwd, COL_TERM_PROMPT_DIR);
    term_print(win, " $ ", COL_TERM_PROMPT_DIR);
}

static int split_args(char *line, char *argv[], int max_args) {
    int argc = 0;
    char *token = strtok(line, " ");

    while (token && argc < max_args - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    argv[argc] = NULL;
    return argc;
}

static int resolve_command_path(const char *cmd, char *out, size_t out_size) {
    static const char *search_dirs[] = { "", "/", "/gnu/bin/", "/bin/" };
    static const char *suffixes[] = { "", ".elf" };

    if (!cmd || !cmd[0]) return -1;

    if (strchr(cmd, '/')) {
        strlcpy(out, cmd, out_size);
        if (access(out, F_OK) == 0) return 0;
        return -1;
    }

    for (size_t i = 0; i < sizeof(search_dirs) / sizeof(search_dirs[0]); ++i) {
        for (size_t j = 0; j < sizeof(suffixes) / sizeof(suffixes[0]); ++j) {
            out[0] = '\0';
            strlcat(out, search_dirs[i], out_size);
            strlcat(out, cmd, out_size);
            strlcat(out, suffixes[j], out_size);
            if (access(out, F_OK) == 0) return 0;
        }
    }

    return -1;
}

static void term_run_external(marea_window_t* win, char *argv[], int argc) {
    char path[256];
    char busybox_path[256];
    int pipefd[2];
    int status = 0;

    if (argc <= 0 || !argv[0]) {
        return;
    }

    if (resolve_command_path(argv[0], path, sizeof(path)) != 0) {
        if (resolve_command_path("busybox", busybox_path, sizeof(busybox_path)) == 0) {
            char *bb_argv[34];
            int bb_argc = 0;

            bb_argv[bb_argc++] = busybox_path;
            bb_argv[bb_argc++] = argv[0];
            for (int i = 1; i < argc && bb_argc < 33; ++i) {
                bb_argv[bb_argc++] = argv[i];
            }
            bb_argv[bb_argc] = NULL;
            argv = bb_argv;
            argc = bb_argc;
            strlcpy(path, busybox_path, sizeof(path));
        } else {
            term_print(win, "\nComando no encontrado: ", COL_DANGER);
            term_print(win, argv[0], COL_DANGER);
            term_print(win, "\n", COL_TERM_FG);
            return;
        }
    }

    if (pipe(pipefd) < 0) {
        term_print(win, "\nError: pipe failed\n", COL_DANGER);
        return;
    }

    {
        int pid = fork();
        if (pid == 0) {
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[0]);
            close(pipefd[1]);
            execve(path, argv, NULL);
            write(STDERR_FILENO, "exec failed\n", 12);
            exit(1);
        } else if (pid > 0) {
            char buf[129];
            ssize_t nread;

            close(pipefd[1]);
            while ((nread = read(pipefd[0], buf, sizeof(buf) - 1)) > 0) {
                buf[nread] = '\0';
                term_print(win, buf, COL_TERM_FG);
            }
            close(pipefd[0]);
            waitpid(pid, &status, 0);
        } else {
            close(pipefd[0]);
            close(pipefd[1]);
            term_print(win, "\nError: fork failed\n", COL_DANGER);
        }
    }
}

static void term_execute(marea_window_t* win) {
    win->input_buf[win->input_len] = '\0';

    if (win->input_len > 0) {
        char line[256];
        char *argv[32];
        int argc;

        strlcpy(line, win->input_buf, sizeof(line));
        argc = split_args(line, argv, 32);
        if (argc == 0) {
            term_print(win, "\n", COL_TERM_FG);
            win->input_len = 0;
            term_draw_prompt(win);
            return;
        }

        if (strcmp(argv[0], "help") == 0) {
            term_print(win, "\n", COL_TERM_FG);
            term_print(win, "Marea Shell Terminal\n", COL_ACCENT);
            term_print(win, "Builtins: help, clear, echo, uname, cd, pwd, exit\n", COL_TERM_FG);
            term_print(win, "Externos: se resuelven en /gnu/bin, /bin y /\n", COL_TERM_FG);
        } else if (strcmp(argv[0], "clear") == 0) {
            term_clear(win);
            win->input_len = 0;
            term_draw_prompt(win);
            return;
        } else if (strcmp(argv[0], "uname") == 0) {
            term_print(win, "\n", COL_TERM_FG);
            term_print(win, "eterOS Marea Shell v1.0\n", COL_ACCENT);
            term_print(win, "Compositor: Wayland-like (SHM)\n", COL_TERM_FG);
            term_print(win, "Arch: x86_64 Long Mode\n", COL_TERM_FG);
        } else if (strcmp(argv[0], "echo") == 0) {
            term_print(win, "\n", COL_TERM_FG);
            for (int i = 1; i < argc; ++i) {
                if (i > 1) term_print(win, " ", COL_TERM_FG);
                term_print(win, argv[i], COL_TERM_FG);
            }
            term_print(win, "\n", COL_TERM_FG);
        } else if (strcmp(argv[0], "cd") == 0) {
            const char *target = (argc > 1) ? argv[1] : "/";
            if (chdir(target) == 0) {
                if (!getcwd(win->cwd, sizeof(win->cwd))) {
                    strlcpy(win->cwd, target, sizeof(win->cwd));
                }
            } else {
                term_print(win, "\ncd: no se pudo cambiar al directorio\n", COL_DANGER);
            }
        } else if (strcmp(argv[0], "pwd") == 0) {
            if (!getcwd(win->cwd, sizeof(win->cwd))) {
                strlcpy(win->cwd, "/", sizeof(win->cwd));
            }
            term_print(win, "\n", COL_TERM_FG);
            term_print(win, win->cwd, COL_TERM_FG);
            term_print(win, "\n", COL_TERM_FG);
        } else if (strcmp(argv[0], "run") == 0) {
            if (argc > 1) {
                term_print(win, "\n", COL_TERM_FG);
                term_run_external(win, &argv[1], argc - 1);
            } else {
                term_print(win, "\nUso: run <archivo>\n", COL_WARNING);
            }
        } else if (strcmp(argv[0], "exit") == 0) {
            term_print(win, "\nSaliendo...\n", COL_DANGER);
            exit(0);
        } else {
            term_print(win, "\n", COL_TERM_FG);
            term_run_external(win, argv, argc);
        }
    } else {
        term_print(win, "\n", COL_TERM_FG);
    }

    win->input_len = 0;
    term_draw_prompt(win);
}

/* ========================================================================= */
/* Editor Window                                                             */
/* ========================================================================= */

#define EDITOR_MARGIN_L 10
#define EDITOR_MARGIN_T (TITLEBAR_HEIGHT + 34)
#define EDITOR_LINE_H 16

static void editor_set_status(marea_window_t* win, const char* message) {
    if (!win) return;
    strlcpy(win->editor_status, message ? message : "", sizeof(win->editor_status));
}

static void editor_load_file(marea_window_t* win, const char* path) {
    FILE* fp;
    size_t nread;

    win->editor_len = 0;
    win->editor_buf[0] = '\0';

    fp = fopen(path, "rb");
    if (!fp) {
        editor_set_status(win, "Archivo nuevo. Usa Save para guardarlo.");
        return;
    }

    nread = fread(win->editor_buf, 1, sizeof(win->editor_buf) - 1, fp);
    fclose(fp);

    win->editor_len = (int)nread;
    win->editor_buf[win->editor_len] = '\0';
    editor_set_status(win, "Archivo cargado desde /data.");
}

static int editor_save_file(marea_window_t* win) {
    FILE* fp;

    fp = fopen(win->editor_path, "wb");
    if (!fp) {
        editor_set_status(win, "No se pudo guardar el archivo.");
        return -1;
    }

    if (win->editor_len > 0 && fwrite(win->editor_buf, 1, (size_t)win->editor_len, fp) != (size_t)win->editor_len) {
        fclose(fp);
        editor_set_status(win, "Error escribiendo en disco.");
        return -1;
    }

    fclose(fp);
    win->editor_dirty = 0;
    editor_set_status(win, "Guardado en /data/notas.txt");
    return 0;
}

static void editor_init_window(marea_window_t* win, const char* path) {
    strlcpy(win->editor_path, path, sizeof(win->editor_path));
    win->editor_dirty = 0;
    editor_load_file(win, path);
}

static void editor_draw_content(marea_window_t* win) {
    int body_x = win->x + 1;
    int body_y = win->y + TITLEBAR_HEIGHT + 1;
    int body_w = win->w - 2;
    int body_h = win->h - TITLEBAR_HEIGHT - 2;
    int toolbar_y = win->y + TITLEBAR_HEIGHT + 6;
    int save_x = win->x + win->w - 88;
    int save_y = win->y + 7;
    int cursor_x = win->x + EDITOR_MARGIN_L;
    int cursor_y = win->y + EDITOR_MARGIN_T;
    int max_x = win->x + win->w - EDITOR_MARGIN_L - 8;
    int max_y = win->y + win->h - 20;

    fill_rect(body_x, body_y, body_w, body_h, 0xFFF7F3EA);
    draw_hline(body_x, toolbar_y + 20, body_w, COL_BORDER);
    draw_text(win->x + 10, toolbar_y, win->editor_path, COL_BG_TITLEBAR, 0xFFF7F3EA);
    draw_text(win->x + 10, toolbar_y + 18, "Escribe texto. Ctrl+S o boton Save para guardar.", COL_TEXT_SECONDARY, 0xFFF7F3EA);

    fill_rounded_rect(save_x, save_y, 72, 18, 5, COL_ACCENT);
    draw_text(save_x + 16, save_y + 1, "Save", COL_TEXT_PRIMARY, 0);

    for (int i = 0; i < win->editor_len; ++i) {
        char c = win->editor_buf[i];

        if (c == '\n') {
            cursor_x = win->x + EDITOR_MARGIN_L;
            cursor_y += EDITOR_LINE_H;
            if (cursor_y > max_y) break;
            continue;
        }

        if (cursor_x > max_x) {
            cursor_x = win->x + EDITOR_MARGIN_L;
            cursor_y += EDITOR_LINE_H;
            if (cursor_y > max_y) break;
        }

        if (c >= 32 && c <= 126) {
            draw_char(cursor_x, cursor_y, c, 0xFF1A1E28, 0xFFF7F3EA);
        }
        cursor_x += 8;
    }

    fill_rect(win->x + 8, win->y + win->h - 22, win->w - 16, 14, 0xFFEDE6D8);
    draw_text(win->x + 12, win->y + win->h - 22, win->editor_status,
              win->editor_dirty ? COL_WARNING : COL_SUCCESS, 0xFFEDE6D8);
}

static void redraw_window(marea_window_t* win) {
    if (!win || !win->visible || win->minimized) {
        return;
    }

    draw_window_chrome(win);
    if (win->kind == WINDOW_KIND_EDITOR) {
        editor_draw_content(win);
    } else {
        fill_rect(win->x + 1, win->y + TITLEBAR_HEIGHT + 1,
                  win->w - 2, win->h - TITLEBAR_HEIGHT - 2, COL_TERM_BG);
    }
    mark_dirty_rect(win->x, win->y, win->w, win->h);
}

static void editor_handle_char(marea_window_t* win, char c) {
    if (c == 0x13) {
        editor_save_file(win);
        return;
    }

    if (c == '\b' || c == 0x08 || c == 0x7F) {
        if (win->editor_len > 0) {
            win->editor_len--;
            win->editor_buf[win->editor_len] = '\0';
            win->editor_dirty = 1;
            editor_set_status(win, "Editando...");
        }
        return;
    }

    if (c == '\r') {
        c = '\n';
    }

    if ((c == '\n' || (c >= 32 && c <= 126)) && win->editor_len < (int)sizeof(win->editor_buf) - 1) {
        win->editor_buf[win->editor_len++] = c;
        win->editor_buf[win->editor_len] = '\0';
        win->editor_dirty = 1;
        editor_set_status(win, "Editando...");
    }
}

/* ========================================================================= */
/* Window Management                                                         */
/* ========================================================================= */

static int create_terminal_window(void) {
    if (window_count >= MAX_WINDOWS) return -1;

    marea_window_t* win = &windows[window_count];
    memset(win, 0, sizeof(*win));
    win->id = window_count;
    win->w = (int)fb_info.width > 700 ? 640 : (int)fb_info.width - 60;
    win->h = (int)fb_info.height > 540 ? 420 : (int)fb_info.height - 120;
    win->x = ((int)fb_info.width - win->w) / 2 + window_count * 20;
    win->y = ((int)fb_info.height - TASKBAR_HEIGHT - win->h) / 2 + window_count * 20;
    win->visible = 1;
    win->focused = 1;
    win->minimized = 0;
    win->kind = WINDOW_KIND_TERMINAL;
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

static int create_editor_window(void) {
    marea_window_t* win;

    if (window_count >= MAX_WINDOWS) return -1;

    win = &windows[window_count];
    memset(win, 0, sizeof(*win));
    win->id = window_count;
    win->w = (int)fb_info.width > 760 ? 700 : (int)fb_info.width - 40;
    win->h = (int)fb_info.height > 560 ? 460 : (int)fb_info.height - 90;
    win->x = ((int)fb_info.width - win->w) / 2 + window_count * 12;
    win->y = ((int)fb_info.height - TASKBAR_HEIGHT - win->h) / 2 + window_count * 12;
    win->visible = 1;
    win->focused = 1;
    win->minimized = 0;
    win->kind = WINDOW_KIND_EDITOR;
    strlcpy(win->title, "Archivos", sizeof(win->title));

    if (focused_window >= 0) {
        windows[focused_window].focused = 0;
    }
    focused_window = window_count;

    editor_init_window(win, "/data/notas.txt");
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

static void handle_mouse_event(const input_event_t* ev) {
    if (ev->type == EV_REL) {
        int delta = ev->value * MOUSE_SENSITIVITY;
        int old_x = mouse_x;
        int old_y = mouse_y;

        cursor_restore_bg(mouse_x, mouse_y);
        mark_dirty_rect(old_x, old_y, CURSOR_W, CURSOR_H);

        if (ev->code == REL_X) mouse_x += delta;
        if (ev->code == REL_Y) mouse_y += delta;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x >= (int)fb_info.width - CURSOR_W) mouse_x = fb_info.width - CURSOR_W;
        if (mouse_y >= (int)fb_info.height - CURSOR_H) mouse_y = fb_info.height - CURSOR_H;

        if (dragging && drag_win >= 0) {
            windows[drag_win].x = mouse_x - drag_offset_x;
            windows[drag_win].y = mouse_y - drag_offset_y;

            if (windows[drag_win].y < 0) windows[drag_win].y = 0;
            if (windows[drag_win].y + windows[drag_win].h > (int)fb_info.height - TASKBAR_HEIGHT) {
                windows[drag_win].y = fb_info.height - TASKBAR_HEIGHT - windows[drag_win].h;
            }

            redraw_all();
        }

        cursor_save_bg(mouse_x, mouse_y);
        cursor_draw(mouse_x, mouse_y);
        mark_dirty_rect(mouse_x, mouse_y, CURSOR_W, CURSOR_H);
        present();
        return;
    }

    if (ev->type == EV_KEY && ev->code == BTN_LEFT) {
        if (ev->value == 1) {
            mouse_btn = 1;

            if (menu_open) {
                int item = hit_menu_item(mouse_x, mouse_y);
                if (item >= 0) {
                    if (strcmp(menu_items[item].label, "Terminal") == 0) {
                        create_terminal_window();
                    } else if (strcmp(menu_items[item].label, "Archivos") == 0) {
                        create_editor_window();
                    }
                    menu_open = 0;
                    redraw_all();
                    cursor_save_bg(mouse_x, mouse_y);
                    cursor_draw(mouse_x, mouse_y);
                    present();
                    return;
                }
            }

            if (hit_start_button(mouse_x, mouse_y)) {
                menu_open = !menu_open;
                redraw_all();
                cursor_save_bg(mouse_x, mouse_y);
                cursor_draw(mouse_x, mouse_y);
                present();
                return;
            }

            if (menu_open) {
                menu_open = 0;
                redraw_all();
                cursor_save_bg(mouse_x, mouse_y);
                cursor_draw(mouse_x, mouse_y);
                present();
            }

            int win_idx = find_window_at(mouse_x, mouse_y);
            if (win_idx >= 0) {
                marea_window_t* win = &windows[win_idx];

                if (focused_window != win_idx) {
                    if (focused_window >= 0) windows[focused_window].focused = 0;
                    win->focused = 1;
                    focused_window = win_idx;
                    redraw_all();
                    cursor_save_bg(mouse_x, mouse_y);
                    cursor_draw(mouse_x, mouse_y);
                    present();
                }

                if (hit_close_button(win, mouse_x, mouse_y)) {
                    win->visible = 0;
                    win->focused = 0;
                    if (focused_window == win_idx) focused_window = -1;
                    redraw_all();
                    cursor_save_bg(mouse_x, mouse_y);
                    cursor_draw(mouse_x, mouse_y);
                    present();
                    return;
                }

                if (win->kind == WINDOW_KIND_EDITOR && hit_editor_save_button(win, mouse_x, mouse_y)) {
                    editor_save_file(win);
                    redraw_window(win);
                    cursor_save_bg(mouse_x, mouse_y);
                    cursor_draw(mouse_x, mouse_y);
                    present();
                    return;
                }

                if (hit_titlebar(win, mouse_x, mouse_y)) {
                    dragging = 1;
                    drag_win = win_idx;
                    drag_offset_x = mouse_x - win->x;
                    drag_offset_y = mouse_y - win->y;
                }
            }
        } else {
            mouse_btn = 0;
            dragging = 0;
            drag_win = -1;
        }
    }
}

static void handle_keyboard_char(char c) {
    int old_x = mouse_x;
    int old_y = mouse_y;

    if (focused_window < 0 || !windows[focused_window].visible) {
        return;
    }

    marea_window_t* win = &windows[focused_window];
    cursor_restore_bg(mouse_x, mouse_y);
    mark_dirty_rect(old_x, old_y, CURSOR_W, CURSOR_H);

    if (win->kind == WINDOW_KIND_EDITOR) {
        editor_handle_char(win, c);
        redraw_window(win);
    } else {
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
    }

    mark_dirty_rect(win->x, win->y, win->w, win->h);
    cursor_save_bg(mouse_x, mouse_y);
    cursor_draw(mouse_x, mouse_y);
    mark_dirty_rect(mouse_x, mouse_y, CURSOR_W, CURSOR_H);
    present();
}

static void drain_mouse_events(int fd) {
    input_event_t ev;
    int available = 0;

    do {
        if (read(fd, &ev, sizeof(ev)) != (ssize_t)sizeof(ev)) {
            break;
        }
        handle_mouse_event(&ev);

        available = 0;
        if (ioctl(fd, FIONREAD, &available) != 0) {
            break;
        }
    } while (available >= (int)sizeof(ev));
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

static int hit_editor_save_button(marea_window_t* win, int mx, int my) {
    int save_x = win->x + win->w - 88;
    int save_y = win->y + 7;

    return (mx >= save_x && mx < save_x + 72 &&
            my >= save_y && my < save_y + 18);
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
        if (windows[i].kind == WINDOW_KIND_EDITOR) {
            editor_draw_content(&windows[i]);
        } else {
            fill_rect(windows[i].x + 1, windows[i].y + TITLEBAR_HEIGHT + 1,
                      windows[i].w - 2, windows[i].h - TITLEBAR_HEIGHT - 2, COL_TERM_BG);
            windows[i].term_cx = 0;
            windows[i].term_cy = 0;
            term_print(&windows[i], "eterOS Marea Shell\n", COL_ACCENT);
            term_print(&windows[i], "Escribe 'help' para comandos.\n\n", COL_TEXT_SECONDARY);
            term_draw_prompt(&windows[i]);
        }
    }

    draw_taskbar();
    draw_menu();
    mark_dirty_rect(0, 0, (int)fb_info.width, (int)fb_info.height);
}

/* ========================================================================= */
/* Drag State                                                                */
/* ========================================================================= */

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

    backbuffer_ptr = malloc(fb_size);
    if (!backbuffer_ptr) {
        printf("[Marea] Error: backbuffer alloc failed\n");
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
    present();

    /* 4. Open input devices */
    int mfd = open("/dev/input/mouse0", O_RDONLY);
    if (mfd < 0) {
        printf("[Marea] Warning: Cannot open /dev/input/mouse0\n");
    }

    /* 5. Keyboard input loop */
    int tfd = open("/dev/tty", O_RDWR);
    if (tfd < 0) {
        printf("[Marea] Warning: Cannot open /dev/tty\n");
    } else {
        struct termios tio;
        if (ioctl(tfd, TCGETS, &tio) == 0) {
            tio.c_lflag &= ~(ECHO | ICANON);
            ioctl(tfd, TCSETS, &tio);
        }
    }

    while (1) {
        struct pollfd fds[2];
        int nfds = 0;

        if (mfd >= 0) {
            fds[nfds].fd = mfd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }
        if (tfd >= 0) {
            fds[nfds].fd = tfd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }

        if (nfds == 0) {
            sleep(1);
            continue;
        }

        if (poll(fds, (nfds_t)nfds, -1) <= 0) {
            continue;
        }

        for (int i = 0; i < nfds; i++) {
            if (!(fds[i].revents & POLLIN)) {
                continue;
            }

            if (fds[i].fd == mfd) {
                drain_mouse_events(mfd);
            } else if (fds[i].fd == tfd) {
                char c;
                if (read(tfd, &c, 1) > 0) {
                    handle_keyboard_char(c);
                }
            }
        }
    }

    return 0;
}
