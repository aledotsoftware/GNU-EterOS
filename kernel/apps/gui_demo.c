/*
 * éterOS - Desktop Environment (Flux UI)
 * 
 * Implementación de un entorno de escritorio básico con:
 * - Barra de tareas y Menú Inicio estilo "Aurora" (Touch-optimized)
 * - Ventana de Terminal interactiva mejorada
 * - Soporte de Mouse y Teclado
 * - Snapping de ventanas
 */

#include <ui/window.h>
#include <ui/omni.h>
#include <task.h>
#include <types.h>
#include <string.h>
#include <vga.h>
#include <gui_demo.h>
#include <net/socket.h>
#include <net/defs.h>

#ifndef htons
#define htons(x) ((((x) & 0xff) << 8) | (((x) & 0xff00) >> 8))
#endif
#include <timer.h>
#include <keyboard.h>
#include <mouse.h>
#include <framebuffer.h>
#include <pci.h>
#include <shell.h>
#include <pmm.h>
#include <serial.h>
#include <ui/image.h>
#include <rtc.h>
#include <fs/initrd.h>
#include <syscall.h>
#include <hal.h>

extern int task_get_cpu_load(void);
extern int task_kill(uint32_t pid);
extern uint32_t my_ip; // Defined in stack.c

/* ========================================================================= */
/* Constantes y Configuración Visual                                         */
/* ========================================================================= */
#define TASKBAR_HEIGHT      40
#define TITLE_BAR_HEIGHT    30

static volatile bool desktop_running = true;

/* Ventanas y Estado */
static window_t* win_sysinfo = NULL;
static window_t* win_devman = NULL;

/* Flux Hub (Universal Launcher) */
static bool hub_active = false;
static char hub_input[64] = "";
static void draw_flux_hub(void);
static void handle_hub_input(char c);

/* Mouse */
static int32_t mouse_x = 512;
static int32_t mouse_y = 384;
static bool    mouse_left_btn = false;

/* Performance: Intelligent Redraw */
static bool gui_needs_redraw = true;



/* ========================================================================= */
/* Flux UI - The Ontology of the System                                      */
/* ========================================================================= */

/* Ontología del Sistema: Colores y Materiales */
/* Ontología del Sistema: Colores y Materiales */
#define FLUX_VOID           0x000000  /* Pure Void for scaling contrast */
#define FLUX_ACCENT_CYAN    0x00FFFF  /* Flow */
#define FLUX_ACCENT_VIOLET  0x9D00FF  /* Deep */
#define FLUX_ACCENT_AMBER   0xFFBF00  /* Alert */
#define FLUX_CARD_BG        0x2A2A2A  /* Lighter grey for visibility */
#define FLUX_BAR_BG         0x151515  /* Status Bar */
#define FLUX_TEXT_PRIMARY   0xFFFFFF
#define FLUX_TEXT_SECONDARY 0xAAAAAA
#define FLUX_DESTRUCTIVE_BG 0xFF440000

typedef enum {
    FLUX_MACRO = 0,   /* Constellation (Overview) */
    FLUX_FOCUS = 1    /* Detail (Active App) */
} flux_zoom_level_t;

/* Constellation Nodes (Apps disponibles) */
typedef enum {
    NODE_TERMINAL = 0,
    NODE_SYSMON,
    NODE_MATRIX,
    NODE_SETTINGS,
    NODE_FILES,
    NODE_SANTITRAVEL,
    /* Nuevas Apps */
    NODE_CALC,
    NODE_EDITOR,
    NODE_CLOCK,
    NODE_WEATHER,
    NODE_PAINT,
    NODE_MUSIC,
    NODE_GALLERY,
    NODE_BROWSER,
    NODE_CALENDAR,
    NODE_DEVMAN,
    NODE_DOOM,
    NODE_COUNT
} flux_node_id_t;

typedef struct {
    flux_node_id_t id;
    const char* title;
    uint32_t color;
} flux_app_meta_t;

static const flux_app_meta_t FLUX_APPS[] = {
    { NODE_TERMINAL, "Terminal",     FLUX_ACCENT_CYAN },
    { NODE_SYSMON,   "Monitor",      FLUX_ACCENT_VIOLET },
    { NODE_MATRIX,   "The Matrix",   0x00FF00 },
    { NODE_SETTINGS, "Ajustes",      FLUX_TEXT_SECONDARY },
    { NODE_FILES,    "Archivos",     FLUX_ACCENT_AMBER },
    { NODE_SANTITRAVEL,"Viajes",     0xFF5555 },
    { NODE_CALC,     "Calc",         0x4488FF },
    { NODE_EDITOR,   "Notas",        0xFFDD44 },
    { NODE_CLOCK,    "Reloj",        0xAAFFEE },
    { NODE_WEATHER,  "Clima",        0x88CCFF },
    { NODE_PAINT,    "Lienzo",       0xFF00FF },
    { NODE_MUSIC,    "Musica",       0xFF4488 },
    { NODE_GALLERY,  "Galeria",      0xAADD55 },
    { NODE_BROWSER,  "Navegador",    0x44FFFF },
    { NODE_CALENDAR, "Calendario",   0xDDAA55 },
    { NODE_DEVMAN,   "Dispositivos", FLUX_ACCENT_AMBER }
    // { NODE_DOOM,     "DOOM",         0xCC0000 } // Engine missing
};


/* Forward declarations */
static void flux_notify(const char* title, const char* message, uint32_t accent);
static void flux_set_zoom(flux_zoom_level_t level, flux_node_id_t node);
static void flux_launch_space(flux_node_id_t node);

/* Forward declarations for Window Callbacks */
static void terminal_on_paint(window_t* win);
static void terminal_on_key(window_t* win, char key);
static void sysinfo_on_paint(window_t* win);
static void sysinfo_on_mouse(window_t* win, int x, int y, int b);
static void settings_on_paint(window_t* win);
static void settings_on_mouse(window_t* win, int x, int y, int b);
static void files_on_paint(window_t* win);
static void files_on_mouse(window_t* win, int x, int y, int b);
static void travel_on_paint(window_t* win);
static void travel_on_mouse(window_t* win, int x, int y, int b);
static void browser_on_paint(window_t* win);
static void browser_on_mouse(window_t* win, int x, int y, int b);
static void browser_on_key(window_t* win, char key);
static void devman_on_paint(window_t* win);
static void doom_on_paint(window_t* win);

/* Helper Draw Functions */
static void draw_generic_app_content(window_t* win, const char* title, uint32_t bg_col);

static void draw_clock_content(window_t* win);

/* Animation State Forward Decl (needed for input handling) */
static int zoom_progress = 0; 
static flux_zoom_level_t target_zoom = FLUX_MACRO;
static flux_node_id_t zooming_node = NODE_TERMINAL;
static rect_t source_rect = {0,0,0,0}; /* Constellation rect */
static rect_t target_rect = {40, 40, 944, 688}; /* Focus rect */ 

/* Doom Integration Disabled - Engine missing */
// extern uint32_t* DG_ScreenBuffer;
// extern void doomgeneric_Create(int argc, char **argv);
// extern void doomgeneric_Tick(void);
// extern void doom_handle_input(int key, int pressed);

static bool doom_running = false;
static window_t* win_doom = NULL;

void task_doom_loop(void) {
    /* char* argv[] = {"doom", "-iwad", "/initrd/doom1.wad", NULL};
    doomgeneric_Create(3, argv);

    while (1) {
        doomgeneric_Tick();
        task_sleep(10);
    } */
    task_exit();
}

static void draw_doom_content(void) {
    if (!win_doom) return;
    int w = win_doom->bounds.w;
    int h = win_doom->bounds.h;
    wm_fill_rect(win_doom, (rect_t){0, 0, w, h}, 0x000000);
    wm_print_at(win_doom, 20, 20, "DOOM Engine no encontrado.");
}


/* ========================================================================= */
/* Terminal App Logica Multi-instancia con Scroll                            */
/* ========================================================================= */

#define TERM_VISIBLE_LINES 16
#define TERM_VISIBLE_COLS  42
/* Aumentamos buffer para permitir "scroll back" conceptual */
#define TERM_BUFFER_LINES  100 
#define MAX_TERMINALS 15

typedef struct {
    window_t* win;
    char buffer[TERM_BUFFER_LINES][TERM_VISIBLE_COLS + 1]; /* Historial */
    int  cursor_x; /* 0..TERM_VISIBLE_COLS-1 */
    int  cursor_y; /* Posición absoluta en buffer */
    int  scroll_offset; 
    char input_buf[64];
    int  input_pos;
    bool used;
} term_instance_t;

static term_instance_t terminals[MAX_TERMINALS];
static term_instance_t* focused_term = NULL;
static term_instance_t* hook_term = NULL;

static void term_init_all(void) {
    for (int i = 0; i < MAX_TERMINALS; i++) {
        terminals[i].used = false;
        terminals[i].win = NULL;
    }
    focused_term = NULL;
    hook_term = NULL;
}

static void term_clear(term_instance_t* term) {
    if (!term) return;
    for (int i = 0; i < TERM_BUFFER_LINES; i++) {
        memset(term->buffer[i], 0, sizeof(term->buffer[i]));
    }
    term->cursor_x = 0;
    term->cursor_y = 0;
    term->scroll_offset = 0;
    term->input_pos = 0;
    memset(term->input_buf, 0, sizeof(term->input_buf));
}

static void term_putc(term_instance_t* term, char c) {
    if (!term) return;

    if (c == '\n') {
        term->cursor_y++;
        term->cursor_x = 0;
    } else if (c == '\b') {
        if (term->cursor_x > 0) {
            term->cursor_x--;
            term->buffer[term->cursor_y][term->cursor_x] = 0;
        }
    } else if (term->cursor_x < TERM_VISIBLE_COLS - 1) {
        term->buffer[term->cursor_y][term->cursor_x++] = c;
        term->buffer[term->cursor_y][term->cursor_x] = 0;
    }

    if (term->cursor_y >= TERM_BUFFER_LINES) {
        for (int i = 0; i < TERM_BUFFER_LINES - 1; i++) {
            memcpy(term->buffer[i], term->buffer[i+1], TERM_VISIBLE_COLS + 1);
        }
        memset(term->buffer[TERM_BUFFER_LINES-1], 0, TERM_VISIBLE_COLS + 1);
        term->cursor_y = TERM_BUFFER_LINES - 1;
    }
    
    term->scroll_offset = 0;
}

static void term_print(term_instance_t* term, const char* text) {
    if (!term) return;
    while (*text) term_putc(term, *text++);
}

static void gui_term_hook(char c) {
    if (hook_term) term_putc(hook_term, c);
    serial_putchar(c); /* Mirror to serial for easier debugging */
}

static void term_execute_cmd(term_instance_t* term, const char* cmd) {
    if (!term) return;
    
    term_print(term, "\n");
    
    if (strcmp(cmd, "clear") == 0) {
        term_clear(term);
        return;
    }
    
    if (strcmp(cmd, "exit") == 0) {
        if (term->win) term->win->active = false;
        term->used = false;
        if (focused_term == term) focused_term = NULL;
        return;
    }

    hook_term = term;
    terminal_set_hook(gui_term_hook);
    int ret = shell_exec((char*)cmd);
    terminal_set_hook(NULL);
    hook_term = NULL;
    
    if (ret != 0) { }
    
    term_print(term, "\neterOS> ");
}

static void term_handle_key(term_instance_t* term, char c) {
    if (!term) { serial_write_string("[TERM] Err: term is NULL\n"); return; }
    
    serial_write_string("[TERM] Handled Key\n");

    if (c == '\n') {
        term_execute_cmd(term, term->input_buf);
        memset(term->input_buf, 0, sizeof(term->input_buf));
        term->input_pos = 0;
    } else if (c == '\b') {
        if (term->input_pos > 0) {
            term->input_pos--;
            term->input_buf[term->input_pos] = 0;
            term_putc(term, '\b'); 
        }
    } else {
        if (term->input_pos < 60) {
            term->input_buf[term->input_pos++] = c;
            term->input_buf[term->input_pos] = 0;
            term_putc(term, c); 
        }
    }
}

static void draw_terminal_content(term_instance_t* term) {
    if (!term || !term->win || !term->win->active) return;

    rect_t client_area = {0, 0, term->win->bounds.w, term->win->bounds.h};
    wm_fill_rect(term->win, client_area, UI_COLOR_BLACK);
    
    int start_line = term->cursor_y - (TERM_VISIBLE_LINES - 1) - term->scroll_offset;
    if (start_line < 0) start_line = 0;
    
    int draw_y = 5;
    for (int i = 0; i < TERM_VISIBLE_LINES; i++) {
        int buf_idx = start_line + i;
        if (buf_idx < TERM_BUFFER_LINES) {
            if (term->buffer[buf_idx][0] != 0) {
                wm_print_at(term->win, 5, draw_y, term->buffer[buf_idx]);
            }
        }
        draw_y += 16;
    }
    
    int rel_cursor_y = term->cursor_y - start_line;
    if (rel_cursor_y >= 0 && rel_cursor_y < TERM_VISIBLE_LINES) {
        int cursor_px = 5 + (term->cursor_x * 8);
        int cursor_py = 5 + (rel_cursor_y * 16);
        bool blink = (timer_get_ticks() / 50) % 2 == 0;
        uint32_t cursor_color = (term == focused_term) ? UI_COLOR_GREEN : 0x404040;
        if (blink || term != focused_term) {
             wm_fill_rect(term->win, (rect_t){cursor_px, cursor_py + 12, 8, 2}, cursor_color);
        }
    }
}

