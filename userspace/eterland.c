#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "font.h"

#define FBIOGET_VSCREENINFO 0x4600

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
} fb_info_t;

uint8_t* fb_ptr;
fb_info_t fb_info;

void draw_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)fb_info.width || y < 0 || y >= (int)fb_info.height) return;
    
    if (fb_info.bpp == 32) {
        uint32_t* ptr = (uint32_t*)(fb_ptr + y * fb_info.pitch + x * 4);
        *ptr = color;
    } else if (fb_info.bpp == 24) {
        uint8_t* ptr = fb_ptr + y * fb_info.pitch + x * 3;
        ptr[0] = color & 0xFF;         // B
        ptr[1] = (color >> 8) & 0xFF;  // G
        ptr[2] = (color >> 16) & 0xFF; // R
    }
}

uint32_t read_pixel(int x, int y) {
    if (x < 0 || x >= (int)fb_info.width || y < 0 || y >= (int)fb_info.height) return 0;
    if (fb_info.bpp == 32) {
        uint32_t* ptr = (uint32_t*)(fb_ptr + y * fb_info.pitch + x * 4);
        return *ptr;
    } else if (fb_info.bpp == 24) {
        uint8_t* ptr = fb_ptr + y * fb_info.pitch + x * 3;
        uint32_t b = ptr[0];
        uint32_t g = ptr[1];
        uint32_t r = ptr[2];
        return (r << 16) | (g << 8) | b;
    }
    return 0;
}

void draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb_info.width) w = fb_info.width - x;
    if (y + h > (int)fb_info.height) h = fb_info.height - y;
    if (w <= 0 || h <= 0) return;
    if (fb_ptr == NULL) return;

    if (fb_info.bpp == 32) {
        for (int i = 0; i < h; i++) {
            uint32_t* ptr = (uint32_t*)(fb_ptr + (y + i) * fb_info.pitch + x * 4);
            for (int j = 0; j < w; j++) {
                ptr[j] = color;
            }
        }
    } else {
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                draw_pixel(x + j, y + i, color);
            }
        }
    }
}

void draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    int index = (unsigned char)c * 16;
    for (int row = 0; row < 16; row++) {
        uint8_t bits = font8x16[index + row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                draw_pixel(x + col, y + row, fg);
            } else if (bg != 0xFFFFFFFF) {
                draw_pixel(x + col, y + row, bg);
            }
        }
    }
}

void draw_string(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            y += 16;
            cx = x;
        } else {
            draw_char(cx, y, *str, fg, bg);
            cx += 8;
        }
        str++;
    }
}

void scroll_rect(int x, int y, int w, int h, int dy, uint32_t bg_color) {
    if (dy >= h) {
        draw_rect(x, y, w, h, bg_color);
        return;
    }
    int bytes_per_pixel = fb_info.bpp / 8;
    int row_bytes = w * bytes_per_pixel;
    
    for (int row = y; row < y + h - dy; row++) {
        uint8_t* dst = fb_ptr + row * fb_info.pitch + x * bytes_per_pixel;
        uint8_t* src = fb_ptr + (row + dy) * fb_info.pitch + x * bytes_per_pixel;
        memmove(dst, src, row_bytes);
    }
    draw_rect(x, y + h - dy, w, dy, bg_color);
}

// Window state
int win_x, win_y, win_w, win_h;
int term_bg = 0x0F111A;
int term_fg = 0xE0E6ED;
int title_bg = 0x1E222A;
int title_fg = 0xFFFFFF;
int border_color = 0x3A3F4C;
int desktop_bg = 0x21252D;
int taskbar_color = 0x15181E;

int term_cx = 0, term_cy = 0;
int term_cols, term_rows;
int margin_l = 8, margin_t = 28;

char input_buffer[256];
int input_len = 0;

void draw_window() {
    draw_rect(win_x - 1, win_y - 1, win_w + 2, win_h + 2, border_color);
    draw_rect(win_x, win_y, win_w, 24, title_bg);
    draw_string(win_x + 8, win_y + 4, "Terminal", title_fg, 0xFFFFFFFF);
    draw_rect(win_x + win_w - 24, win_y + 4, 16, 16, 0xE74C3C);
    draw_rect(win_x + win_w - 44, win_y + 4, 16, 16, 0x2ECC71);
    draw_rect(win_x + win_w - 64, win_y + 4, 16, 16, 0xF1C40F);
    draw_rect(win_x, win_y + 24, win_w, win_h - 24, term_bg);
}