static void term_create(void) {
    int slot = -1;
    for (int i = 0; i < MAX_TERMINALS; i++) {
        if (!terminals[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) return; 
    
    char title[32];
    strlcpy(title, "Terminal ", 32);
    char num[4];
    itoa_s(slot + 1, num, 4, 10);
    strlcpy(title + 9, num, 32);
    
    int x = 50 + (slot * 30);
    int y = 50 + (slot * 30);
    if (x > 600) x = 100;
    if (y > 400) y = 100;
    
    /* Ventana Full-Space para Terminal */
    terminals[slot].win = wm_create_window(x, y, 900, 600, title);
    if (!terminals[slot].win) return;
    
    terminals[slot].win->bg_color = UI_COLOR_BLACK;
    terminals[slot].win->fg_color = UI_COLOR_WHITE; /* Ensure text is visible */

    terminals[slot].win->on_paint = terminal_on_paint;
    terminals[slot].win->on_key = terminal_on_key;
    terminals[slot].used = true;
    
    term_clear(&terminals[slot]);
    term_print(&terminals[slot], "eterOS Genesis Shell\n\n");
    term_print(&terminals[slot], "eterOS> ");
    focused_term = &terminals[slot];
}

/* ========================================================================= */
/* System Apps (SysInfo, Matrix)                                             */
/* ========================================================================= */

static void draw_progress_bar(window_t* win, int x, int y, int w, int h, int percent_1000, uint32_t color) {
    rect_t r = {x, y, w, h};
    if (win) {
        wm_fill_rect(win, r, 0x404040);
        int fill_w = (w * percent_1000) / 1000;
        if (fill_w > w) fill_w = w;
        if (fill_w < 0) fill_w = 0;
        wm_fill_rect(win, (rect_t){x, y, fill_w, h}, color);
    } else {
        omni_fill_rect(x, y, w, h, 0x404040);
        int fill_w = (w * percent_1000) / 1000;
        if (fill_w > w) fill_w = w;
        if (fill_w < 0) fill_w = 0;
        omni_fill_rect(x, y, fill_w, h, color);
    }
}

static void handle_sysinfo_click(int x, int y) {
    /* Handle Kill Logic */
    if (!win_sysinfo) return;
    int w = win_sysinfo->bounds.w;
    int list_y = 180;
    
    if (y > list_y + 26) {
        int row = (y - (list_y + 35)) / 16;
        if (row >= 0) {
            /* Find task at this row */
            int visible_row = 0;
            int max_slots = task_get_max();
            for (int i = 0; i < max_slots; i++) {
                task_t* t = task_get_at(i);
                if (t && t->name[0] != '\0' && t->state != TASK_DEAD) {
                    if (visible_row == row) {
                        /* Check X coordinate for Kill Button (w-30 area) */
                        if (x > w - 40) {
                            task_kill(t->id);
                        }
                        return;
                    }
                    visible_row++;
                }
            }
        }
    }
}

static void draw_sysinfo_content(void) {
    int w = win_sysinfo->bounds.w;
    int h = win_sysinfo->bounds.h;
    wm_fill_rect(win_sysinfo, (rect_t){0, 0, w, h}, 0x1A1A1A);
    
    /* Header Bar */
    wm_fill_rect(win_sysinfo, (rect_t){0, 0, w, 40}, 0x252525);
    wm_print_at(win_sysinfo, 20, 12, "ETER-MONITOR v2.1 (Live)");
    
    /* Stats Layout */
    int card_w = (w / 2) - 30;
    
    /* RAM CARD */
    rect_t ram_card = {20, 60, card_w, 100};
    wm_fill_rect(win_sysinfo, ram_card, 0x2A2A2A);
    wm_fill_rect(win_sysinfo, (rect_t){20, 60, 4, 100}, FLUX_ACCENT_AMBER);
    
    /* Cache expensive calculations (RAM/CPU/Net) - Update every 500ms */
    static uint32_t last_update = 0;
    static char cached_ram_buf[64] = "RAM: ...";
    static int cached_ram_pct = 0;
    static char cached_cpu_buf[32] = "CPU Load: ...";
    static int cached_cpu_load = 0;
    static char cached_net_buf[32] = "IP: ...";
    
    if (timer_get_ticks() - last_update > 50 || last_update == 0) {
        /* Update RAM */
        uint64_t total = pmm_get_total_ram();
        uint64_t free  = pmm_get_free_ram();
        uint64_t used  = total - free;
        cached_ram_pct = (total > 0) ? (used * 1000) / total : 0;

        strlcpy(cached_ram_buf, "RAM: ", 64);
        char num[16];
        itoa_s(used / (1024*1024), num, 16, 10);
        strlcat(cached_ram_buf, num, 64);
        strlcat(cached_ram_buf, " MB", 64);

        /* Update CPU */
        cached_cpu_load = task_get_cpu_load();
        strlcpy(cached_cpu_buf, "CPU Load: ", 32);
        itoa_s(cached_cpu_load, num, 16, 10);
        strlcat(cached_cpu_buf, num, 32);
        strlcat(cached_cpu_buf, "% (1 Core)", 32);

        /* Update Net */
        strlcpy(cached_net_buf, "IP: ", 32);
        uint8_t* ip = (uint8_t*)&my_ip;
        if (my_ip == 0) {
            strlcat(cached_net_buf, "No Conn", 32);
        } else {
            itoa_s(ip[0], num, 16, 10); strlcat(cached_net_buf, num, 32); strlcat(cached_net_buf, ".", 32);
            itoa_s(ip[1], num, 16, 10); strlcat(cached_net_buf, num, 32); strlcat(cached_net_buf, ".", 32);
            itoa_s(ip[2], num, 16, 10); strlcat(cached_net_buf, num, 32); strlcat(cached_net_buf, ".", 32);
            itoa_s(ip[3], num, 16, 10); strlcat(cached_net_buf, num, 32);
        }

        last_update = timer_get_ticks();
    }

    wm_print_at(win_sysinfo, 40, 75, cached_ram_buf);
    draw_progress_bar(win_sysinfo, 40, 100, card_w - 40, 12, cached_ram_pct, FLUX_ACCENT_AMBER);

    /* CPU & NET CARD */
    rect_t cpu_card = {w/2 + 10, 60, card_w, 100};
    wm_fill_rect(win_sysinfo, cpu_card, 0x2A2A2A);
    wm_fill_rect(win_sysinfo, (rect_t){w/2 + 10, 60, 4, 100}, FLUX_ACCENT_CYAN);
    
    wm_print_at(win_sysinfo, w/2 + 30, 70, cached_cpu_buf);
    
    /* CPU Progress Bar */
    draw_progress_bar(win_sysinfo, w/2 + 30, 90, card_w - 40, 10, cached_cpu_load * 10, FLUX_ACCENT_CYAN);

    /* Network Info */
    wm_print_at(win_sysinfo, w/2 + 30, 110, cached_net_buf);

    /* Process List (Lower Section) */
    int list_y = 180;
    wm_fill_rect(win_sysinfo, (rect_t){20, list_y, w - 40, h - list_y - 20}, 0x222222);
    wm_print_at(win_sysinfo, 40, list_y + 10, "PID   State   Name                 [Kill]");
    wm_fill_rect(win_sysinfo, (rect_t){40, list_y + 26, w - 80, 1}, 0x444444);
    
    int row = 0;
    int max_slots = task_get_max();
    for (int i = 0; i < max_slots; i++) {
        task_t* t = task_get_at(i);
        if (t && t->name[0] != '\0' && t->state != TASK_DEAD) {
             char row_buf[80] = "";
             char pid_s[8];
             itoa_s(t->id, pid_s, 8, 10);
             strlcpy(row_buf, pid_s, 80);
             int pad = 6 - strlen(pid_s);
             while(pad-- > 0) strlcat(row_buf, " ", 80);
             
             const char* st = (t->state == TASK_RUNNING) ? "RUN " : "RDY ";
             strlcat(row_buf, st, 80);
             strlcat(row_buf, "    ", 80);
             strlcat(row_buf, t->name, 80);
             
             wm_print_at(win_sysinfo, 40, list_y + 35 + (row * 16), row_buf);
             
             /* Draw valid kill button */
             int btn_x = w - 35;
             int btn_y = list_y + 35 + (row * 16);

             /* Hover Check */
             int global_btn_x = win_sysinfo->bounds.x + btn_x;
             int global_btn_y = win_sysinfo->bounds.y + TITLE_BAR_HEIGHT + btn_y;

             bool hover = (mouse_x >= global_btn_x && mouse_x < global_btn_x + 24 &&
                           mouse_y >= global_btn_y && mouse_y < global_btn_y + 16);

             /* 🎨 Palette: Improved Kill Button UX */
             uint32_t btn_bg = hover ? FLUX_DESTRUCTIVE_BG : 0x444444;
             uint32_t btn_fg = 0xFFFFFF;

             /* Draw Background Rect (Visual Button) - Use full row height (16) */
             wm_fill_rect(win_sysinfo, (rect_t){btn_x, btn_y, 20, 16}, btn_bg);

             /* Draw Centered 'X' using window context */
             uint32_t old_fg = win_sysinfo->fg_color;
             uint32_t old_bg = win_sysinfo->bg_color;

             win_sysinfo->fg_color = btn_fg;
             win_sysinfo->bg_color = btn_bg;

             wm_print_at(win_sysinfo, btn_x + 6, btn_y, "X");

             win_sysinfo->fg_color = old_fg;
             win_sysinfo->bg_color = old_bg;

             if (hover) {
                 /* 🎨 Palette: Tooltip for clarity */
                 const char* tip_text = "Terminate Process";
                 int tip_w = strlen(tip_text) * 8 + 10;
                 int tip_h = 20;
                 /* Tooltip position (global) */
                 int tip_x = global_btn_x - tip_w - 5;
                 int tip_y = global_btn_y - 2;

                 /* Draw global overlay */
                 omni_fill_rect(tip_x, tip_y, tip_w, tip_h, 0x222222);
                 omni_draw_rect(tip_x, tip_y, tip_w, tip_h, 0x666666);
                 omni_draw_string(NULL, tip_x + 5, tip_y + 2, tip_text, 0xFFFFFF, 0x222222);
             }
             
             row++;
             if (row > 10) break;
        }
    }
}

static void draw_devman_content(void) {
    if (!win_devman) return;
    int w = win_devman->bounds.w;
    int h = win_devman->bounds.h;
    
    wm_fill_rect(win_devman, (rect_t){0, 0, w, h}, 0x121212);
    
    /* Header */
    wm_fill_rect(win_devman, (rect_t){0, 0, w, 45}, 0x1E1E1E);
    wm_print_at(win_devman, 20, 15, "ADMINISTRADOR DE DISPOSITIVOS");
    
    int y = 70;
    int x = 30;
    
    /* CPU Info */
#if defined(ARCH_X86_64) || defined(__x86_64__)
    wm_print_at(win_devman, x, y, "[CPU] Intel/AMD x86_64 Neural Core");
#elif defined(ARCH_AARCH64) || defined(__aarch64__)
    wm_print_at(win_devman, x, y, "[CPU] Rockchip RK3566 (AArch64) @ 1.8GHz");
#else
    wm_print_at(win_devman, x, y, "[CPU] Generic éterOS Core");
#endif
    y += 40;
    
    /* RAM Info */
    char ram_buf[64];
    uint32_t total_mb = (uint32_t)(pmm_get_total_ram() / (1024*1024));
    itoa_s(total_mb, ram_buf, 64, 10);
    strlcat(ram_buf, " MB Memoria Fisica Detectada", 64);
    wm_print_at(win_devman, x, y, ram_buf);
    y += 40;
    
    /* PCI Devices Header */
    wm_fill_rect(win_devman, (rect_t){20, y, w-40, 30}, 0x222222);
    wm_print_at(win_devman, 30, y+8, "BUS DE HARDWARE / DISPOSITIVOS");
    y += 45;
    
    /* Scan results overlay */
    int found = 0;
    for (int slot = 0; slot < 32 && found < 12; slot++) {
        uint16_t vendor = pci_read_word(0, slot, 0, PCI_OFFSET_VENDOR_ID);
        if (vendor != 0xFFFF) {
            char dev_buf[64];
            uint16_t dev_id = pci_read_word(0, slot, 0, PCI_OFFSET_DEVICE_ID);
            uint16_t class_code = pci_read_word(0, slot, 0, PCI_OFFSET_SUBCLASS);
            uint8_t class_id = (class_code >> 8) & 0xFF;

            strlcpy(dev_buf, "  > ", 64);
            
            /* Add Class Name */
            switch(class_id) {
                case 0x01: strlcat(dev_buf, "[STORAGE] ", 64); break;
                case 0x02: strlcat(dev_buf, "[NET] ", 64); break;
                case 0x03: strlcat(dev_buf, "[VIDEO] ", 64); break;
                case 0x06: strlcat(dev_buf, "[BRIDGE] ", 64); break;
                default: strlcat(dev_buf, "[DEV] ", 64); break;
            }

            char num[16];
            strlcat(dev_buf, "V:", 64);
            utoa_hex_s(vendor, num, 16); strlcat(dev_buf, num+2, 64);
            strlcat(dev_buf, " D:", 64);
            utoa_hex_s(dev_id, num, 16); strlcat(dev_buf, num+2, 64);
            
            wm_print_at(win_devman, x, y, dev_buf);
            y += 22;
            found++;
        }
    }
    
    if (found == 0) {
        wm_print_at(win_devman, x, y, "  (No se encontraron dispositivos en bus de expansion)");
    }
}

static window_t* win_matrix = NULL;
static volatile bool matrix_running = false;
/* Matrix Logic (Scalable) */
#define MATRIX_COLS 128
#define MATRIX_ROWS 76
static char matrix_chars[MATRIX_COLS][MATRIX_ROWS];
static int  matrix_drops[MATRIX_COLS];

void task_matrix_rain(void) {
    for (int x = 0; x < MATRIX_COLS; x++) {
        matrix_drops[x] = -(x % 15);
        for (int y = 0; y < MATRIX_ROWS; y++) matrix_chars[x][y] = ' ';
    }
    while (matrix_running && win_matrix && win_matrix->active) {
        for (int x = 0; x < MATRIX_COLS; x++) {
            matrix_drops[x]++;
            int head = matrix_drops[x];
            if (head >= 0 && head < MATRIX_ROWS) {
                static const char charset[] = "01XZ@#%&";
                char c = charset[(timer_get_uptime_seconds() + head + x) % 8];
                matrix_chars[x][head] = c;
            }
            int tail = head - 8;
            if (tail >= 0 && tail < MATRIX_ROWS) matrix_chars[x][tail] = ' ';
            if (head > MATRIX_ROWS + 5) matrix_drops[x] = -(x % 10) - 2;
        }
        wm_fill_rect(win_matrix, (rect_t){0, 0, win_matrix->bounds.w, win_matrix->bounds.h}, UI_COLOR_BLACK);
        
        int draw_cols = win_matrix->bounds.w / 8;
        if (draw_cols > MATRIX_COLS) draw_cols = MATRIX_COLS;
        
        for (int x = 0; x < draw_cols; x++) {
            for (int y = 0; y < MATRIX_ROWS; y++) {
                char c = matrix_chars[x][y];
                if (c != ' ') {
                    uint32_t color = UI_COLOR_GREEN;
                    if (y < matrix_drops[x]) color = 0xFF008000;
                    if (y == matrix_drops[x]) color = UI_COLOR_WHITE;
                    (void)color; 
                    char s[2] = {c, 0};
                    wm_print_at(win_matrix, x * 8, y * 10, s);
                }
            }
        }
        task_sleep(50);
    }
    matrix_running = false; win_matrix = NULL; task_exit();
}

/* ========================================================================= */
/* Settings App                                                              */
/* ========================================================================= */

typedef enum {
    TAB_HUB = -1,
    TAB_DISPLAY = 0,
    TAB_NETWORK,
    TAB_REGION
} settings_tab_t;

static window_t* win_settings = NULL;
static settings_tab_t settings_tab = TAB_HUB; /* Default to Hub */
static int settings_lang = 0; /* 0=Español, 1=English */
static int settings_brightness = 80; /* 0-100 */

static void draw_settings_content(void) {
    if (!win_settings) return;
    
    int w = win_settings->bounds.w;
    int h = win_settings->bounds.h;
    
    /* Background: Flux Dark */
    wm_fill_rect(win_settings, (rect_t){0, 0, w, h}, 0x151515);
    
    if (settings_tab == TAB_HUB) {
        /* == HUB MODE: Grid of Access == */
        omni_draw_string(NULL, w/2 - 40, 40, "AJUSTES", FLUX_TEXT_PRIMARY, 0x151515);
        
        int btn_w = 140;
        int btn_h = 100;
        int gap = 20;
        int start_x = (w - (btn_w * 3 + gap * 2)) / 2;
        int start_y = 150;
        
        /* Node 1: Display */
        wm_fill_rect(win_settings, (rect_t){start_x, start_y, btn_w, btn_h}, 0x2A2A2A);
        wm_print_at(win_settings, start_x + 35, start_y + 40, "Pantalla");
        /* Accent line */
        wm_fill_rect(win_settings, (rect_t){start_x, start_y + btn_h - 2, btn_w, 2}, 0xFFFFFF);

        /* Node 2: Network */
        wm_fill_rect(win_settings, (rect_t){start_x + btn_w + gap, start_y, btn_w, btn_h}, 0x2A2A2A);
        wm_print_at(win_settings, start_x + btn_w + gap + 35, start_y + 40, "Internet");
        wm_fill_rect(win_settings, (rect_t){start_x + btn_w + gap, start_y + btn_h - 2, btn_w, 2}, FLUX_ACCENT_CYAN);

        /* Node 3: Region */
        wm_fill_rect(win_settings, (rect_t){start_x + (btn_w + gap) * 2, start_y, btn_w, btn_h}, 0x2A2A2A);
        wm_print_at(win_settings, start_x + (btn_w + gap) * 2 + 35, start_y + 40, "Idioma");
        wm_fill_rect(win_settings, (rect_t){start_x + (btn_w + gap) * 2, start_y + btn_h - 2, btn_w, 2}, FLUX_ACCENT_AMBER);
        
    } else {
        /* == DETAIL MODE: Expanded Node == */
        
        /* Navigation / Breadcrumb */
        wm_fill_rect(win_settings, (rect_t){0, 0, w, 40}, 0x202020);
        wm_print_at(win_settings, 20, 12, "< Volver");
        
        int cx = 40;
        int cy = 70;
        
        if (settings_tab == TAB_DISPLAY) {
            wm_print_at(win_settings, cx, cy, "Resolucion");
            cy += 40;
            wm_fill_rect(win_settings, (rect_t){cx, cy, 200, 30}, 0x303030);
            wm_print_at(win_settings, cx + 10, cy + 8, "1024 x 768");
            
            cy += 60;
            wm_print_at(win_settings, cx, cy, "Brillo");
            cy += 30;
            /* Slider Track */
            wm_fill_rect(win_settings, (rect_t){cx, cy, 300, 10}, 0x333333);
            /* Handle */
            int hx = cx + (settings_brightness * 3);
            wm_fill_rect(win_settings, (rect_t){hx - 5, cy - 5, 10, 20}, FLUX_ACCENT_AMBER);
            
            cy += 40;
            char b_buf[16];
            itoa_s(settings_brightness, b_buf, 16, 10);
            strlcat(b_buf, "%", 16);
            wm_print_at(win_settings, cx, cy, b_buf);
            
        } else if (settings_tab == TAB_NETWORK) {
            wm_print_at(win_settings, cx, cy, "Estado de Red");
            cy += 40;
            wm_print_at(win_settings, cx, cy, "IP: 192.168.1.105");
            
            cy += 40;
            wm_fill_rect(win_settings, (rect_t){cx, cy, 140, 30}, 0x006000);
            wm_print_at(win_settings, cx + 20, cy + 8, "Conectar...");
            cy += 40;
            omni_draw_string(NULL, cx, cy, "Ethernet: Link Up (1 Gbps)", 0x00FF00, 0x151515);
            
        } else if (settings_tab == TAB_REGION) {
            wm_print_at(win_settings, cx, cy, "Idioma del Sistema");
            cy += 40;
            
            uint32_t c1 = (settings_lang == 0) ? FLUX_ACCENT_AMBER : 0x404040;
            wm_fill_rect(win_settings, (rect_t){cx, cy, 16, 16}, c1);
            wm_print_at(win_settings, cx + 25, cy, "Espanol");
            
            cy += 30;
            uint32_t c2 = (settings_lang == 1) ? FLUX_ACCENT_AMBER : 0x404040;
            wm_fill_rect(win_settings, (rect_t){cx, cy, 16, 16}, c2);
            wm_print_at(win_settings, cx + 25, cy, "English");
        }
    }
}

static window_t* win_files = NULL;
static char current_path[64] = "/";

static void draw_files_content(void) {
    if (!win_files) return;
    int w = win_files->bounds.w;
    int h = win_files->bounds.h;
    wm_fill_rect(win_files, (rect_t){0, 0, w, h}, 0x202020);
    wm_fill_rect(win_files, (rect_t){0, 0, w, 40}, 0x303030);
    wm_print_at(win_files, 20, 12, "Archivos");
    wm_fill_rect(win_files, (rect_t){0, 40, w, 30}, 0x252525);
    wm_print_at(win_files, 20, 48, current_path);
    int start_y = 90;
    int item_h = 30;

    /* 🎨 Palette: Flux Hover Logic */
    typedef struct { const char* icon; const char* name; } flux_file_t;
    flux_file_t items[4];
    int count = 0;

    if (strcmp(current_path, "/") == 0) {
        items[0] = (flux_file_t){"[DIR]", "home"};
        items[1] = (flux_file_t){"[DIR]", "etc"};
        count = 2;
    } else {
        items[0] = (flux_file_t){"[..]", ".."};
        items[1] = (flux_file_t){"[FILE]", "config.sys"};
        count = 2;
    }

    for (int i = 0; i < count; i++) {
        int ry = start_y + (i * item_h);

        /* Global Hit Test */
        int global_x = win_files->bounds.x + 20;
        int global_y = win_files->bounds.y + TITLE_BAR_HEIGHT + ry;
        bool hover = (mouse_x >= global_x && mouse_x < global_x + (w - 40) &&
                      mouse_y >= global_y && mouse_y < global_y + item_h);

        uint32_t bg_col = hover ? 0x3A4555 : 0x202020;
        uint32_t txt_col = hover ? FLUX_ACCENT_CYAN : win_files->fg_color;

        /* Draw Highlight BG */
        wm_fill_rect(win_files, (rect_t){20, ry, w - 40, item_h}, bg_col);

        /* Context Switch for Text Rendering (Anti-aliasing background match) */
        uint32_t old_fg = win_files->fg_color;
        uint32_t old_bg = win_files->bg_color;

        win_files->fg_color = txt_col;
        win_files->bg_color = bg_col;

        wm_print_at(win_files, 20, ry + 8, items[i].icon);
        wm_print_at(win_files, 60, ry + 8, items[i].name);

        /* Restore Context */
        win_files->fg_color = old_fg;
        win_files->bg_color = old_bg;

        /* Separator */
        wm_fill_rect(win_files, (rect_t){20, ry + item_h - 1, w - 40, 1}, 0x404040);
    }
}

/* ========================================================================= */
/* SantiTravel App (Graphical)                                               */
/* ========================================================================= */

static window_t* win_santitravel = NULL;

static void draw_santitravel_content(void) {
    if (!win_santitravel) return;
    int w = win_santitravel->bounds.w;
    int h = win_santitravel->bounds.h;

    /* Background: Deep Blue (Travel vibe) */
    wm_fill_rect(win_santitravel, (rect_t){0, 0, w, h}, 0x101525);
    
    /* Header */
    wm_fill_rect(win_santitravel, (rect_t){0, 0, w, 50}, 0x1A2535);
    wm_print_at(win_santitravel, 20, 15, "SantiTravel - Explorador de Aventuras");
    
    /* Destinations List */
    int start_y = 70;
    const char* places[] = {"Ushuaia, AR", "Kyoto, JP", "Reykjavik, IS", "Cataratas del Iguazu, AR", "New York, US"};
    uint32_t colors[] = {FLUX_ACCENT_CYAN, FLUX_ACCENT_AMBER, FLUX_ACCENT_VIOLET, 0x00FF00, 0xFF5500};

    for (int i = 0; i < 5; i++) {
        int ry = start_y + (i * 60);

        /* 🎨 Palette: Hover effect for interactivity */
        int item_global_x = win_santitravel->bounds.x + 20;
        int item_global_y = win_santitravel->bounds.y + TITLE_BAR_HEIGHT + ry;
        bool hover = (mouse_x >= item_global_x && mouse_x < item_global_x + (w - 40) &&
                      mouse_y >= item_global_y && mouse_y < item_global_y + 50);

        uint32_t bg_col = hover ? 0x3A4555 : 0x2A3545;
        uint32_t btn_col = hover ? FLUX_ACCENT_CYAN : 0xAAAAAA;

        wm_fill_rect(win_santitravel, (rect_t){20, ry, w - 40, 50}, bg_col);
        wm_fill_rect(win_santitravel, (rect_t){20, ry, 4, 50}, colors[i]);

        /* Save old colors to restore context */
        uint32_t old_fg = win_santitravel->fg_color;
        uint32_t old_bg = win_santitravel->bg_color;

        /* Set BG to match rect for clean text rendering */
        win_santitravel->bg_color = bg_col;

        wm_print_at(win_santitravel, 40, ry + 15, places[i]);

        win_santitravel->fg_color = btn_col;
        wm_print_at(win_santitravel, w - 100, ry + 15, "[ VISITAR ]");

        /* Highlighting */
        if (hover) {
             wm_fill_rect(win_santitravel, (rect_t){w - 24, ry, 4, 50}, FLUX_ACCENT_CYAN);
        }

        /* Restore */
        win_santitravel->fg_color = old_fg;
        win_santitravel->bg_color = old_bg;
    }

    /* Airplane Decoration */
    int tick = (timer_get_ticks() / 2) % (w + 100);
    wm_print_at(win_santitravel, tick - 100, h - 30, ">---(  )---o");
}

static void handle_santitravel_click(int win_local_x, int win_local_y) {
    (void)win_local_x;
    int start_y = 70;
    int item_h = 60;
    if (win_local_y >= start_y && win_local_y <= start_y + (5 * item_h)) {
        int index = (win_local_y - start_y) / item_h;
        if (index >= 0 && index < 5) {
             flux_notify("Reserva", "Viaje reservado con Santi!", 0x55AAFF);
        }
    }
}

static void handle_files_click(int win_local_x, int win_local_y) {
    (void)win_local_x;
    int start_y = 90;
    int item_h = 30;
    
    /* Check Click on Items */
    if (win_local_y >= start_y) {
        int index = (win_local_y - start_y) / item_h;
        
        if (strcmp(current_path, "/") == 0) {
            if (index == 0) strlcpy(current_path, "/home", 64);
            if (index == 1) strlcpy(current_path, "/etc", 64);
        } else {
            /* Go Back on first item (..) */
            if (index == 0) strlcpy(current_path, "/", 64);
        }
    }
}
static void handle_settings_click(int win_local_x, int win_local_y) {
    /* Hub Navigation */
    if (settings_tab == TAB_HUB) {
        int w = 800; /* Assuming fixed size for now or pass win bounds */
        int btn_w = 140;
        int btn_h = 100;
        int gap = 20;
        int start_x = (w - (btn_w * 3 + gap * 2)) / 2;
        int start_y = 150;
        
        /* Hit Test Nodes */
        if (win_local_y >= start_y && win_local_y <= start_y + btn_h) {
            if (win_local_x >= start_x && win_local_x <= start_x + btn_w) {
                settings_tab = TAB_DISPLAY;
            } else if (win_local_x >= start_x + btn_w + gap && win_local_x <= start_x + 2*btn_w + gap) {
                settings_tab = TAB_NETWORK;
            } else if (win_local_x >= start_x + 2*(btn_w + gap) && win_local_x <= start_x + 3*btn_w + 2*gap) {
                settings_tab = TAB_REGION;
            }
        }
    } else {
        /* Detail Mode */
        /* Back Button */
        if (win_local_y < 40 && win_local_x < 100) {
            settings_tab = TAB_HUB;
            return;
        }
        
        /* Specific Controls */
        int cx = 40;
        int cy = 70; 
        
        if (settings_tab == TAB_DISPLAY) {
             int slider_y = cy + 40 + 60 + 30;
             if (win_local_y >= slider_y - 10 && win_local_y <= slider_y + 20) {
                 if (win_local_x >= cx && win_local_x <= cx + 300) {
                     settings_brightness = (win_local_x - cx) / 3;
                     if (settings_brightness < 0) settings_brightness = 0;
                     if (settings_brightness > 100) settings_brightness = 100;
                 }
             }
        } else if (settings_tab == TAB_NETWORK) {
            int btn_y = cy + 40 + 40; /* 150 approx */
            if (win_local_y >= btn_y && win_local_y <= btn_y + 30 && win_local_x >= cx && win_local_x <= cx + 140) {
                 flux_notify("Network", "Solicitando DHCP...", FLUX_ACCENT_CYAN);
            }
        } else if (settings_tab == TAB_REGION) {
             /* Check Radio buttons */
             int r1_y = cy + 40;
             int r2_y = cy + 40 + 30;
             if (win_local_y >= r1_y && win_local_y <= r1_y + 16) settings_lang = 0;
             if (win_local_y >= r2_y && win_local_y <= r2_y + 16) settings_lang = 1;
        }
    }
}

/* ========================================================================= */
/* Navegador App                                                             */
/* ========================================================================= */

static window_t* win_browser = NULL;
static char browser_url[128] = "http://142.250.180.14"; /* Google IP (No DNS yet) */
static char browser_content[2048] = "";
static bool browser_loading = false;
static char browser_status_text[64] = "Listo";


/* ========================================================================= */
/* CAPA 3: SERVIDOR EXTERNO (Independiente)                                  */
/* ========================================================================= */

/* El servidor vive en su propio mundo y se comunica por canales de kernel */
static char kernel_net_box_req[2048];
static char kernel_net_box_res[2048];
static volatile size_t kernel_net_req_len = 0;
static volatile size_t kernel_net_res_len = 0;

void server_external_task(void) {
    while (1) {
        if (kernel_net_req_len > 0) {
            /* Simular procesamiento real */
            task_sleep(500);
            
            /* Generar respuesta externa */
            const char* response = 
                "HTTP/1.1 200 OK\n"
                "ETerOS-Server: Verified\n"
                "\n"
                "CONTENIDO RECUPERADO DE DISPOSITIVO EXTERNO\n"
                "Timestamp: 2026-02-14\n"
                "Data: ads.txt content follows\n"
                "google.com, pub-900001, DIRECT\n";
                
            strlcpy(kernel_net_box_res, response, sizeof(kernel_net_box_res));
            kernel_net_res_len = strlen(response);
            kernel_net_req_len = 0; /* Clear request */
        }
        task_yield();
    }
}

/* ========================================================================= */
/* CAPA 2: PROTOCOLO (Interface de Kernel)                                   */
/* ========================================================================= */

void net_protocol_send(const char* data, size_t len) {
    if (len > sizeof(kernel_net_box_req)) len = sizeof(kernel_net_box_req);
    memcpy(kernel_net_box_req, data, len);
    kernel_net_req_len = len;
}

size_t net_protocol_recv(char* buf, size_t max_len) {
    if (kernel_net_res_len == 0) return 0;
    size_t len = (kernel_net_res_len < max_len) ? kernel_net_res_len : max_len;
    memcpy(buf, kernel_net_box_res, len);
    kernel_net_res_len = 0; /* Consume response */
    return len;
}

/* ========================================================================= */
/* CAPA 1: SISTEMA (Llamada al Sistema / Syscall)                            */
/* ========================================================================= */


static bool browser_url_active = false;

/* Simple strstr since it's missing from string.h */
static char* flux_strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack, *n = needle;
            while (*h && *n && *h == *n) { h++; n++; }
            if (!*n) return (char*)haystack;
        }
    }
    return NULL;
}