void term_newline() {
    term_cx = 0;
    term_cy++;
    if (term_cy >= term_rows) {
        term_cy = term_rows - 1;
        scroll_rect(win_x + margin_l, win_y + margin_t, win_w - margin_l*2, win_h - margin_t - 8, 16, term_bg);
    }
}

void term_print_char(char c, uint32_t fg) {
    if (c == '\n') {
        term_newline();
    } else if (c == '\b' || c == 0x08 || c == 0x7F) {
        if (term_cx > 0) {
            term_cx--;
            draw_char(win_x + margin_l + term_cx * 8, win_y + margin_t + term_cy * 16, ' ', fg, term_bg);
        }
    } else if (c >= 32 && c <= 126) {
        if (term_cx >= term_cols) {
            term_newline();
        }
        draw_char(win_x + margin_l + term_cx * 8, win_y + margin_t + term_cy * 16, c, fg, term_bg);
        term_cx++;
    }
}

void term_print(const char* str, uint32_t fg) {
    while (*str) {
        term_print_char(*str, fg);
        str++;
    }
}

void draw_desktop() {
    draw_rect(0, 0, fb_info.width, fb_info.height, desktop_bg);
    draw_rect(0, fb_info.height - 40, fb_info.width, 40, taskbar_color);
    
    draw_rect(10, fb_info.height - 35, 80, 30, 0x2980B9);
    draw_string(25, fb_info.height - 28, "Inicio", 0xFFFFFF, 0xFFFFFFFF);
    
    draw_rect(100, fb_info.height - 35, 150, 30, 0x34495E);
    draw_string(110, fb_info.height - 28, "Terminal", 0xFFFFFF, 0xFFFFFFFF);
}

void execute_command() {
    input_buffer[input_len] = '\0';
    if (input_len > 0) {
        char cmd[256];
        char arg[256] = {0};
        int i = 0, j = 0;
        
        while (input_buffer[i] && input_buffer[i] != ' ') {
            cmd[i] = input_buffer[i];
            i++;
        }
        cmd[i] = '\0';
        
        if (input_buffer[i] == ' ') i++;
        while (input_buffer[i]) {
            arg[j++] = input_buffer[i++];
        }
        arg[j] = '\0';

        if (strcmp(cmd, "help") == 0) {
            term_print("\nEterland Terminal UI Simulator v0.2.0 Genesis SMP\n", 0x2ECC71);
            term_print("Comandos: help, clear, echo, uname, ls, cat, run, exit\n", term_fg);
        } else if (strcmp(cmd, "clear") == 0) {
            draw_rect(win_x, win_y + 24, win_w, win_h - 24, term_bg);
            term_cx = 0; term_cy = 0;
            input_len = 0;
            return;
        } else if (strcmp(cmd, "echo") == 0) {
            term_print("\n", term_fg);
            term_print(arg, term_fg);
            term_print("\n", term_fg);
        } else if (strcmp(cmd, "uname") == 0) {
            term_print("\neterOS (Marea UI)\n", 0x3498DB);
            term_print("Graphics Server: Eterland (Zero-copy capable)\n", term_fg);
        } else if (strcmp(cmd, "run") == 0) {
            if (strlen(arg) > 0) {
                term_print("\nEjecutando ", term_fg);
                term_print(arg, 0xF1C40F);
                term_print("...\n", term_fg);
                
                int child = fork();
                if (child == 0) {
                    char path[256] = "/";
                    strlcat(path, arg, sizeof(path));
                    
                    char *argv[] = { path, NULL };
                    char *envp[] = { NULL };
                    execve(path, argv, envp);
                    
                    /* If execve returns, it failed */
                    exit(1);
                }
            } else {
                term_print("\nUso: run <archivo.elf>\n", 0xF1C40F);
            }
        } else if (strcmp(cmd, "exit") == 0) {
            term_print("\nSaliendo...\n", 0xE74C3C);
            exit(0);
        } else {
            term_print("\nComando no encontrado: ", 0xE74C3C);
            term_print(cmd, 0xE74C3C);
            term_print("\n", term_fg);
        }
    } else {
        term_print("\n", term_fg);
    }
    input_len = 0;
    term_print("root@eterOS", 0x2ECC71);
    term_print(" ~ $ ", 0x3498DB);
}

typedef struct {
    uint16_t type;
    uint16_t code;
    int32_t  value;
} input_event_t;
#define EV_REL      0x02
#define EV_KEY      0x01
#define REL_X       0x00
#define REL_Y       0x01
#define BTN_LEFT    0x110

#define CURSOR_W 10
#define CURSOR_H 15
uint32_t cursor_bg[10 * 15];

void save_cursor_bg(int cx, int cy) {
    for (int i=0; i<CURSOR_H; i++) {
        for (int j=0; j<CURSOR_W; j++) {
            cursor_bg[i*CURSOR_W + j] = read_pixel(cx+j, cy+i);
        }
    }
}

void restore_cursor_bg(int cx, int cy) {
    for (int i=0; i<CURSOR_H; i++) {
        for (int j=0; j<CURSOR_W; j++) {
            draw_pixel(cx+j, cy+i, cursor_bg[i*CURSOR_W + j]);
        }
    }
}

void draw_cursor(int cx, int cy) {
    for (int i=0; i<CURSOR_H; i++) {
        for (int j=0; j<CURSOR_W; j++) {
            if (j <= i/2) draw_pixel(cx+j, cy+i, 0xFFFFFF);
            else if (j == (i/2)+1) draw_pixel(cx+j, cy+i, 0x000000); 
        }
    }
}

int mx, my;



int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    printf("[Eterland] Starting Compositor UI...\n");

    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) return 1;

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_info) != 0) return 1;

    size_t fb_size = fb_info.height * fb_info.pitch;
    fb_ptr = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_ptr == MAP_FAILED) return 1;



    // Center window
    win_w = fb_info.width > 640 ? 640 : fb_info.width - 40;
    win_h = fb_info.height > 480 ? 400 : fb_info.height - 80;
    win_x = (fb_info.width - win_w) / 2;
    win_y = (fb_info.height - win_h) / 2 - 20;

    term_cols = (win_w - margin_l*2) / 8;
    term_rows = (win_h - margin_t - 8) / 16;

    draw_desktop();
    draw_window();

    term_print("eterOS Marea UI v0.2.0 Genesis SMP\n", 0x3498DB);
    term_print("Type 'help' for commands.\n\n", 0x95A5A6);
    term_print("root@eterOS", 0x2ECC71);
    term_print(" ~ $ ", 0x3498DB);

    int tfd = open("/dev/tty", O_RDWR);
    if (tfd < 0) return 1;

    int mfd = open("/dev/input/mouse0", O_RDONLY);

    mx = fb_info.width / 2;
    my = fb_info.height / 2;

    save_cursor_bg(mx, my);
    draw_cursor(mx, my);

    struct pollfd fds[2];
    fds[0].fd = tfd;
    fds[0].events = POLLIN;
    fds[1].fd = mfd;
    fds[1].events = POLLIN;

    while (1) {
        if (poll(fds, 2, -1) > 0) {
            if (mfd >= 0 && (fds[1].revents & POLLIN)) {
                input_event_t ev;
                if (read(mfd, &ev, sizeof(ev)) == sizeof(ev)) {
                    if (ev.type == EV_REL) {
                        int old_x = mx;
                        int old_y = my;
                        restore_cursor_bg(old_x, old_y);
                        if (ev.code == REL_X) mx += ev.value;
                        if (ev.code == REL_Y) my += ev.value;

                        if (mx < 0) mx = 0;
                        if (my < 0) my = 0;
                        if (mx >= (int)fb_info.width - CURSOR_W) mx = (int)fb_info.width - CURSOR_W;
                        if (my >= (int)fb_info.height - CURSOR_H) my = (int)fb_info.height - CURSOR_H;

                        save_cursor_bg(mx, my);
                        draw_cursor(mx, my);
                    }
                }
            }
            if (fds[0].revents & POLLIN) {
                char c;
                if (read(tfd, &c, 1) > 0) {
                    int old_x = mx;
                    int old_y = my;
                    restore_cursor_bg(old_x, old_y);
                    if (c == '\r' || c == '\n') {
                        execute_command();
                    } else if (c == '\b' || c == 0x08 || c == 0x7F) {
                        if (input_len > 0) {
                            input_len--;
                            term_print_char('\b', term_fg);
                        }
                    } else if (c >= 32 && c <= 126) {
                        if (input_len < (int)sizeof(input_buffer)-1) {
                            input_buffer[input_len++] = c;
                            term_print_char(c, term_fg);
                        }
                    }
                    save_cursor_bg(mx, my);
                    draw_cursor(mx, my);
                }
            }
        }
    }

    return 0;
}