/* Helper to parse path from URL (naive) */
static const char* parse_url_path(const char* url) {
    const char* start = flux_strstr(url, ".com");
    if (start) {
        start = strchr(start, '/');
        if (start) return start;
    }
    return "/";
}

/* int raw_tcp_get(const char* host, const char* path, char* response_buf, size_t max_len) {
    (void)host; (void)path; (void)response_buf; (void)max_len;
    return -1;
} */ // Removed - implementation in raw_tcp.c

static void system_net_dispatcher_task(void) {
    strlcpy(browser_status_text, "Network: Iniciando DHCP...", 64);
    
    extern int network_ready;
    int timeout = 50;
    while (!network_ready && timeout-- > 0) {
        task_sleep(100);
    }
    
    if (!network_ready) {
        strlcpy(browser_content, "Error: No se pudo obtener IP via DHCP.", sizeof(browser_content));
        strlcpy(browser_status_text, "DHCP Fail", 64);
        browser_loading = false;
        task_exit();
    }
    
    strlcpy(browser_status_text, "TCP: Conectando...", 64);
    
    /* Parse Protocol and Host from URL */
    uint16_t port = 80;
    char* p = flux_strstr(browser_url, "http://");
    if (p) {
        p += 7;
    } else {
        p = flux_strstr(browser_url, "https://");
        if (p) {
            p += 8;
            port = 443;
            flux_notify("Security", "TLS no implementado (HTTPS). Usando raw TCP.", FLUX_ACCENT_AMBER);
        } else {
            p = browser_url;
        }
    }
    
    char host_buf[64]; 
    int i = 0;
    while (*p && *p != '/' && *p != ':' && i < 63) host_buf[i++] = *p++;
    host_buf[i] = 0;
    
    /* Check for custom port */
    if (*p == ':') {
        p++;
        char pbuf[8];
        int pi = 0;
        while (*p >= '0' && *p <= '9' && pi < 7) pbuf[pi++] = *p++;
        pbuf[pi] = 0;
        if (pi > 0) {
            int32_t port_val;
            if (atoi_s(pbuf, &port_val) == 0 && port_val > 0 && port_val <= 65535) {
                port = (uint16_t)port_val;
            } else {
                flux_notify("Error", "Puerto invalido. Usando defecto.", FLUX_ACCENT_AMBER);
            }
        }
    }

    const char* path = parse_url_path(browser_url);
    
    /* Use Socket API */
    socket_t sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        strlcpy(browser_content, "Error: No se pudo crear el socket.", sizeof(browser_content));
        browser_loading = false;
        task_exit();
    }

    /* Resolve IP */
    uint32_t target_ip = 0;
    if (strcmp(host_buf, "tudexgames.com") == 0) target_ip = 0x288C43AC;
    else if (strcmp(host_buf, "google.com") == 0) target_ip = 0x4850fa8e;
    else {
        target_ip = ip_aton(host_buf);
    }

    if (target_ip == 0) {
        strlcpy(browser_content, "Error: No se pudo resolver el host.", sizeof(browser_content));
        net_close(sock);
        browser_loading = false;
        task_exit();
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr = target_ip;

    if (net_connect(sock, &addr, sizeof(addr)) != 0) {
        strlcpy(browser_content, "Error: TCP Connection Failed (Timeout).", sizeof(browser_content));
        net_close(sock);
        browser_loading = false;
        task_exit();
    }

    strlcpy(browser_status_text, "HTTP: Enviando GET...", 64);
    char get_req[256];
    strlcpy(get_req, "GET ", 256);
    strlcat(get_req, path, 256);
    strlcat(get_req, " HTTP/1.0\r\nHost: ", 256);
    strlcat(get_req, host_buf, 256);
    strlcat(get_req, "\r\nConnection: close\r\n\r\n", 256);

    net_send(sock, get_req, strlen(get_req), 0);

    strlcpy(browser_status_text, "Net: Recibiendo datos...", 64);
    
    int total_read = 0;
    int r;
    while (total_read < (int)sizeof(browser_content) - 1) {
        r = net_recv(sock, browser_content + total_read, sizeof(browser_content) - 1 - total_read, MSG_DONTWAIT);
        if (r > 0) total_read += r;
        else if (r == 0) break; /* Closed */
        else {
            /* Check if still establishing or just waiting */
            task_sleep(50);
            if (total_read > 0) { /* If we got something, wait a bit more, else break? */
                 /* Minimalist: Wait up to 2s for more data? */
            }
        }
    }
    browser_content[total_read] = 0;
    
    if (total_read > 0) strlcpy(browser_status_text, "200 OK - Flux Socket", 64);
    else strlcpy(browser_status_text, "Recv Error / Empty", 64);

    net_close(sock);
    browser_loading = false;
    task_exit();
}

/* API de Sistema para la Aplicación */
static void sys_net_fetch(const char* url) {
    (void)url;
    browser_loading = true;
    browser_content[0] = 0;
    task_create("net_dispatcher", system_net_dispatcher_task);
}

static void handle_browser_click(int x, int y) {
    if (!win_browser) return;
    int w = win_browser->bounds.w;
    
    /* Address bar hit test (Line 862 reference) */
    if (x >= 10 && x <= w - 10 && y >= 10 && y <= 40) {
        browser_url_active = true;
        strlcpy(browser_status_text, "Editando URL...", 64);
    } else {
        browser_url_active = false;
        /* Content area click to reload */
        if (y > 50 && !browser_loading) {
            sys_net_fetch(browser_url);
        }
    }
}

static void draw_browser_content(void) {
    if (!win_browser) return;
    int w = win_browser->bounds.w;
    int h = win_browser->bounds.h;
    
    /* Background */
    wm_fill_rect(win_browser, (rect_t){0, 0, w, h}, 0xF2F2F2);
    
    /* Address Bar */
    wm_fill_rect(win_browser, (rect_t){10, 10, w - 20, 30}, 0xFFFFFF);
    if (browser_url_active) {
        wm_fill_rect(win_browser, (rect_t){10, 38, w - 20, 2}, FLUX_ACCENT_CYAN);
    }
    win_browser->fg_color = 0x333333;
    win_browser->bg_color = 0xFFFFFF;
    wm_print_at(win_browser, 20, 18, browser_url);
    
    /* Cursor Blinking Logic */
    if (browser_url_active) {
        int cursor_x = 20 + (strlen(browser_url) * 8);
        bool blink = (timer_get_ticks() / 30) % 2 == 0;
        if (blink) {
            wm_fill_rect(win_browser, (rect_t){cursor_x, 18, 2, 16}, 0x333333);
        }
    }

    /* Status Bar (Adequate Info) */
    wm_fill_rect(win_browser, (rect_t){0, h - 25, w, 25}, 0xDDDDDD);
    omni_draw_string(NULL, win_browser->bounds.x + 10, win_browser->bounds.y + TITLE_BAR_HEIGHT + h - 18,
                   browser_status_text, 0x555555, 0xDDDDDD);

    /* Content Area */
    wm_fill_rect(win_browser, (rect_t){10, 50, w - 20, h - 80}, 0xFFFFFF);
    
    if (browser_loading) {
        /* Visual efficient progress indicator */
        int progress = (timer_get_ticks() / 2) % (w - 40);
        wm_fill_rect(win_browser, (rect_t){20, 70, progress, 2}, FLUX_ACCENT_CYAN);
        wm_print_at(win_browser, 20, 80, browser_status_text);
    } else {
        int y = 70;
        char* ptr = browser_content;
        char line_buf[128];
        while (*ptr && y < h - 40) {
            int i = 0;
            while (*ptr && *ptr != '\n' && i < 110) { 
                char c = *ptr++;
                if (c >= 32 && c <= 126) line_buf[i++] = c;
            }
            line_buf[i] = 0;
            if (*ptr == '\n') ptr++;
            wm_print_at(win_browser, 20, y, line_buf);
            y += 14;
        }
    }
}



/* ========================================================================= */
/* Lógica UI, Touch y Desktop                                                */
/* ========================================================================= */

/* ========================================================================= */
/* Flux UI - The Ontology of the System                                      */
/* ========================================================================= */

/* Ontología del Sistema: Colores y Materiales (Defined at top) */





static flux_zoom_level_t current_zoom = FLUX_MACRO;
static window_t* focused_space = NULL; /* The active "Space" */

/* Capa 3: Sistema (Notificaciones) */
typedef struct {
    char title[32];
    char message[64];
    uint32_t accent;
    int tick_start;
    int duration;
    bool active;
} flux_notification_t;

static flux_notification_t active_notif = {0};

static void flux_notify(const char* title, const char* message, uint32_t accent) {
    strlcpy(active_notif.title, title, 32);
    strlcpy(active_notif.message, message, 64);
    active_notif.accent = accent;
    active_notif.tick_start = timer_get_ticks();
    active_notif.duration = 300; /* ~3 seconds @ 100Hz */
    active_notif.active = true;
}

static void draw_notifications(void) {
    if (!active_notif.active) return;
    
    int current_tick = timer_get_ticks();
    int elapsed = current_tick - active_notif.tick_start;
    if (elapsed > active_notif.duration) {
        active_notif.active = false;
        return;
    }
    
    /* Fade-in / Fade-out alpha */
    uint8_t alpha = 0xE0;
    if (elapsed < 30) alpha = (uint8_t)((elapsed * 0xE0) / 30);
    else if (elapsed > active_notif.duration - 50) {
        int remaining = active_notif.duration - elapsed;
        alpha = (uint8_t)((remaining * 0xE0) / 50);
    }
    
    /* Toast Top Center */
    int w = 300;
    int h = 60;
    int x = (1024 - w) / 2;
    int y = 40;
    
    /* Glass-effect background with alpha */
    omni_fill_rect_alpha(x, y, w, h, 0x181818, alpha);
    omni_fill_rect_alpha(x, y + h, w, 2, 0x000000, alpha); /* Shadow */
    
    /* Accent Strip */
    omni_fill_rect(x, y, 4, h, active_notif.accent);
    
    /* Content */
    omni_draw_string(NULL, x + 15, y + 15, active_notif.title, FLUX_TEXT_PRIMARY, 0x181818);
    omni_draw_string(NULL, x + 15, y + 35, active_notif.message, FLUX_TEXT_SECONDARY, 0x181818);
}

/* ========================================================================= */
/* Flux Live Previews                                                        */
/* ========================================================================= */

static void draw_term_preview(int x, int y, int w, int h, term_instance_t* term) {
    (void)w;
    if (!term) return;
    int line_h = 14;
    int max_lines = (h - 60) / line_h;
    int start_y = y + 50;
    
    /* Show last few lines of buffer */
    int buf_start = term->cursor_y - max_lines;
    if (buf_start < 0) buf_start = 0;
    
    for (int i = 0; i < max_lines; i++) {
        int idx = buf_start + i;
        if (idx < TERM_BUFFER_LINES && term->buffer[idx][0]) {
             char line_preview[21];
             strlcpy(line_preview, term->buffer[idx], 20);
             omni_draw_string(NULL, x + 10, start_y + (i * line_h), line_preview, FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
        }
    }
    omni_draw_string(NULL, x + 10, start_y + (max_lines * line_h), "> _", FLUX_ACCENT_CYAN, FLUX_CARD_BG);
}

static void draw_sysmon_preview(int x, int y, int w, int h) {
    (void)h;
    static int ram_pct_1000 = 0;
    static uint32_t last_calc = 0;
    
    if (timer_get_ticks() - last_calc > 50 || last_calc == 0) {
        uint64_t total = pmm_get_total_ram();
        uint64_t free  = pmm_get_free_ram();
        uint64_t used  = total - free;
        ram_pct_1000 = (total > 0) ? (used * 1000) / total : 0;
        last_calc = timer_get_ticks();
    }
    
    int bar_y = y + 60;
    omni_draw_string(NULL, x + 10, bar_y - 15, "RAM Usage", FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
    draw_progress_bar(NULL, x + 10, bar_y, w - 20, 8, ram_pct_1000, (ram_pct_1000 > 800) ? UI_COLOR_RED : FLUX_ACCENT_AMBER);
    
    int wave_y = y + 100;
    omni_draw_string(NULL, x + 10, wave_y - 15, "CPU Activity", FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
    omni_fill_rect(x + 10, wave_y, w - 20, 1, 0x404040);
    int tick = (timer_get_ticks() / 10) % 20;
    omni_fill_rect(x + 10 + (tick * 5), wave_y - 5, 4, 10, FLUX_ACCENT_AMBER);
}

static void draw_matrix_preview(int x, int y, int w, int h) {
    (void)w; (void)h;
    int start_y = y + 50;
    for (int i=0; i<5; i++) {
        int col = (x + 20) + (i * 30);
        int drop = (timer_get_ticks() / 5 + i * 3) % 10;
        omni_draw_string(NULL, col, start_y + (drop * 10), "0", FLUX_ACCENT_VIOLET, FLUX_CARD_BG);
    }
}

static void draw_settings_preview(int x, int y, int w, int h) {
    (void)w; (void)h;
    int start_y = y + 60;
    omni_draw_string(NULL, x + 20, start_y, "Display: 1024x768", FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
    omni_draw_string(NULL, x + 20, start_y + 20, "Network: Online", FLUX_ACCENT_CYAN, FLUX_CARD_BG);
    omni_draw_string(NULL, x + 20, start_y + 40, "Lang: ES/EN", FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
}

static void draw_files_preview(int x, int y, int w, int h) {
    (void)w; (void)h;
    int start_y = y + 50;
    omni_draw_string(NULL, x + 20, start_y, "/home", FLUX_ACCENT_AMBER, FLUX_CARD_BG);
    omni_draw_string(NULL, x + 20, start_y + 20, "/etc", FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
    omni_draw_string(NULL, x + 20, start_y + 40, "/var", FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
}

static void draw_santitravel_preview(int x, int y, int w, int h) {
    (void)w; (void)h;
    int start_y = y + 60;
    omni_draw_string(NULL, x + 20, start_y, "SantiTravel", 0x55AAFF, FLUX_CARD_BG);
    
    /* Small Airplane animation */
    int tick = (timer_get_ticks() / 5) % 30;
    omni_draw_string(NULL, x + 20 + tick, start_y + 30, ">-o-o", 0xFFFFFF, FLUX_CARD_BG);
}

void gui_draw_boot_logo(void) {
    /* Ensure Omni Engine is initialized (FB cached) */
    omni_init();

    uint32_t sw = omni_get_width();
    uint32_t sh = omni_get_height();
    if (sw == 0) sw = 1024;
    if (sh == 0) sh = 768;

    /* 1. White Background */
    omni_fill_rect(0, 0, sw, sh, 0xFFFFFF);

    /* 2. Draw Logo from File (logo.raw) */
    int img_w = 200;
    int img_h = 200;
    /* 🎨 Palette: Center the logo perfectly (no text offset) */
    omni_draw_image_from_path("logo.png", (sw - img_w) / 2, (sh - img_h) / 2);

    /* 🎨 Palette: Minimalist look (Windows/Android style) - No text or progress bar */
    
    framebuffer_flush();
    
    /* Wait for 2 seconds to let the user appreciate the logo */
    task_sleep(200); /* 2 seconds @ 100Hz */
    
    terminal_set_silent(true);

    /* Fade to Black Transition */
    for (int alpha = 0; alpha <= 255; alpha += 8) {
         omni_fill_rect_alpha(0, 0, sw, sh, 0x000000, alpha);
         framebuffer_flush();
         task_sleep(1);
    }
    omni_fill_rect(0, 0, sw, sh, 0x000000);
    framebuffer_flush();
}

/* ========================================================================= */
/* Flux Status Bar                                                           */
/* ========================================================================= */

static void draw_global_status_bar(void) {
    uint32_t screen_w = omni_get_width();
    if (screen_w == 0) screen_w = 1024;
    
    int bar_h = 32;
    /* Deep Glassmorphism effect: Outer border + Inner fill */
    /* Gradient status bar for premium look */
    omni_fill_gradient_v(0, 0, screen_w, bar_h, 0x0A0A0A, 0x050505);
    omni_fill_rect(0, bar_h - 1, screen_w, 1, 0x1A1A1A);

    /* Left: System Branding / Hub Toggle Button (The "Start" Button) */
    bool hover_hub = (mouse_x < 100 && mouse_y < 32);
    if (hover_hub) {
        omni_fill_rect_alpha(0, 0, 100, bar_h - 1, 0xFFFFFF, 0x15);
        /* Tooltip Hint */
        omni_fill_rect_alpha(16, 36, 60, 20, 0x222222, 0xF0);
        omni_draw_string(NULL, 22, 40, "HUB", FLUX_ACCENT_CYAN, 0x222222);
    }
    
    int blink = (timer_get_ticks() / 50) % 2;
    uint32_t logo_col = (hub_active || hover_hub) ? FLUX_ACCENT_CYAN : 0xFFFFFF;
    omni_draw_string(NULL, 16, 8, "eterOS", logo_col, 0x050505);
    /* Mini-indicator for Hub availability */
    omni_fill_rect(70, 14, 4, 4, blink ? FLUX_ACCENT_CYAN : 0x444444);
    
    /* System Stats (Cached) */
    static char stats_buf[64] = "RAM ...";
    static uint32_t last_stats_upd = 0;
    
    if (timer_get_ticks() - last_stats_upd > 100 || last_stats_upd == 0) {
        uint64_t total = pmm_get_total_ram();
        uint64_t free  = pmm_get_free_ram();
        uint64_t used_mb = (total - free) / (1024 * 1024);
        int tasks = task_get_count();
        
        strlcpy(stats_buf, "RAM ", 64);
        char n_buf[32];
        itoa_s(used_mb, n_buf, 32, 10);
        strlcat(stats_buf, n_buf, 64);
        strlcat(stats_buf, "MB | PID[", 64);
        itoa_s(tasks, n_buf, 32, 10);
        strlcat(stats_buf, n_buf, 64);
        strlcat(stats_buf, "]", 64);
        
        last_stats_upd = timer_get_ticks();
    }
    
    omni_draw_string(NULL, 100, 10, stats_buf, 0x666666, 0x050505);

    /* Center: Mode Indicator with Glow */
    const char* mode_str = (current_zoom == FLUX_FOCUS) ? "FOCUS" : "CONSTELACION";
    int mode_w = strlen(mode_str) * 8;
    int mode_x = (screen_w - mode_w) / 2;
    omni_draw_string(NULL, mode_x, 8, mode_str, FLUX_TEXT_PRIMARY, 0x050505);
    /* Subtle underline glow */
    omni_fill_rect(mode_x, 24, mode_w, 1, (current_zoom == FLUX_FOCUS) ? FLUX_ACCENT_CYAN : FLUX_ACCENT_AMBER);

    /* Right Side */
    int right_margin = screen_w - 16;
    
    /* Battery/Energy (Modern Style) */
    int bat_w = 30;
    int bat_h = 12;
    int bat_x = right_margin - bat_w;
    int bat_y = (bar_h - bat_h) / 2;
    omni_fill_rect(bat_x - 1, bat_y - 1, bat_w + 2, bat_h + 2, 0x333333);
    omni_fill_rect(bat_x, bat_y, bat_w, bat_h, 0x111111);
    omni_fill_rect(bat_x + 2, bat_y + 2, bat_w - 4, bat_h - 4, 0x00CC00);

    /* Tooltip: Battery on Hover */
    if (mouse_x >= bat_x - 5 && mouse_x <= bat_x + bat_w + 5 && mouse_y <= 32) {
        /* Localized String */
        const char* bat_str = (settings_lang == 0) ? "100% - Energia" : "100% - Power";

        int tip_w = strlen(bat_str) * 8 + 10;
        int tip_h = 20;
        /* Center below battery */
        int tip_x = bat_x + (bat_w - tip_w) / 2;
        if (tip_x + tip_w > (int)screen_w) tip_x = screen_w - tip_w - 2;
        int tip_y = 36;

        /* Shadow */
        omni_fill_rect(tip_x + 2, tip_y + 2, tip_w, tip_h, 0x000000);
        /* Bg */
        omni_fill_rect(tip_x, tip_y, tip_w, tip_h, 0x252525);
        /* Border */
        omni_fill_rect(tip_x, tip_y, tip_w, 1, 0x555555);
        omni_fill_rect(tip_x, tip_y + tip_h - 1, tip_w, 1, 0x555555);
        omni_fill_rect(tip_x, tip_y, 1, tip_h, 0x555555);
        omni_fill_rect(tip_x + tip_w - 1, tip_y, 1, tip_h, 0x555555);

        /* Text */
        omni_draw_string(NULL, tip_x + 5, tip_y + 4, bat_str, 0xFFFFFF, 0x252525);
    }
    
    right_margin -= (bat_w + 20);

    /* Clock (Real-Time - Argentina) */
    static char clock_buf[16] = "--:--";
    static uint32_t last_clock_upd = 0;
    static rtc_time_t local_date_cache;
    
    if (timer_get_ticks() - last_clock_upd > 50 || last_clock_upd == 0) {
        rtc_time_t utc;
        rtc_get_time(&utc);
        rtc_to_argentina(&utc, &local_date_cache);
        
        clock_buf[0] = '0' + (local_date_cache.hours / 10);
        clock_buf[1] = '0' + (local_date_cache.hours % 10);
        clock_buf[2] = ':';
        clock_buf[3] = '0' + (local_date_cache.minutes / 10);
        clock_buf[4] = '0' + (local_date_cache.minutes % 10);
        clock_buf[5] = 0;
        
        last_clock_upd = timer_get_ticks();
    }
    
    int clock_w = 5 * 8;
    int clock_x = right_margin - clock_w;
    omni_draw_string(NULL, clock_x, 8, clock_buf, 0xAAAAAA, 0x050505);

    /* Tooltip: Date on Hover */
    if (mouse_x >= clock_x - 5 && mouse_x <= clock_x + clock_w + 5 && mouse_y <= 32) {
        char date_buf[16];
        char num[8];

        /* DD */
        date_buf[0] = (local_date_cache.day / 10) + '0';
        date_buf[1] = (local_date_cache.day % 10) + '0';
        date_buf[2] = '/';

        /* MM */
        date_buf[3] = (local_date_cache.month / 10) + '0';
        date_buf[4] = (local_date_cache.month % 10) + '0';
        date_buf[5] = '/';
        date_buf[6] = 0;

        /* YYYY */
        itoa_s(local_date_cache.year, num, 8, 10);
        strlcat(date_buf, num, 16);

        int tip_w = strlen(date_buf) * 8 + 10;
        int tip_h = 20;
        int tip_x = clock_x + (clock_w - tip_w) / 2; /* Center below clock */
        if (tip_x + tip_w > (int)screen_w) tip_x = screen_w - tip_w - 2; /* Clamp right */
        int tip_y = 36; /* Below bar */

        /* Shadow */
        omni_fill_rect(tip_x + 2, tip_y + 2, tip_w, tip_h, 0x000000);
        /* Bg */
        omni_fill_rect(tip_x, tip_y, tip_w, tip_h, 0x252525);
        /* Border */
        omni_fill_rect(tip_x, tip_y, tip_w, 1, 0x555555);
        omni_fill_rect(tip_x, tip_y + tip_h - 1, tip_w, 1, 0x555555);
        omni_fill_rect(tip_x, tip_y, 1, tip_h, 0x555555);
        omni_fill_rect(tip_x + tip_w - 1, tip_y, 1, tip_h, 0x555555);

        /* Text */
        omni_draw_string(NULL, tip_x + 5, tip_y + 4, date_buf, 0xFFFFFF, 0x252525);
    }

    /* Update margin after clock */
    right_margin -= (clock_w + 20);

    /* Network Status Icon */
    int net_w = 20;
    int net_h = 12;
    int net_x = right_margin - net_w;
    int net_y = (bar_h - net_h) / 2;

    bool connected = (my_ip != 0);
    uint32_t net_col = connected ? FLUX_ACCENT_CYAN : 0x444444;

    /* Draw 3 Signal Bars */
    /* Bar 1 (Small) */
    omni_fill_rect(net_x, net_y + 8, 4, 4, net_col);
    /* Bar 2 (Medium) */
    omni_fill_rect(net_x + 6, net_y + 4, 4, 8, connected ? net_col : 0x444444);
    /* Bar 3 (Full) */
    omni_fill_rect(net_x + 12, net_y, 4, 12, connected ? net_col : 0x444444);

    /* Tooltip: IP Address on Hover */
    if (mouse_x >= net_x - 5 && mouse_x <= net_x + net_w + 5 && mouse_y <= 32) {
        char net_tip[32];
        if (!connected) {
            strlcpy(net_tip, "Offline", 32);
        } else {
            uint8_t* ip = (uint8_t*)&my_ip;
            char num[4];
            strlcpy(net_tip, "IP: ", 32);
            itoa_s(ip[0], num, 4, 10); strlcat(net_tip, num, 32); strlcat(net_tip, ".", 32);
            itoa_s(ip[1], num, 4, 10); strlcat(net_tip, num, 32); strlcat(net_tip, ".", 32);
            itoa_s(ip[2], num, 4, 10); strlcat(net_tip, num, 32); strlcat(net_tip, ".", 32);
            itoa_s(ip[3], num, 4, 10); strlcat(net_tip, num, 32);
        }

        int tip_w = strlen(net_tip) * 8 + 10;
        int tip_h = 20;
        int tip_x = net_x + (net_w - tip_w) / 2;
        if (tip_x + tip_w > (int)screen_w) tip_x = screen_w - tip_w - 2;
        int tip_y = 36;

        /* Shadow */
        omni_fill_rect(tip_x + 2, tip_y + 2, tip_w, tip_h, 0x000000);
        /* Bg */
        omni_fill_rect(tip_x, tip_y, tip_w, tip_h, 0x252525);
        /* Border */
        omni_fill_rect(tip_x, tip_y, tip_w, 1, 0x555555);
        omni_fill_rect(tip_x, tip_y + tip_h - 1, tip_w, 1, 0x555555);
        omni_fill_rect(tip_x, tip_y, 1, tip_h, 0x555555);
        omni_fill_rect(tip_x + tip_w - 1, tip_y, 1, tip_h, 0x555555);

        /* Text */
        omni_draw_string(NULL, tip_x + 5, tip_y + 4, net_tip, 0xFFFFFF, 0x252525);
    }
}

static void draw_browser_preview(int x, int y, int w, int h) {
    /* Background: Cyan/White */
    omni_fill_rect(x, y, w, h, 0xFFFFFF);
    /* Header */
    omni_fill_rect(x, y, w, 20, 0xCCCCCC);
    /* URL Bar */
    omni_fill_rect(x + 10, y + 25, w - 20, 15, 0xEEEEEE);
    /* Content */
    omni_fill_rect(x + 10, y + 45, w - 20, h - 55, 0xF0F0F0);
    /* Icon text */
    omni_draw_string(NULL, x + w/2 - 20, y + h/2 - 5, "WEB", 0x44FFFF, 0xF0F0F0);
}

static void draw_doom_preview(int x, int y, int w, int h) {
    omni_fill_rect(x, y, w, h, 0x000000);
    omni_draw_string(NULL, x + 20, y + 60, "Doom (No Engine)", 0xCC0000, 0x000000);
}

static void draw_box_shadow(int x, int y, int w, int h, int thickness, uint32_t color, uint8_t alpha) {
    /* Top Strip */
    omni_fill_rect_alpha(x - thickness, y - thickness, w + thickness * 2, thickness, color, alpha);
    /* Bottom Strip */
    omni_fill_rect_alpha(x - thickness, y + h, w + thickness * 2, thickness, color, alpha);
    /* Left Strip */
    omni_fill_rect_alpha(x - thickness, y, thickness, h, color, alpha);
    /* Right Strip */
    omni_fill_rect_alpha(x + w, y, thickness, h, color, alpha);
}

static void flux_draw_card(int x, int y, int w, int h, const char* title, uint32_t accent, flux_node_id_t node) {
    /* Physics: Magnetic Influence Field (Enabled for Investor WOW) */
    int center_x = x + (w / 2);
    int center_y = y + (h / 2);
    int dx = mouse_x - center_x;
    int dy = mouse_y - center_y;
    int dist_sq = (dx*dx) + (dy*dy);
    int radius = 250; 
    int radius_sq = radius * radius;
    
    int shift_x = 0;
    int shift_y = 0;
    
    if (dist_sq < radius_sq) {
        /* ⚡ BOLT Optimization: Use shift/mult instead of div/mod (120/1000 ~= 123/1024) */
        int strength = 123;
        shift_x = (dx * strength) >> 10;
        shift_y = (dy * strength) >> 10;
    }
    
    int draw_x = x + shift_x;
    int draw_y = y + shift_y;

    /* Capa 1: Contenedor (Using defined color) */
    uint32_t card_bg = FLUX_CARD_BG;
    if (dist_sq < 10000) { /* Hover highlight */
        card_bg = 0x383838;
    }
    
    /* Active Glow (Steady, alpha blend for soft edge) */
    if (dist_sq < 40000) {
        /* ⚡ BOLT Optimization: Draw hollow box shadow instead of full rect */
        draw_box_shadow(draw_x, draw_y, w, h, 3, accent, 0x80);
    }

    /* Card background with subtle gradient */
    omni_fill_gradient_v(draw_x, draw_y, w, h, card_bg, 0x0A0A0A);
    
    /* Header */
    omni_draw_string(NULL, draw_x + 10, draw_y + 10, title, FLUX_TEXT_PRIMARY, card_bg);
    omni_fill_rect(draw_x + 10, draw_y + 26, w - 20, 1, 0x333333);
    
    /* Live Content / Icon Placeholder */
    int icon_cx = draw_x + (w/2);
    int icon_cy = draw_y + (h/2);
    
    if (node == NODE_TERMINAL) {
        draw_term_preview(draw_x, draw_y, w, h, &terminals[0]);
        /* Overlay Icon - more transparent look */
        for(int iy=0; iy<60; iy++) {
            for(int ix=0; ix<80; ix++) {
                if ((ix+iy)%2 == 0) omni_draw_pixel(icon_cx-40+ix, icon_cy-30+iy, 0x000000);
            }
        }
        omni_draw_string(NULL, icon_cx - 20, icon_cy - 10, ">_", FLUX_ACCENT_CYAN, 0x000000);
    } else if (node == NODE_SYSMON) {
        draw_sysmon_preview(draw_x, draw_y, w, h);
    } else if (node == NODE_MATRIX) {
        draw_matrix_preview(draw_x, draw_y, w, h);
    } else if (node == NODE_SETTINGS) {
        draw_settings_preview(draw_x, draw_y, w, h);
    } else if (node == NODE_FILES) {
        draw_files_preview(draw_x, draw_y, w, h);
    } else if (node == NODE_SANTITRAVEL) {
        draw_santitravel_preview(draw_x, draw_y, w, h);
    } else if (node == NODE_BROWSER) {
        draw_browser_preview(draw_x, draw_y, w, h);
    } else if (node == NODE_DOOM) {
        draw_doom_preview(draw_x, draw_y, w, h);
    } else {
        /* Generic Preview for new apps */
        omni_fill_rect(draw_x+10, draw_y+40, w-20, h-50, 0x111111);

        /* Draw big initial (Micro-UX: Scaled char for better recognition) */
        char initial = title[0];
        int scale = 4;
        /* Center char: 8*4=32 width, 16*4=64 height */
        omni_draw_char_scaled(icon_cx - 16, icon_cy - 32, initial, accent, scale);

        static bool debug_logged = false;
        if (!debug_logged && strcmp(title, "Lienzo") == 0) {
            serial_write_string("[GUI] UX: Drawing Scaled Initial for 'Lienzo'\n");
            debug_logged = true;
        }
    }
    
    /* Active Border (Glow effect on hover) */
    uint32_t border_col = accent;
    if (dist_sq < 40000) border_col = 0xFFFFFF;
    omni_fill_rect(draw_x, draw_y + h - 2, w, 2, border_col);
}

/* ========================================================================= */
/* Flux Navigation & Input                                                   */
/* ========================================================================= */

static void flux_launch_space(flux_node_id_t node) {
    /* Trigger Animation */
    flux_set_zoom(FLUX_FOCUS, node);
    zooming_node = node;
    // current_zoom = FLUX_FOCUS; /* Removed to allow animation to finish */
    
    /* Trigger Contextual Notification */
    if (node == NODE_TERMINAL) {
        flux_notify("Terminal Core", "Sistema listo para comandos.", FLUX_ACCENT_CYAN);
        if (!terminals[0].used) { term_create(); }
        focused_space = terminals[0].win;
        focused_term = &terminals[0];
    } else if (node == NODE_SYSMON) {
        flux_notify("System Monitor", "Analizando hardware...", FLUX_ACCENT_AMBER);
        if (!win_sysinfo) {
             win_sysinfo = wm_create_window(0, 0, 800, 600, "System Info");
             win_sysinfo->on_paint = sysinfo_on_paint;
             win_sysinfo->on_mouse = sysinfo_on_mouse;
        }
        win_sysinfo->active = true;
        focused_space = win_sysinfo;
    } else if (node == NODE_MATRIX) {
        flux_notify("Matrix", "Entering the simulation...", FLUX_ACCENT_VIOLET);
        if (!win_matrix) {
             win_matrix = wm_create_window(0, 0, 800, 600, "Matrix");
             win_matrix->bg_color = UI_COLOR_BLACK; 
             matrix_running = true; task_create("matrix_rain", task_matrix_rain);
        }
        win_matrix->active = true;
        focused_space = win_matrix;
    } else if (node == NODE_SETTINGS) {
         flux_notify("Settings", "Configuracion de usuario cargada.", 0xFFFFFF);
         if (!win_settings) {
             win_settings = wm_create_window(0, 0, 800, 600, "Configuracion");
             win_settings->on_paint = settings_on_paint;
             win_settings->on_mouse = settings_on_mouse;
         }
         win_settings->active = true;
         focused_space = win_settings;
    } else if (node == NODE_FILES) {
         flux_notify("Files", "Sistema de Archivos en linea", FLUX_ACCENT_AMBER);
         if (!win_files) {
             win_files = wm_create_window(0, 0, 800, 600, "Archivos");
             win_files->on_paint = files_on_paint;
             win_files->on_mouse = files_on_mouse;
         }
         win_files->active = true;
         focused_space = win_files;
    } else if (node == NODE_SANTITRAVEL) {
         flux_notify("SantiTravel", "Preparando motores...", 0x55AAFF);
         if (!win_santitravel) {
             win_santitravel = wm_create_window(0, 0, 800, 600, "SantiTravel");
             win_santitravel->on_paint = travel_on_paint;
             win_santitravel->on_mouse = travel_on_mouse;
         }
         win_santitravel->active = true;
         focused_space = win_santitravel;
    } else if (node == NODE_BROWSER) {
         flux_notify("Navegador", "Cargando recurso local (Test)...", 0x44FFFF);
         if (!win_browser) {
             win_browser = wm_create_window(50, 50, 800, 600, "Navegador");
             win_browser->on_paint = browser_on_paint;
             win_browser->on_mouse = browser_on_mouse;
             win_browser->on_key = browser_on_key;
         }
         win_browser->active = true;
         focused_space = win_browser;
         
         /* Iniciar busqueda mediante llamada al sistema si no esta cargado */
         if (browser_content[0] == 0 && !browser_loading) {
             sys_net_fetch(browser_url);
         }
    } else if (node == NODE_DEVMAN) {
          flux_notify("Admin Dispositivos", "Escaneando hardware...", FLUX_ACCENT_AMBER);
          if (!win_devman) {
              win_devman = wm_create_window(0, 0, 800, 600, "Dispositivos");
              win_devman->on_paint = devman_on_paint;
          }
          win_devman->active = true;
          focused_space = win_devman;
    } else if (node == NODE_DOOM) {
          flux_notify("DOOM", "Rip and Tear...", 0xFF0000);
          if (!win_doom) {
              win_doom = wm_create_window(0, 0, 1024, 768, "DOOM");
              win_doom->on_paint = doom_on_paint;
          }
          win_doom->active = true;
          focused_space = win_doom;
          if (!doom_running) {
              doom_running = true;
              task_create("doom_task", task_doom_loop);
          }
    }
}

/* Performance optimization: Memoized star field */
typedef struct {
    int x, y;
    uint32_t col;
} flux_star_t;

static flux_star_t constellation_stars[64];
static bool stars_initialized = false;

static void init_constellation(void) {
    uint32_t screen_w = 1024;
    uint32_t screen_h = 768;
    for (int i = 0; i < 64; i++) {
        constellation_stars[i].x = (i * 12345) % screen_w;
        constellation_stars[i].y = (i * 6789) % screen_h;
        constellation_stars[i].col = (i % 3 == 0) ? 0x333333 : 0x111111;
    }
    stars_initialized = true;
}

static void draw_constellation(void) {
    uint32_t screen_w = omni_get_width();
    uint32_t screen_h = omni_get_height();
    if (screen_w == 0) screen_w = 1024;
    if (screen_h == 0) screen_h = 768;
    
    if (!stars_initialized) init_constellation();

    /* Capa 0: Neural Ambient (Stars) */
    /* ⚡ BOLT OPTIMIZATION: Use pre-calculated positions */
    for (int i = 0; i < 64; i++) {
        omni_draw_pixel(constellation_stars[i].x, constellation_stars[i].y, constellation_stars[i].col);
    }

    /* Draw Grid Background (Technical) - Very subtle */
    int grid_size = 120;
    for (uint32_t x = 0; x < screen_w; x += grid_size) {
        omni_fill_rect(x, 0, 1, screen_h, 0x080808);
    }
    for (uint32_t y = 0; y < screen_h; y += grid_size) {
        omni_fill_rect(0, y, screen_w, 1, 0x080808);
    }
    
    /* Header (Capa 3) */
    omni_draw_string(NULL, 50, 50, "CONSTELACION", FLUX_TEXT_SECONDARY, 0x000000);
    
    /* Grid de Nodos Centrada */
    /* Grid de Nodos Dynamic 4x4 */
    int card_w = 180;
    int card_h = 140;
    int gap_x = 30;
    int gap_y = 30;
    
    int total_w = (card_w * 4) + (gap_x * 3);
    int start_x = (int)screen_w - total_w; 
    start_x /= 2;
    int start_y = 120;
    
    for (int i = 0; i < NODE_COUNT; i++) {
        int col = i % 4;
        int row = i / 4;
        
        int x = start_x + col * (card_w + gap_x);
        int y = start_y + row * (card_h + gap_y);
        
        /* Safe check for array bounds though NODE_COUNT matches */
        if (i < (int)(sizeof(FLUX_APPS)/sizeof(FLUX_APPS[0]))) {
            flux_draw_card(x, y, card_w, card_h, FLUX_APPS[i].title, FLUX_APPS[i].color, (flux_node_id_t)i);
        }
    }
}

static void draw_focus_mode(void) {
    if (!focused_space || !focused_space->active) {
        current_zoom = FLUX_MACRO;
        return;
    }

    int margin = 40; 
    focused_space->bounds.x = margin;
    focused_space->bounds.y = margin;
    focused_space->bounds.w = 1024 - (margin * 2);
    focused_space->bounds.h = 768 - (margin * 2);
    
    /* Shadow/Glow (Pulsing) */
    uint32_t ticks = (uint32_t)timer_get_ticks();
    int phase = ticks % 200; /* 2 second period */
    int val = (phase < 100) ? phase : (200 - phase); /* 0..100..0 */
    /* Alpha between 60 (approx 0x3C) and 180 (approx 0xB4) */
    uint8_t alpha = 60 + ((val * 120) / 100);

    /* Outer soft glow */
    /* ⚡ BOLT Optimization: Draw hollow box shadow (Avoid overdraw) */
    draw_box_shadow(focused_space->bounds.x, focused_space->bounds.y,
                    focused_space->bounds.w, focused_space->bounds.h, 6, FLUX_ACCENT_CYAN, alpha / 3);

    /* Inner strong glow */
    draw_box_shadow(focused_space->bounds.x, focused_space->bounds.y,
                    focused_space->bounds.w, focused_space->bounds.h, 2, FLUX_ACCENT_CYAN, alpha);

    /* Draw Window Frame (Title Bar, Close Button, Background) */
    wm_draw_window(focused_space);
                     
    /* Dispatch Draw Content based on Node ID */
    if (zooming_node == NODE_TERMINAL) {
         draw_terminal_content(focused_term);
    } else if (zooming_node == NODE_SYSMON) {
         if (focused_space == win_sysinfo) {
             wm_fill_rect(focused_space, (rect_t){0,0,focused_space->bounds.w, focused_space->bounds.h}, FLUX_CARD_BG);
             draw_sysinfo_content();
         }
    } else if (zooming_node == NODE_MATRIX) {
         /* Matrix is auto-drawn by tasks, but we can draw overlay here if needed */
    } else if (zooming_node == NODE_SETTINGS) {
         draw_settings_content();
    } else if (zooming_node == NODE_FILES) {
         draw_files_content();
    } else if (zooming_node == NODE_SANTITRAVEL) {
         draw_santitravel_content();
    } else if (zooming_node == NODE_CLOCK) {
         draw_clock_content(focused_space);
    } else if (zooming_node == NODE_BROWSER) {
         draw_browser_content();
    } else if (zooming_node == NODE_DEVMAN) {
         if (focused_space == win_devman) {
             draw_devman_content();
         }
    } else if (zooming_node == NODE_DOOM) {
         if (focused_space == win_doom) {
             draw_doom_content();
         }
    } else {
         /* Generic for others */
         if (zooming_node < sizeof(FLUX_APPS)/sizeof(FLUX_APPS[0])) {
             draw_generic_app_content(focused_space, FLUX_APPS[zooming_node].title, 0x222222);
         }
    }

    /* Flux UX: Close Button Hover Glow (Micro-interaction) - Drawn AFTER content */
    int btn_size = 20;
    int btn_margin = (TITLE_BAR_HEIGHT - btn_size) / 2;
    int btn_x = focused_space->bounds.x + focused_space->bounds.w - btn_size - btn_margin;
    int btn_y = focused_space->bounds.y + btn_margin;

    if (mouse_x >= btn_x && mouse_x <= btn_x + btn_size &&
        mouse_y >= btn_y && mouse_y <= btn_y + btn_size) {

        /* Draw Glow (Lighter Red) */
        omni_fill_rect(btn_x, btn_y, btn_size, btn_size, 0xFF6666);

        /* Redraw X (White) */
        omni_draw_line(btn_x + 4, btn_y + 4, btn_x + 15, btn_y + 15, 0xFFFFFF);
        omni_draw_line(btn_x + 15, btn_y + 4, btn_x + 4, btn_y + 15, 0xFFFFFF);

        /* Tooltip: Close */
        const char* tip = (settings_lang == 0) ? "Cerrar" : "Close";
        int tip_w = strlen(tip) * 8 + 10;
        int tip_h = 20;
        int tip_x = btn_x + (btn_size - tip_w) / 2;

        /* Clamp to screen width */
        uint32_t screen_w = omni_get_width();
        if (screen_w == 0) screen_w = 1024;
        if (tip_x + tip_w > (int)screen_w) tip_x = screen_w - tip_w - 5;

        int tip_y = btn_y + btn_size + 5;

        /* Shadow */
        omni_fill_rect(tip_x + 2, tip_y + 2, tip_w, tip_h, 0x000000);
        /* Bg */
        omni_fill_rect(tip_x, tip_y, tip_w, tip_h, 0x252525);
        /* Border */
        omni_draw_rect(tip_x, tip_y, tip_w, tip_h, 0x555555);
        /* Text */
        omni_draw_string(NULL, tip_x + 5, tip_y + 4, tip, 0xFFFFFF, 0x252525);
    }
    
    /* Capa 2: Controles de Navegación (Reactive Bottom Bar) */
    /* Check Hover */
    bool hover_bottom = (mouse_y > 720);
    
    int bar_h = hover_bottom ? 8 : 4;
    int bar_w = hover_bottom ? 240 : 180; /* Expands on hover */
    uint32_t bar_col = hover_bottom ? FLUX_ACCENT_CYAN : 0x404040;
    
    int bar_x = (1024 - bar_w) / 2;
    int bar_y = 768 - 15;
    
    omni_fill_rect(bar_x, bar_y, bar_w, bar_h, bar_col);

    /* UX: Home label on hover - guides user back to Constellation */
    if (hover_bottom) {
        const char* hint = "<- CONSTELACION";
        int hint_w = strlen(hint) * 8;
        int hint_x = (1024 - hint_w) / 2;
        int hint_y = bar_y - 20;
        /* Glassmorphism tooltip bg */
        omni_fill_rect_alpha(hint_x - 8, hint_y - 4, hint_w + 16, 18, 0x0A0A0A, 0xD0);
        omni_draw_string(NULL, hint_x, hint_y, hint, FLUX_ACCENT_CYAN, 0x0A0A0A);
    }
}

/* Input Handling for Flux */
static void handle_flux_click(void) {
    uint32_t screen_w = omni_get_width();
    uint32_t screen_h = omni_get_height();
    if (screen_w == 0) screen_w = 1024;
    if (screen_h == 0) screen_h = 768;

    /* Global Hub Toggle (Top Left Logo) */
    if (mouse_y < 32 && mouse_x < 100) {
        hub_active = !hub_active;
        if (hub_active) hub_input[0] = 0;
        return;
    }

    if (hub_active) {
        /* If clicking outside the hub, close it */
        int w = 500; int h = 400;
        int x = (screen_w - w) / 2;
        int y = (screen_h - h) / 2;
        if (mouse_x < x || mouse_x > x + w || mouse_y < y || mouse_y > y + h) {
            hub_active = false;
            return;
        }

        /* Handle clicks on Hub items */
        int ix = x + 25;
        int list_y = y + 55 + 55; // y + 110
        int col_w = (w - 60) / 2;

        int match_count = 0;
        for (int i = 0; i < NODE_COUNT; i++) {
            if (match_count >= 6) break;
            if (strlen(hub_input) == 0 || flux_strstr(FLUX_APPS[i].title, hub_input)) {
                int item_y = list_y + 20 + (match_count * 20);
                int item_h = 20;

                if (mouse_x >= ix && mouse_x < ix + col_w &&
                    mouse_y >= item_y && mouse_y < item_y + item_h) {

                    flux_launch_space((flux_node_id_t)i);
                    hub_active = false;
                    return;
                }
                match_count++;
            }
        }
        return;
    }

    if (current_zoom == FLUX_MACRO) {
        /* Hit testing Constellation Cards - 4x4 Grid */
        int card_w = 180;
        int card_h = 140;
        int gap_x = 30;
        int gap_y = 30;
        
        int total_w = (card_w * 4) + (gap_x * 3);
        int start_x = (int)screen_w - total_w; 
        start_x /= 2;
        int start_y = 120;

        for (int i = 0; i < NODE_COUNT; i++) {
            int col = i % 4;
            int row = i / 4;
            
            int x = start_x + col * (card_w + gap_x);
            int y = start_y + row * (card_h + gap_y);
            
            if (mouse_x >= x && mouse_x <= x + card_w && mouse_y >= y && mouse_y <= y + card_h) {
                flux_launch_space((flux_node_id_t)i); 
                return;
            }
        }
        
        /* Focus Mode Input (Not applicable in MACRO, but logic flow check) */
    } else if (current_zoom == FLUX_FOCUS) {
        /* Check Bottom Home gesture (Bottom 10% of screen) */
        int gesture_zone = screen_h - 48;
        if (mouse_y > gesture_zone) {
            target_zoom = FLUX_MACRO;
            return;
        }
        
        /* Pass clicks to active window if inside bounds */
        if (focused_space) {
             /* Check Close Button Click (Top Right) */
             int title_h = 30; /* Defined in window.c */
             int btn_size = 20;
             int btn_margin = (title_h - btn_size) / 2;

             int btn_x = focused_space->bounds.x + focused_space->bounds.w - btn_size - btn_margin;
             int btn_y = focused_space->bounds.y + btn_margin;

             if (mouse_x >= btn_x && mouse_x <= btn_x + btn_size &&
                 mouse_y >= btn_y && mouse_y <= btn_y + btn_size) {
                 target_zoom = FLUX_MACRO;
                 return;
             }

             int lx = mouse_x - focused_space->bounds.x;
             int ly = mouse_y - focused_space->bounds.y;
             
             /* Content usually handles its own absolute coords? No, window relative. */
             /* The handlers below assume Window-Local coordinates */
             
             if (focused_space == win_settings) {
                 handle_settings_click(lx, ly - TITLE_BAR_HEIGHT);
             } else if (focused_space == win_sysinfo) {
                 handle_sysinfo_click(lx, ly - TITLE_BAR_HEIGHT);
             } else if (focused_space == win_files) {
                 handle_files_click(lx, ly - TITLE_BAR_HEIGHT);
             } else if (focused_space == win_santitravel) {
                 handle_santitravel_click(lx, ly - TITLE_BAR_HEIGHT);
             } else if (focused_space == win_browser) {
                 handle_browser_click(lx, ly - TITLE_BAR_HEIGHT);
             }
        }
    }
}

/* ========================================================================= */
/* Flux Animation State (Fixed-Point Math 1000 = 1.0)                        */
/* ========================================================================= */
/* Moved to top of file to avoid forward declaration issues */

/* Ripple Effect State */
typedef struct {
    int x, y;
    int radius;
    bool active;
} flux_ripple_t;

static flux_ripple_t click_ripple = {0};

/* Physics State */
static int zoom_velocity = 0;

static void flux_set_zoom(flux_zoom_level_t level, flux_node_id_t node) {
    target_zoom = level;
    if (level == FLUX_FOCUS) {
        zooming_node = node;
        /* Calculate source rect for the node */
        /* Calculate source rect for the node using 4x4 Grid */
        int card_w = 180;
        int card_h = 140;
        int gap_x = 30;
        int gap_y = 30;
        int total_w = (card_w * 4) + (gap_x * 3);
        int start_x = (1024 - total_w) / 2;
        int start_y = 120;

        int i = (int)node;
        int col = i % 4;
        int row = i / 4;
        
        int x = start_x + col * (card_w + gap_x);
        int y = start_y + row * (card_h + gap_y);
        
        source_rect = (rect_t){x, y, card_w, card_h};
    }
}

static void flux_update_zoom(void) {
    /* Fixed-Point Spring Physics (1000 = 1.0) */
    int target = (target_zoom == FLUX_FOCUS) ? 1000 : 0;
    int tension = 250; /* Faster snap (was 180) */
    int friction = 700; /* Less friction (was 750) */
    
    int dist = target - zoom_progress;
    int force = (dist * tension) / 1000;
    
    /* Minimum push to overcome integer division floor */
    if (dist > 0 && force == 0) force = 2;
    if (dist < 0 && force == 0) force = -2;

    zoom_velocity += force;
    zoom_velocity = (zoom_velocity * friction) / 1000;
    zoom_progress += zoom_velocity;
    
    /* Stronger Snap */
    if (target == 1000 && zoom_progress > 950) {
        zoom_progress = 1000;
        zoom_velocity = 0;
    }
    if (target == 0 && zoom_progress < 50) {
        zoom_progress = 0;
        zoom_velocity = 0;
    }

    /* Update State */
    if (zoom_progress == 1000) current_zoom = FLUX_FOCUS;
    else if (zoom_progress == 0) current_zoom = FLUX_MACRO;
    else current_zoom = -1; /* Transitioning */
}

static rect_t draw_zoom_transition(void) {
    /* Optimization: Clear only the potential area of the window (Target Rect) */
    /* This allows us to keep the rest of the screen static (Constellation) */
    /* and drastically reduces bandwidth by flushing only this area. */
    
    /* Clear Maximum Bounds (Target Rect) to Black to erase trails */
    /* We use a slightly larger area to be safe? No, target_rect is the max size. */
    /* But if we are shrinking, we start at target_rect and go to source_rect. */
    /* So wiping target_rect is always safe to clear trails. */
    
    /* However, wiping Target Rect (big) every frame is costlier than necessary if small */
    /* But standard rect fill is very fast. The bottleneck is FLUSH (PCI bus). */
    /* So we fill big rect, draw small rect, flush big rect. */
    
    /* Determine bounds */
    int bx = (source_rect.x < target_rect.x) ? source_rect.x : target_rect.x;
    int by = (source_rect.y < target_rect.y) ? source_rect.y : target_rect.y;
    int bw = (source_rect.w > target_rect.w) ? source_rect.w : target_rect.w;
    int bh = (source_rect.h > target_rect.h) ? source_rect.h : target_rect.h;
    
    /* Align to bounds union */
    omni_fill_rect(bx, by, bw, bh, 0x000000);

    /* Draw Expanding Card */
    rect_t r;
    r.x = source_rect.x + ((target_rect.x - source_rect.x) * zoom_progress) / 1000;
    r.y = source_rect.y + ((target_rect.y - source_rect.y) * zoom_progress) / 1000;
    r.w = source_rect.w + ((target_rect.w - source_rect.w) * zoom_progress) / 1000;
    r.h = source_rect.h + ((target_rect.h - source_rect.h) * zoom_progress) / 1000;
    
    omni_fill_rect(r.x, r.y, r.w, r.h, FLUX_CARD_BG);
    omni_fill_rect(r.x, r.y, r.w, 4, FLUX_ACCENT_CYAN);
    
    if (zoom_progress > 900) {
         omni_draw_string(NULL, r.x + 20, r.y + 20, "Materializando...", FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
    }
    
    return (rect_t){bx, by, bw, bh};
}

static void draw_ripple_effect(void) {
    if (!click_ripple.active) return;

    /* Expand */
    click_ripple.radius += 3;
    if (click_ripple.radius > 60) {
        click_ripple.active = false;
        return;
    }

    /* Draw Ring using Midpoint Algorithm */
    int cx = click_ripple.x;
    int cy = click_ripple.y;
    int r = click_ripple.radius;
    uint32_t col = FLUX_ACCENT_CYAN;
    uint8_t alpha = 255 - (r * 4); /* Fade out */

    int x = 0;
    int y = r;
    int d = 3 - 2 * r;

    while (x <= y) {
        /* Draw 2x2 blocks for visibility */
        omni_fill_rect_alpha(cx + x, cy + y, 2, 2, col, alpha);
        omni_fill_rect_alpha(cx - x, cy + y, 2, 2, col, alpha);
        omni_fill_rect_alpha(cx + x, cy - y, 2, 2, col, alpha);
        omni_fill_rect_alpha(cx - x, cy - y, 2, 2, col, alpha);
        omni_fill_rect_alpha(cx + y, cy + x, 2, 2, col, alpha);
        omni_fill_rect_alpha(cx - y, cy + x, 2, 2, col, alpha);
        omni_fill_rect_alpha(cx + y, cy - x, 2, 2, col, alpha);
        omni_fill_rect_alpha(cx - y, cy - x, 2, 2, col, alpha);

        if (d < 0) {
            d += 4 * x + 6;
        } else {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

static void draw_generic_app_content(window_t* win, const char* title, uint32_t bg_col) {
    if (!win) return;
    wm_fill_rect(win, (rect_t){0, 0, win->bounds.w, win->bounds.h}, bg_col);
    wm_print_at(win, 20, 20, title);
    wm_print_at(win, 20, 40, "Aplicacion en construccion...");
}



static void draw_clock_content(window_t* win) {
    if (!win) return;
    wm_fill_rect(win, (rect_t){0, 0, win->bounds.w, win->bounds.h}, 0x101020);
    
    static char cached_time_str[32] = "--:--:--";
    static uint32_t last_clock_update = 0;
    
    if (timer_get_ticks() - last_clock_update > 50 || last_clock_update == 0) {
        uint32_t ticks = timer_get_ticks();
        int sec = (ticks / 100) % 60;
        int min = (ticks / 6000) % 60;
        int hour = (ticks / 360000) % 24;

        char buf[8];
        itoa_s(hour, buf, sizeof(buf), 10);
        strlcpy(cached_time_str, buf, sizeof(cached_time_str));
        strlcat(cached_time_str, ":", sizeof(cached_time_str));
        itoa_s(min, buf, sizeof(buf), 10);
        if(min<10) strlcat(cached_time_str, "0", sizeof(cached_time_str));
        strlcat(cached_time_str, buf, sizeof(cached_time_str));
        strlcat(cached_time_str, ":", sizeof(cached_time_str));
        itoa_s(sec, buf, sizeof(buf), 10);
        if(sec<10) strlcat(cached_time_str, "0", sizeof(cached_time_str));
        strlcat(cached_time_str, buf, sizeof(cached_time_str));

        last_clock_update = timer_get_ticks();
    }
    
    /* Big Text (Fake implementation) */
    wm_print_at(win, 60, 60, "HORA DEL SISTEMA");
    wm_print_at(win, 80, 90, cached_time_str);
}



/* ========================================================================= */
/* Flux Hub: UI & Interaction                                                */
/* ========================================================================= */

static void draw_flux_hub(void) {
    if (!hub_active) return;
    
    uint32_t sw = omni_get_width(); if (sw == 0) sw = 1024;
    uint32_t sh = omni_get_height(); if (sh == 0) sh = 768;
    
    int w = 600;
    int h = 450;
    int x = (sw - w) / 2;
    int y = (sh - h) / 2;
    
    /* 1. Ultra-Glass Backdrop */
    omni_fill_rect_alpha(x - 8, y - 8, w + 16, h + 16, 0x000000, 0x60);
    omni_fill_rect_alpha(x, y, w, h, 0x080808, 0xF2);
    omni_fill_rect(x, y, w, 1, 0x555555);
    
    /* 2. Header & Search Area */
    omni_draw_string(NULL, x + 25, y + 25, "UNIVERSAL DOM EXPLORER", FLUX_ACCENT_CYAN, 0x080808);
    
    int ix = x + 25;
    int iy = y + 55;
    int iw = w - 50;
    omni_fill_rect(ix, iy, iw, 35, 0x151515);
    if (strlen(hub_input) == 0) {
        omni_draw_string(NULL, ix + 10, iy + 10, "Busca apps, procesos o archivos...", 0x444444, 0x151515);
    } else {
        omni_draw_string(NULL, ix + 10, iy + 10, hub_input, 0xFFFFFF, 0x151515);
    }

    /* Cursor Blinking (UX: Visual Feedback for Input Focus) */
    int cursor_x = ix + 10 + (strlen(hub_input) * 8);
    /* Blink every ~300ms */
    if ((timer_get_ticks() / 30) % 2 == 0) {
        omni_fill_rect(cursor_x, iy + 10, 2, 16, FLUX_ACCENT_CYAN);
    }
    
    /* 3. Categorized Listing (The "DOM") */
    int list_y = iy + 55;
    int col_w = (w - 60) / 2;
    
    /* LEFT: Apps & System */
    omni_draw_string(NULL, ix, list_y, "[ NODOS DE APLICACION ]", 0x666666, 0x080808);
    int match_count = 0;
    for (int i = 0; i < NODE_COUNT; i++) {
        if (match_count >= 6) break;
        if (strlen(hub_input) == 0 || flux_strstr(FLUX_APPS[i].title, hub_input)) {
            int item_y = list_y + 20 + (match_count * 20);
            int item_h = 20;

            bool hover = (mouse_x >= ix && mouse_x < ix + col_w &&
                          mouse_y >= item_y && mouse_y < item_y + item_h);

            if (hover) {
                 omni_fill_rect_alpha(ix, item_y, col_w, item_h, 0x333333, 0xC0);
            }

            uint32_t col = (match_count == 0 && strlen(hub_input) > 0) ? FLUX_ACCENT_CYAN : FLUX_TEXT_SECONDARY;

            if (hover) col = FLUX_ACCENT_CYAN;

            omni_draw_string_transparent(ix + 10, item_y, FLUX_APPS[i].title, col);
            match_count++;
        }
    }
    
    /* RIGHT: Active Processes (The "Live DOM") */
    int rx = ix + col_w + 30;
    omni_draw_string(NULL, rx, list_y, "[ PROCESOS ACTIVOS ]", 0x666666, 0x080808);
    int proc_count = 0;
    for (int i = 0; i < 32; i++) {
        task_t* t = task_get_by_id(i);
        if (t && t->state != TASK_DEAD) {
            if (proc_count >= 6) break;
            char p_buf[32];
            strlcpy(p_buf, t->name, 20);
            strlcat(p_buf, " (RUN)", 32);
            omni_draw_string(NULL, rx + 10, list_y + 20 + (proc_count * 20), p_buf, 0x88CC88, 0x080808);
            proc_count++;
        }
    }
    
    /* BOTTOM: File System (The "Static DOM") */
    int bx = ix;
    int by = list_y + 160;
    omni_draw_string(NULL, bx, by, "[ DISCO (DOM) ]", 0x666666, 0x080808);
    
    /* Simular listado de /initrd */
    const char* files[] = {"test.elf", "logo.png", "hello.txt", "doom1.wad", "README"};
    int fc = 0;
    for (int i = 0; i < 5; i++) {
        if (strlen(hub_input) == 0 || flux_strstr(files[i], hub_input)) {
            if (fc >= 4) break;
            omni_draw_string(NULL, bx + 10, by + 20 + (fc * 20), files[i], 0xAAAAAA, 0x080808);
            fc++;
        }
    }
    
    if (match_count == 0 && fc == 0 && proc_count > 0 && strlen(hub_input) > 0) {
        omni_draw_string(NULL, ix, by + 120, "CMD: Presiona Enter para ejecutar comando", FLUX_ACCENT_AMBER, 0x080808);
    }
}

static void handle_hub_input(char c) {
    if (c == 27) { /* ESC */
        hub_active = false;
        return;
    }
    
    if (c == '\n') {
        /* 1. Try to find matching app */
        for (int i = 0; i < NODE_COUNT; i++) {
            if (flux_strstr(FLUX_APPS[i].title, hub_input)) {
                flux_launch_space((flux_node_id_t)i);
                hub_active = false;
                return;
            }
        }
        
        /* 2. Fallback: Execute as Shell Command */
        if (strlen(hub_input) > 0) {
            extern int shell_exec(char* cmd);
            shell_exec(hub_input);
            flux_notify("Terminal", "Comando ejecutado", FLUX_ACCENT_CYAN);
        }
        hub_active = false;
        return;
    }
    
    if (c == '\b') {
        int len = strlen(hub_input);
        if (len > 0) hub_input[len - 1] = 0;
    } else if (c >= 32 && c <= 126) {
        int len = strlen(hub_input);
        if (len < 63) {
            hub_input[len] = c;
            hub_input[len + 1] = 0;
        }
    }
}

/* ========================================================================= */
/* UI Event Queue (Reactive Input System)                                    */
/* ========================================================================= */

typedef enum {
    UI_EVENT_MOUSE_PACKET,
    UI_EVENT_KEY_PRESS
} ui_event_type_t;

typedef struct {
    ui_event_type_t type;
    int32_t dx;
    int32_t dy;
    uint8_t buttons;
    char key;
} ui_event_t;

#define UI_EVENT_QUEUE_SIZE 128
static ui_event_t ui_event_queue[UI_EVENT_QUEUE_SIZE];
static volatile int ui_event_head = 0;
static volatile int ui_event_tail = 0;

/* ========================================================================= */
/* New Window Manager Integration Wrappers                                   */
/* ========================================================================= */

static window_t* desktop_win = NULL;
static window_t* taskbar_win = NULL;
static window_t* hub_win = NULL;

/* Desktop Callbacks */
static void desktop_on_paint(window_t* win) {
    (void)win;
    draw_constellation();

    /* Layer 3: System Notifications (Always on top of desktop) */
    draw_notifications();

    /* Ripple Effect */
    draw_ripple_effect();
}

static void desktop_on_mouse(window_t* win, int x, int y, int buttons) {
    (void)win;
    /* Update globals for existing logic */
    mouse_x = x;
    mouse_y = y;

    bool new_left = (buttons & 1);
    if (new_left && !mouse_left_btn) {
        /* Click */
        mouse_left_btn = true;
        handle_flux_click();

        /* Trigger Ripple */
        click_ripple.x = mouse_x;
        click_ripple.y = mouse_y;
        click_ripple.radius = 5;
        click_ripple.active = true;
    }
    mouse_left_btn = new_left;
}

/* Taskbar Callbacks */
static void taskbar_on_paint(window_t* win) {
    (void)win;
    draw_global_status_bar();
}

static void taskbar_on_mouse(window_t* win, int x, int y, int buttons) {
    (void)win;
    mouse_x = x;
    mouse_y = y;

    bool new_left = (buttons & 1);
    if (new_left && !mouse_left_btn) {
        mouse_left_btn = true;
        /* Handle Hub Toggle if click is in corner */
        if (x < 100) {
            hub_active = !hub_active;
            if (hub_win) hub_win->active = hub_active;
        }
    }
    mouse_left_btn = new_left;
}

/* Hub Callbacks */
static void hub_on_paint(window_t* win) {
    (void)win;
    draw_flux_hub();
}

static void hub_on_key(window_t* win, char key) {
    (void)win;
    handle_hub_input(key);
    if (!hub_active && hub_win) {
        hub_win->active = false;
    }
}

/* App Wrappers */
static void terminal_on_paint(window_t* win) {
    /* Find which terminal instance this is?
       For now, just draw focused_term if it matches.
       Ideally, we store instance pointer in win->user_data.
       But window_t doesn't have user_data yet.
       We will use the existing logic which draws based on 'focused_term' or loop.
    */
    /* Currently draw_terminal_content takes a term instance */
    /* We need to find the term instance that owns this window */
    for(int i=0; i<MAX_TERMINALS; i++) {
        if(terminals[i].win == win) {
            draw_terminal_content(&terminals[i]);
            return;
        }
    }
}

static void terminal_on_key(window_t* win, char key) {
    for(int i=0; i<MAX_TERMINALS; i++) {
        if(terminals[i].win == win) {
            term_handle_key(&terminals[i], key);
            return;
        }
    }
}

/* Generic App Wrappers */
static void sysinfo_on_paint(window_t* win) { (void)win; draw_sysinfo_content(); }
static void sysinfo_on_mouse(window_t* win, int x, int y, int b) {
    (void)win; (void)b; handle_sysinfo_click(x, y);
}

static void settings_on_paint(window_t* win) { (void)win; draw_settings_content(); }
static void settings_on_mouse(window_t* win, int x, int y, int b) {
    (void)win; (void)b; handle_settings_click(x, y);
}

static void files_on_paint(window_t* win) { (void)win; draw_files_content(); }
static void files_on_mouse(window_t* win, int x, int y, int b) {
    (void)win; (void)b; handle_files_click(x, y);
}

static void travel_on_paint(window_t* win) { (void)win; draw_santitravel_content(); }
static void travel_on_mouse(window_t* win, int x, int y, int b) {
    (void)win; (void)b; handle_santitravel_click(x, y);
}

static void browser_on_paint(window_t* win) { (void)win; draw_browser_content(); }
static void browser_on_mouse(window_t* win, int x, int y, int b) {
    (void)win; (void)b; handle_browser_click(x, y);
}
static void browser_on_key(window_t* win, char key) {
    (void)win;
    if (browser_url_active) {
        if (key == '\n') {
            browser_url_active = false;
            sys_net_fetch(browser_url);
        } else if (key == '\b') {
            int len = strlen(browser_url);
            if (len > 0) browser_url[len-1] = 0;
        } else {
            int len = strlen(browser_url);
            if (len < (int)sizeof(browser_url)-1) {
                browser_url[len] = key;
                browser_url[len+1] = 0;
            }
        }
    }
}

static void devman_on_paint(window_t* win) { (void)win; draw_devman_content(); }
static void doom_on_paint(window_t* win) { (void)win; draw_doom_content(); }
/* static void doom_on_key(window_t* win, char key) { (void)win; doom_handle_input((int)key, 1); } */

void gui_demo_run(void) {
    serial_write_string("[GUI] Starting GUI (WM Mode)...\n");
    
    desktop_running = true;
    term_init_all();
    wm_init();
    omni_init();
    framebuffer_enable_double_buffer();
    
    /* Create Desktop Window (Bottom) */
    desktop_win = wm_create_window(0, 0, 1024, 768, "Desktop");
    desktop_win->bg_color = 0x000000;
    desktop_win->on_paint = desktop_on_paint;
    desktop_win->on_mouse = desktop_on_mouse;
    
    /* Create Taskbar (Top) */
    taskbar_win = wm_create_window(0, 0, 1024, 32, "Taskbar");
    taskbar_win->bg_color = 0x000000; // Handled by draw
    taskbar_win->on_paint = taskbar_on_paint;
    taskbar_win->on_mouse = taskbar_on_mouse;
    
    /* Create Hub (Hidden) */
    hub_win = wm_create_window(0, 0, 1024, 768, "Hub");
    hub_win->active = false;
    hub_win->on_paint = hub_on_paint;
    hub_win->on_key = hub_on_key; /* Hub needs focus to type */
    
    /* Reset State */
    current_zoom = FLUX_MACRO;
    
    serial_write_string("[GUI] Entering WM Event Loop...\n");
    
    while (desktop_running) {
        /* Pump Events */
        wm_pump_events();
        
        /* Check Hub State Sync */
        if (hub_active && !hub_win->active) {
            hub_win->active = true;
            wm_bring_to_front(hub_win);
        } else if (!hub_active && hub_win->active) {
            hub_win->active = false;
        }
        
        task_yield();
    }
    
    terminal_initialize(NULL);
}
