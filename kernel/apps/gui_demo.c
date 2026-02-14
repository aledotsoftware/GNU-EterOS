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
#include <task.h>
#include <types.h>
#include <string.h>
#include <vga.h>
#include <timer.h>
#include <keyboard.h>
#include <mouse.h>
#include <framebuffer.h>
#include <shell.h>
#include <pmm.h>
#include <serial.h>
#include <ui/image.h>
#include <rtc.h>
#include <fs/initrd.h>
#include <syscall.h>

/* ========================================================================= */
/* Constantes y Configuración Visual                                         */
/* ========================================================================= */
#define TASKBAR_HEIGHT      40
#define TITLE_BAR_HEIGHT    30

static volatile bool desktop_running = true;

/* Ventanas y Estado */
static window_t* win_sysinfo = NULL;

/* Mouse */
static int32_t mouse_x = 512;
static int32_t mouse_y = 384;
static bool    mouse_left_btn = false;
static bool    mouse_clicked = false;



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
    NODE_PONG,
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
    { NODE_PONG,     "Pong",         0xFFFFFF }
};


/* Forward declarations */
static void flux_notify(const char* title, const char* message, uint32_t accent);
static void flux_set_zoom(flux_zoom_level_t level, flux_node_id_t node);
static void flux_launch_space(flux_node_id_t node);

/* Helper Draw Functions */
static void draw_generic_app_content(window_t* win, const char* title, uint32_t bg_col);
static void draw_pong_content(window_t* win);
static void draw_clock_content(window_t* win);

/* Animation State Forward Decl (needed for input handling) */
static int zoom_progress = 0; 
static flux_zoom_level_t target_zoom = FLUX_MACRO;
static flux_node_id_t zooming_node = NODE_TERMINAL;
static rect_t source_rect = {0,0,0,0}; /* Constellation rect */
static rect_t target_rect = {40, 40, 944, 688}; /* Focus rect */ 


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
    
    /* Ventana ligeramente más ancha para title bar moderna */
    terminals[slot].win = wm_create_window(x, y, 360, 260 + TITLE_BAR_HEIGHT - 20, title);
    if (!terminals[slot].win) return;
    
    terminals[slot].win->bg_color = UI_COLOR_BLACK;
    terminals[slot].win->fg_color = UI_COLOR_WHITE; /* Ensure text is visible */
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
        framebuffer_rect(x, y, w, h, 0x404040);
        int fill_w = (w * percent_1000) / 1000;
        if (fill_w > w) fill_w = w;
        if (fill_w < 0) fill_w = 0;
        framebuffer_rect(x, y, fill_w, h, color);
    }
}

static void draw_sysinfo_content(void) {
    int w = win_sysinfo->bounds.w;
    int h = win_sysinfo->bounds.h;
    wm_fill_rect(win_sysinfo, (rect_t){0, 0, w, h}, 0x1A1A1A);
    
    /* Header Bar */
    wm_fill_rect(win_sysinfo, (rect_t){0, 0, w, 40}, 0x252525);
    wm_print_at(win_sysinfo, 20, 12, "ETER-MONITOR v2.0 (Live Kernel Context)");
    
    /* Stats Layout */
    int card_w = (w / 2) - 30;
    
    /* RAM CARD */
    rect_t ram_card = {20, 60, card_w, 100};
    wm_fill_rect(win_sysinfo, ram_card, 0x2A2A2A);
    wm_fill_rect(win_sysinfo, (rect_t){20, 60, 4, 100}, FLUX_ACCENT_AMBER);
    
    uint64_t total = pmm_get_total_ram();
    uint64_t free  = pmm_get_free_ram();
    uint64_t used  = total - free;
    int usage_pct_1000 = (total > 0) ? (used * 1000) / total : 0;
    
    char ram_buf[64] = "RAM: ";
    char num[16];
    itoa_s(used / (1024*1024), num, 16, 10); strlcat(ram_buf, num, 64); strlcat(ram_buf, " MB / ", 64);
    itoa_s(total / (1024*1024), num, 16, 10); strlcat(ram_buf, num, 64); strlcat(ram_buf, " MB", 64);
    wm_print_at(win_sysinfo, 40, 75, ram_buf);
    draw_progress_bar(win_sysinfo, 40, 100, card_w - 40, 12, usage_pct_1000, FLUX_ACCENT_AMBER);

    /* CPU CARD with Live Graph */
    rect_t cpu_card = {w/2 + 10, 60, card_w, 100};
    wm_fill_rect(win_sysinfo, cpu_card, 0x2A2A2A);
    wm_fill_rect(win_sysinfo, (rect_t){w/2 + 10, 60, 4, 100}, FLUX_ACCENT_CYAN);
    wm_print_at(win_sysinfo, w/2 + 30, 75, "CPU Load: Real-time");
    
    /* Visual activity graph */
    int gx = w/2 + 30;
    int gy = 100;
    int gw = card_w - 40;
    int gh = 40;
    wm_fill_rect(win_sysinfo, (rect_t){gx, gy, gw, gh}, 0x151515);
    for (int i=0; i<gw; i+=4) {
        int v = (timer_get_ticks() + i) % 30;
        int bh = (v * gh) / 30;
        wm_fill_rect(win_sysinfo, (rect_t){gx + i, gy + gh - bh, 2, bh}, 0x008080);
    }

    /* Process List (Lower Section) */
    int list_y = 180;
    wm_fill_rect(win_sysinfo, (rect_t){20, list_y, w - 40, h - list_y - 20}, 0x222222);
    wm_print_at(win_sysinfo, 40, list_y + 10, "PID   State   Name                 CPUID");
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
             row++;
             if (row > 10) break;
        }
    }
}

static window_t* win_matrix = NULL;
static volatile bool matrix_running = false;
#define MATRIX_COLS 40
#define MATRIX_ROWS 20
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
        wm_fill_rect(win_matrix, (rect_t){0, 0, 320, 200}, UI_COLOR_BLACK);
        for (int x = 0; x < MATRIX_COLS; x++) {
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
        ui_draw_string(NULL, w/2 - 40, 40, "AJUSTES", FLUX_TEXT_PRIMARY, 0x151515);
        
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
            ui_draw_string(NULL, cx, cy, "Ethernet: Link Up (1 Gbps)", 0x00FF00, 0x151515);
            
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
    #define DRAW_ITEM(y, icon, name) \
        wm_print_at(win_files, 20, y, icon); \
        wm_print_at(win_files, 50, y, name); \
        wm_fill_rect(win_files, (rect_t){20, y + 25, w - 40, 1}, 0x404040);

    if (strcmp(current_path, "/") == 0) {
        DRAW_ITEM(start_y, "[DIR]", "home");
        DRAW_ITEM(start_y + item_h, "[DIR]", "etc");
    } else {
        DRAW_ITEM(start_y, "[..]", "..");
        DRAW_ITEM(start_y + item_h, "[FILE]", "config.sys");
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
        wm_fill_rect(win_santitravel, (rect_t){20, ry, w - 40, 50}, 0x2A3545);
        wm_fill_rect(win_santitravel, (rect_t){20, ry, 4, 50}, colors[i]);
        wm_print_at(win_santitravel, 40, ry + 15, places[i]);
        wm_print_at(win_santitravel, w - 100, ry + 15, "[ VISITAR ]");
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
static char browser_url[128] = "https://tudexgames.com/ads.txt";
static char browser_content[2048] = "";
static bool browser_loading = false;
static char browser_status_text[64] = "Listo";
static char proto_buffer[2048] = "";

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

static void system_net_dispatcher_task(void) {
    strlcpy(browser_status_text, "Network: Iniciando DHCP...", 64);
    
    extern int network_ready;
    
    int timeout = 50;
    while (!network_ready && timeout-- > 0) {
        task_sleep(100);
    }
    
    if (!network_ready) {
        strlcpy(browser_content, "Error: No se pudo obtener IP via DHCP (Hardware Offline).", sizeof(browser_content));
        strlcpy(browser_status_text, "DHCP Fail", 64);
        browser_loading = false;
        task_exit();
    }
    
    strlcpy(browser_status_text, "DNS: Resolviendo host...", 64);
    task_sleep(400);
    
    strlcpy(browser_status_text, "TCP: Conectando...", 64);
    
    extern int raw_tcp_get(const char* host, const char* path, char* response_buf, size_t max_len);
    const char* path = parse_url_path(browser_url);
    int res = raw_tcp_get("tudexgames.com", path, proto_buffer, sizeof(proto_buffer));
    
    if (res > 0) {
        strlcpy(browser_content, proto_buffer, sizeof(browser_content));
        strlcpy(browser_status_text, "200 OK - Conexion Real", 64);
    } else {
        if (res == -2) strlcpy(browser_content, "Error: El servidor no respondio al SYN (TCP Timeout).", sizeof(browser_content));
        else if (res == -3) strlcpy(browser_content, "Error: Se conecto pero no hubo datos (HTTP Timeout).", sizeof(browser_content));
        else strlcpy(browser_content, "Error: Fallo en la capa de red (E1000/ARP).", sizeof(browser_content));
        strlcpy(browser_status_text, "Net Error", 64);
    }
    
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
    
    /* Status Bar (Adequate Info) */
    wm_fill_rect(win_browser, (rect_t){0, h - 25, w, 25}, 0xDDDDDD);
    ui_draw_string(NULL, win_browser->bounds.x + 10, win_browser->bounds.y + TITLE_BAR_HEIGHT + h - 18, 
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
            while (*ptr && *ptr != '\n' && i < 110) { line_buf[i++] = *ptr++; }
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
    if (current_tick - active_notif.tick_start > active_notif.duration) {
        active_notif.active = false;
        return;
    }
    
    /* Toast Top Center */
    int w = 300;
    int h = 60;
    int x = (1024 - w) / 2;
    int y = 40; /* Top margin */
    
    /* Background & Shadow effect */
    framebuffer_rect(x, y, w, h, 0x202020);
    framebuffer_rect(x, y + h, w, 2, 0x000000); /* Shadow */
    
    /* Accent Strip */
    framebuffer_rect(x, y, 4, h, active_notif.accent);
    
    /* Content */
    ui_draw_string(NULL, x + 15, y + 15, active_notif.title, FLUX_TEXT_PRIMARY, 0x202020);
    ui_draw_string(NULL, x + 15, y + 35, active_notif.message, FLUX_TEXT_SECONDARY, 0x202020);
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
             ui_draw_string(NULL, x + 10, start_y + (i * line_h), line_preview, FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
        }
    }
    ui_draw_string(NULL, x + 10, start_y + (max_lines * line_h), "> _", FLUX_ACCENT_CYAN, FLUX_CARD_BG);
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
    ui_draw_string(NULL, x + 10, bar_y - 15, "RAM Usage", FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
    draw_progress_bar(NULL, x + 10, bar_y, w - 20, 8, ram_pct_1000, (ram_pct_1000 > 800) ? UI_COLOR_RED : FLUX_ACCENT_AMBER);
    
    int wave_y = y + 100;
    ui_draw_string(NULL, x + 10, wave_y - 15, "CPU Activity", FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
    framebuffer_rect(x + 10, wave_y, w - 20, 1, 0x404040);
    int tick = (timer_get_ticks() / 10) % 20;
    framebuffer_rect(x + 10 + (tick * 5), wave_y - 5, 4, 10, FLUX_ACCENT_AMBER);
}

static void draw_matrix_preview(int x, int y, int w, int h) {
    (void)w; (void)h;
    int start_y = y + 50;
    for (int i=0; i<5; i++) {
        int col = (x + 20) + (i * 30);
        int drop = (timer_get_ticks() / 5 + i * 3) % 10;
        ui_draw_string(NULL, col, start_y + (drop * 10), "0", FLUX_ACCENT_VIOLET, FLUX_CARD_BG);
    }
}

static void draw_settings_preview(int x, int y, int w, int h) {
    (void)w; (void)h;
    int start_y = y + 60;
    ui_draw_string(NULL, x + 20, start_y, "Display: 1024x768", FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
    ui_draw_string(NULL, x + 20, start_y + 20, "Network: Online", FLUX_ACCENT_CYAN, FLUX_CARD_BG);
    ui_draw_string(NULL, x + 20, start_y + 40, "Lang: ES/EN", FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
}

static void draw_files_preview(int x, int y, int w, int h) {
    (void)w; (void)h;
    int start_y = y + 50;
    ui_draw_string(NULL, x + 20, start_y, "/home", FLUX_ACCENT_AMBER, FLUX_CARD_BG);
    ui_draw_string(NULL, x + 20, start_y + 20, "/etc", FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
    ui_draw_string(NULL, x + 20, start_y + 40, "/var", FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
}

static void draw_santitravel_preview(int x, int y, int w, int h) {
    (void)w; (void)h;
    int start_y = y + 60;
    ui_draw_string(NULL, x + 20, start_y, "SantiTravel", 0x55AAFF, FLUX_CARD_BG);
    
    /* Small Airplane animation */
    int tick = (timer_get_ticks() / 5) % 30;
    ui_draw_string(NULL, x + 20 + tick, start_y + 30, ">-o-o", 0xFFFFFF, FLUX_CARD_BG);
}

void gui_draw_boot_logo(void) {
    uint32_t sw = framebuffer_get_width();
    uint32_t sh = framebuffer_get_height();
    if (sw == 0) sw = 1024;
    if (sh == 0) sh = 768;

    /* 1. White Background */
    framebuffer_clear(0xFFFFFF);

    /* 2. Draw Logo from File (logo.raw) */
    int img_w = 200;
    int img_h = 200;
    ui_draw_image("logo.png", (sw - img_w) / 2, (sh - img_h) / 2 - 40);

    /* 3. Draw Text (Dark Grey) */
    const char* title = "ETEROS GENESIS";
    int title_w = strlen(title) * 8;
    ui_draw_string(NULL, (sw - title_w) / 2, sh / 2 + 60, title, 0x333333, 0xFFFFFF);
    
    const char* sub = "Cargando subsistemas...";
    int sub_w = strlen(sub) * 8;
    ui_draw_string(NULL, (sw - sub_w) / 2, sh / 2 + 80, sub, 0x666666, 0xFFFFFF);
    
    /* 4. Progress Bar (Light Grey Track, Cyan Fill) */
    int bw = 300;
    int bx = (sw - bw) / 2;
    int by = sh / 2 + 110;
    framebuffer_rect(bx, by, bw, 4, 0xEEEEEE);
    
    framebuffer_flush();
    
    /* Fake Progress Animation (approx 2 seconds) */
    /* Smooth Progress Animation (Time-based ~2 seconds) */
    uint32_t start_tick = timer_get_ticks();
    uint32_t duration_ticks = 200; /* 2 seconds @ 100Hz */
    
    while (1) {
        uint32_t current = timer_get_ticks();
        if (current - start_tick > duration_ticks) break;
        
        int progress = ((current - start_tick) * 100) / duration_ticks;
        int fill_w = (bw * progress) / 100;
        
        framebuffer_rect(bx, by, fill_w, 4, 0x00AAAA);
        
        /* Partial flush for speed! */
        framebuffer_flush_rect(bx, by, bw, 4);
        
        /* Yield to CPU, but wake up fast */
        task_sleep(10); 
    }
    
    /* Ensure 100% at end */
    framebuffer_rect(bx, by, bw, 4, 0x00AAAA);
    framebuffer_flush_rect(bx, by, bw, 4);
    
    /* Final pause (1 second) */
    task_sleep(100); 
}

/* ========================================================================= */
/* Flux Status Bar                                                           */
/* ========================================================================= */

static void draw_global_status_bar(void) {
    uint32_t screen_w = framebuffer_get_width();
    if (screen_w == 0) screen_w = 1024;
    
    int bar_h = 32;
    /* Deep Glassmorphism effect: Outer border + Inner fill */
    framebuffer_rect(0, 0, screen_w, bar_h, 0x050505);
    framebuffer_rect(0, bar_h - 1, screen_w, 1, 0x1A1A1A); 

    /* Left: System Branding + Pulse */
    int blink = (timer_get_ticks() / 50) % 2;
    ui_draw_string(NULL, 16, 8, "eterOS", 0xFFFFFF, 0x050505);
    framebuffer_rect(70, 14, 4, 4, blink ? FLUX_ACCENT_CYAN : 0x444444);
    
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
    
    ui_draw_string(NULL, 100, 10, stats_buf, 0x666666, 0x050505);

    /* Center: Mode Indicator with Glow */
    const char* mode_str = (current_zoom == FLUX_FOCUS) ? "FOCUS" : "CONSTELACION";
    int mode_w = strlen(mode_str) * 8;
    int mode_x = (screen_w - mode_w) / 2;
    ui_draw_string(NULL, mode_x, 8, mode_str, FLUX_TEXT_PRIMARY, 0x050505);
    /* Subtle underline glow */
    framebuffer_rect(mode_x, 24, mode_w, 1, (current_zoom == FLUX_FOCUS) ? FLUX_ACCENT_CYAN : FLUX_ACCENT_AMBER);

    /* Right Side */
    int right_margin = screen_w - 16;
    
    /* Battery/Energy (Modern Style) */
    int bat_w = 30;
    int bat_h = 12;
    int bat_x = right_margin - bat_w;
    int bat_y = (bar_h - bat_h) / 2;
    framebuffer_rect(bat_x - 1, bat_y - 1, bat_w + 2, bat_h + 2, 0x333333);
    framebuffer_rect(bat_x, bat_y, bat_w, bat_h, 0x111111);
    framebuffer_rect(bat_x + 2, bat_y + 2, bat_w - 4, bat_h - 4, 0x00CC00);
    
    right_margin -= (bat_w + 20);

    /* Clock (Real-Time - Argentina) */
    static char clock_buf[16] = "--:--";
    static uint32_t last_clock_upd = 0;
    
    if (timer_get_ticks() - last_clock_upd > 50 || last_clock_upd == 0) {
        rtc_time_t utc, local;
        rtc_get_time(&utc);
        rtc_to_argentina(&utc, &local);
        
        clock_buf[0] = '0' + (local.hours / 10);
        clock_buf[1] = '0' + (local.hours % 10);
        clock_buf[2] = ':';
        clock_buf[3] = '0' + (local.minutes / 10);
        clock_buf[4] = '0' + (local.minutes % 10);
        clock_buf[5] = 0;
        
        last_clock_upd = timer_get_ticks();
    }
    
    int clock_w = 5 * 8;
    ui_draw_string(NULL, right_margin - clock_w, 8, clock_buf, 0xAAAAAA, 0x050505);
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
        int strength = 120; // 0.12 * 1000
        shift_x = (dx * strength) / 1000;
        shift_y = (dy * strength) / 1000;
    }
    
    int draw_x = x + shift_x;
    int draw_y = y + shift_y;

    /* Capa 1: Contenedor (Using defined color) */
    uint32_t card_bg = FLUX_CARD_BG;
    if (dist_sq < 10000) { /* Hover highlight */
        card_bg = 0x383838; /* Lighter for visibility (was 0x1A1A1A) */
    }
    
    /* Active Glow (Steady, no strobing for A11y) */
    if (dist_sq < 40000) {
        framebuffer_rect(draw_x - 2, draw_y - 2, w + 4, h + 4, accent);
    }

    framebuffer_rect(draw_x, draw_y, w, h, card_bg); 
    
    /* Header */
    ui_draw_string(NULL, draw_x + 10, draw_y + 10, title, FLUX_TEXT_PRIMARY, card_bg);
    framebuffer_rect(draw_x + 10, draw_y + 26, w - 20, 1, 0x333333);
    
    /* Live Content / Icon Placeholder */
    int icon_cx = draw_x + (w/2);
    int icon_cy = draw_y + (h/2);
    
    if (node == NODE_TERMINAL) {
        draw_term_preview(draw_x, draw_y, w, h, &terminals[0]);
        /* Overlay Icon - more transparent look */
        for(int iy=0; iy<60; iy++) {
            for(int ix=0; ix<80; ix++) {
                if ((ix+iy)%2 == 0) framebuffer_putpixel(icon_cx-40+ix, icon_cy-30+iy, 0x000000);
            }
        }
        ui_draw_string(NULL, icon_cx - 20, icon_cy - 10, ">_", FLUX_ACCENT_CYAN, 0x000000);
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
    } else {
        /* Generic Preview for new apps */
        wm_fill_rect(NULL, (rect_t){draw_x+10, draw_y+40, w-20, h-50}, 0x111111);
        /* Draw big initial? For now just color block */
        framebuffer_rect(icon_cx-20, icon_cy-20, 40, 40, accent);
    }
    
    /* Active Border (Glow effect on hover) */
    uint32_t border_col = accent;
    if (dist_sq < 40000) border_col = 0xFFFFFF;
    framebuffer_rect(draw_x, draw_y + h - 2, w, 2, border_col);
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
        if (!win_sysinfo) win_sysinfo = wm_create_window(0, 0, 800, 600, "System Info"); 
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
         if (!win_settings) win_settings = wm_create_window(0, 0, 800, 600, "Configuracion");
         win_settings->active = true;
         focused_space = win_settings;
    } else if (node == NODE_FILES) {
         flux_notify("Files", "Sistema de Archivos en linea", FLUX_ACCENT_AMBER);
         if (!win_files) win_files = wm_create_window(0, 0, 800, 600, "Archivos");
         win_files->active = true;
         focused_space = win_files;
    } else if (node == NODE_SANTITRAVEL) {
         flux_notify("SantiTravel", "Preparando motores...", 0x55AAFF);
         if (!win_santitravel) win_santitravel = wm_create_window(0, 0, 800, 600, "SantiTravel");
         win_santitravel->active = true;
         focused_space = win_santitravel;
    } else if (node == NODE_BROWSER) {
         flux_notify("Navegador", "Cargando recurso local (Test)...", 0x44FFFF);
         if (!win_browser) win_browser = wm_create_window(50, 50, 800, 600, "Navegador");
         win_browser->active = true;
         focused_space = win_browser;
         
         /* Iniciar busqueda mediante llamada al sistema si no esta cargado */
         if (browser_content[0] == 0 && !browser_loading) {
             sys_net_fetch(browser_url);
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
    uint32_t screen_w = framebuffer_get_width();
    uint32_t screen_h = framebuffer_get_height();
    if (screen_w == 0) screen_w = 1024;
    if (screen_h == 0) screen_h = 768;
    
    if (!stars_initialized) init_constellation();

    /* Capa 0: Neural Ambient (Stars) */
    /* ⚡ BOLT OPTIMIZATION: Use pre-calculated positions */
    for (int i = 0; i < 64; i++) {
        framebuffer_putpixel(constellation_stars[i].x, constellation_stars[i].y, constellation_stars[i].col);
    }

    /* Draw Grid Background (Technical) - Very subtle */
    int grid_size = 120;
    for (uint32_t x = 0; x < screen_w; x += grid_size) {
        framebuffer_rect(x, 0, 1, screen_h, 0x080808);
    }
    for (uint32_t y = 0; y < screen_h; y += grid_size) {
        framebuffer_rect(0, y, screen_w, 1, 0x080808);
    }
    
    /* Header (Capa 3) */
    ui_draw_string(NULL, 50, 50, "CONSTELACION", FLUX_TEXT_SECONDARY, 0x000000);
    
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
    
    /* Shadow/Glow */
    framebuffer_rect(focused_space->bounds.x - 2, focused_space->bounds.y - 2, 
                     focused_space->bounds.w + 4, focused_space->bounds.h + 4, FLUX_ACCENT_CYAN); 
                     
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
    } else if (zooming_node == NODE_PONG) {
         draw_pong_content(focused_space);
    } else if (zooming_node == NODE_CLOCK) {
         draw_clock_content(focused_space);
    } else if (zooming_node == NODE_BROWSER) {
         draw_browser_content();
    } else {
         /* Generic for others */
         if (zooming_node < sizeof(FLUX_APPS)/sizeof(FLUX_APPS[0])) {
             draw_generic_app_content(focused_space, FLUX_APPS[zooming_node].title, 0x222222);
         }
    }
    
    /* Capa 2: Controles de Navegación (Reactive Bottom Bar) */
    /* Check Hover */
    bool hover_bottom = (mouse_y > 720);
    
    int bar_h = hover_bottom ? 8 : 4;
    int bar_w = hover_bottom ? 240 : 180; /* Expands on hover */
    uint32_t bar_col = hover_bottom ? FLUX_ACCENT_CYAN : 0x404040;
    
    int bar_x = (1024 - bar_w) / 2;
    int bar_y = 768 - 15;
    
    framebuffer_rect(bar_x, bar_y, bar_w, bar_h, bar_col);
}

/* Input Handling for Flux */
static void handle_flux_click(void) {
    uint32_t screen_w = framebuffer_get_width();
    uint32_t screen_h = framebuffer_get_height();
    if (screen_w == 0) screen_w = 1024;
    if (screen_h == 0) screen_h = 768;

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
             int lx = mouse_x - focused_space->bounds.x;
             int ly = mouse_y - focused_space->bounds.y;
             
             /* Content usually handles its own absolute coords? No, window relative. */
             /* The handlers below assume Window-Local coordinates */
             
             if (focused_space == win_settings) {
                 handle_settings_click(lx, ly - TITLE_BAR_HEIGHT);
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
    framebuffer_rect(bx, by, bw, bh, 0x000000);

    /* Draw Expanding Card */
    rect_t r;
    r.x = source_rect.x + ((target_rect.x - source_rect.x) * zoom_progress) / 1000;
    r.y = source_rect.y + ((target_rect.y - source_rect.y) * zoom_progress) / 1000;
    r.w = source_rect.w + ((target_rect.w - source_rect.w) * zoom_progress) / 1000;
    r.h = source_rect.h + ((target_rect.h - source_rect.h) * zoom_progress) / 1000;
    
    framebuffer_rect(r.x, r.y, r.w, r.h, FLUX_CARD_BG);
    framebuffer_rect(r.x, r.y, r.w, 4, FLUX_ACCENT_CYAN); 
    
    if (zoom_progress > 900) {
         ui_draw_string(NULL, r.x + 20, r.y + 20, "Materializando...", FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
    }
    
    return (rect_t){bx, by, bw, bh};
}

static void draw_generic_app_content(window_t* win, const char* title, uint32_t bg_col) {
    if (!win) return;
    wm_fill_rect(win, (rect_t){0, 0, win->bounds.w, win->bounds.h}, bg_col);
    wm_print_at(win, 20, 20, title);
    wm_print_at(win, 20, 40, "Aplicacion en construccion...");
}

static void draw_pong_content(window_t* win) {
    if (!win) return;
    wm_fill_rect(win, (rect_t){0, 0, win->bounds.w, win->bounds.h}, 0x000000);
    /* Draw Net */
    for(int y=10; y<win->bounds.h-10; y+=20) {
        wm_fill_rect(win, (rect_t){win->bounds.w/2 - 2, y, 4, 10}, 0xFFFFFF);
    }
    /* Draw Paddles */
    wm_fill_rect(win, (rect_t){20, 100, 10, 60}, 0xFFFFFF);
    wm_fill_rect(win, (rect_t){win->bounds.w - 30, 150, 10, 60}, 0xFFFFFF);
    /* Draw Ball */
    wm_fill_rect(win, (rect_t){win->bounds.w/2 - 50, 120, 10, 10}, 0xFFFFFF);
    
    wm_print_at(win, win->bounds.w/2 - 40, 20, "PONG - 0 : 0");
}

static void draw_clock_content(window_t* win) {
    if (!win) return;
    wm_fill_rect(win, (rect_t){0, 0, win->bounds.w, win->bounds.h}, 0x101020);
    
    char time_str[32];
    uint32_t ticks = timer_get_ticks();
    int sec = (ticks / 100) % 60;
    int min = (ticks / 6000) % 60;
    int hour = (ticks / 360000) % 24;
    
    /* Simple fake time based on uptime */
    char buf[8];
    itoa_s(hour, buf, sizeof(buf), 10);
    strlcpy(time_str, buf, sizeof(time_str));
    strlcat(time_str, ":", sizeof(time_str));
    itoa_s(min, buf, sizeof(buf), 10);
    if(min<10) strlcat(time_str, "0", sizeof(time_str));
    strlcat(time_str, buf, sizeof(time_str));
    strlcat(time_str, ":", sizeof(time_str));
    itoa_s(sec, buf, sizeof(buf), 10);
    if(sec<10) strlcat(time_str, "0", sizeof(time_str));
    strlcat(time_str, buf, sizeof(time_str));
    
    /* Big Text (Fake implementation) */
    wm_print_at(win, 60, 60, "HORA DEL SISTEMA");
    wm_print_at(win, 80, 90, time_str);
}



static void on_mouse_event(int8_t dx, int8_t dy, uint8_t buttons) {
    mouse_x += dx; mouse_y += dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= 1024) mouse_x = 1023;
    if (mouse_y >= 768) mouse_y = 767;
    
    bool left_btn = (buttons & 1);
    
    if (left_btn && !mouse_left_btn) {
        mouse_clicked = true;
    }
    mouse_left_btn = left_btn;
}

void gui_demo_run(void) {
    serial_write_string("[GUI] Starting GUI...\n");
    
    /* Reset state on launch/re-launch */
    desktop_running = true;
    serial_write_string("[GUI] Initializing terminals...\n");
    term_init_all();

    serial_write_string("[GUI] Initializing WM...\n");
    wm_init();
    
    serial_write_string("[GUI] Creating terminal window...\n");
    term_create(); 
    
    /* Re-enable Double Buffer for flicker-free rendering */
    framebuffer_enable_double_buffer();
    
    mouse_set_callback(on_mouse_event);

    /* 4. External Server Simulation - BACKGROUND TASK */
    task_create("ext_server", server_external_task);

    
    /* Init Apps */
    /* term_init_all(); -- Removed duplicate call */
    
    /* Force State */
    current_zoom = FLUX_MACRO;
    target_zoom = FLUX_MACRO;
    zoom_progress = 0;
    
    /* Initial Draw */
    serial_write_string("[GUI] Initial Draw...\n");
    draw_constellation();
    wm_draw_all();
    
    serial_write_string("[GUI] Entering Main Loop...\n");
    
    desktop_running = true;
    static uint64_t last_frame_ticks = 0;

    while (desktop_running) {
        /* ⚡ BOLT: Intelligent Frame Refresh (Cap at ~100 FPS if resolution is 10ms) */
        /* This prevents saturated loops that slow down the emulator's drawing pipeline */
        uint64_t current_ticks = timer_get_ticks();
        if (current_ticks == last_frame_ticks) {
            task_sleep(1); /* ⚡ BOLT: Real sleep to cool down the CPU */
            continue;
        }
        last_frame_ticks = current_ticks;

        rect_t dirty = {0, 0, 1024, 768};

        /* Handle Mouse Click in main thread context */
        if (mouse_clicked) {
            mouse_clicked = false;
            handle_flux_click();
        }

        /* Update Logic */
        flux_update_zoom();

        /* ... keyboard input ... */
        if (keyboard_has_input()) {
             char c = keyboard_getchar();
             
             /* Global Navigation Hotkeys */
             if (c == 27) { /* ESC */
                 if (current_zoom == FLUX_MACRO) {
                     desktop_running = false;
                 } else {
                     target_zoom = FLUX_MACRO;
                     flux_set_zoom(FLUX_MACRO, NODE_TERMINAL); /* Node doesn't matter for macro */
                 }
                 /* continue; would skip the rest of the loop, including drawing! 
                    We should just stop processing input for this frame or handle logic carefully. 
                    Actually, 'continue' in a 'while' loop skips to next iteration. 
                    So drawing is skipped. This causes flickering or black screen if held.
                    Better to just not process move/term logic. */
             } else {
                 /* Infinite Zoom Controls (Arrows) - Only if NOT typing in terminal */
                 bool typing_in_term = (current_zoom == FLUX_FOCUS && focused_term != NULL);
                 
                  if (!typing_in_term) {
                      if ((unsigned char)c == KEY_UP) { /* UP Arrow */
                          if (current_zoom == FLUX_MACRO && target_zoom == FLUX_MACRO) {
                              flux_set_zoom(FLUX_FOCUS, NODE_TERMINAL); 
                              flux_launch_space(NODE_TERMINAL); 
                          }
                      }
                      else if ((unsigned char)c == KEY_DOWN) { /* DOWN Arrow */
                          if (current_zoom == FLUX_FOCUS || target_zoom == FLUX_FOCUS) {
                              target_zoom = FLUX_MACRO;
                          }
                      }
                  }
                 
                 /* Pass input to terminal if focused */
                 if (current_zoom == FLUX_FOCUS && focused_term != NULL) {
                     /* We assume focused_term is valid if set. 
                        Ideally we check if focused_space matches focused_term->win */
                     if (focused_term->win && focused_term->win->active) {
                         term_handle_key(focused_term, c);
                     }
                 }
            }
        }
        
        /* Capa 0: Deep Void (Premium Dark) */
        
        if ((int)current_zoom == -1) {
             /* Transitioning: Clearing is handled inside draw_zoom_transition (localized) */
        } else {
             framebuffer_clear(0x000000);
        }
        
        if (current_zoom == FLUX_MACRO) {
            draw_constellation();
        } else if (current_zoom == FLUX_FOCUS) {
            draw_focus_mode();
        } else {
             /* Transitioning */
             dirty = draw_zoom_transition();
        }
        
        /* Layer 3: System Notifications (Always on top) */
        draw_notifications();
        
        /* Status Bar (Always Visible) */
        draw_global_status_bar();
        
        /* Cursor: Neural Orb (Reactive) */
        uint32_t cursor_col = (current_zoom == FLUX_FOCUS) ? FLUX_ACCENT_CYAN : FLUX_TEXT_PRIMARY;
        
        /* Glow Effect (Steady, no pulse) */
        /* ⚡ BOLT OPTIMIZATION: Use block rects instead of 81 putpixel calls */
        framebuffer_rect(mouse_x - 3, mouse_y - 1, 7, 3, 0x303030);
        framebuffer_rect(mouse_x - 1, mouse_y - 3, 3, 7, 0x303030);
        framebuffer_rect(mouse_x - 2, mouse_y - 2, 5, 5, 0x404040);
        
        /* Core Orb (Tactile Click Feedback) */
        if (mouse_left_btn) {
            /* Pressed: Smaller, Amber Core */
            framebuffer_rect(mouse_x - 1, mouse_y - 1, 2, 2, FLUX_ACCENT_AMBER);
        } else {
            /* Hover: Standard 4x4 Core */
            framebuffer_rect(mouse_x - 2, mouse_y - 2, 4, 4, cursor_col);
        }
        
        /* Optimization: Flush only dirty regions during transition */
        if ((int)current_zoom == -1) {
            /* 1. Flush Animation Area */
            framebuffer_flush_rect(dirty.x, dirty.y, dirty.w, dirty.h);
            
            /* 2. Flush Mouse Area (approx 40x40 around pointer) */
            int mk_x = mouse_x - 20; if (mk_x < 0) mk_x = 0;
            int mk_y = mouse_y - 20; if (mk_y < 0) mk_y = 0;
            framebuffer_flush_rect(mk_x, mk_y, 40, 40);
            
            /* 3. Flush Status Bar (Assume top 40px and bottom 40px cover it) */
            framebuffer_flush_rect(0, 0, 1024, 40);
            framebuffer_flush_rect(0, 768-40, 1024, 40);
        } else {
            framebuffer_flush(); 
        } 
        /* Layer 4: Keyboard Input Dispatching */
        extern bool keyboard_has_input(void);
        extern char keyboard_getchar(void);
        while (keyboard_has_input()) {
            char c = keyboard_getchar();
            if (current_zoom == FLUX_FOCUS && focused_space == win_browser && browser_url_active) {
                if (c == '\n') {
                    browser_url_active = false;
                    sys_net_fetch(browser_url);
                } else if (c == '\b') {
                    int len = strlen(browser_url);
                    if (len > 0) browser_url[len-1] = 0;
                } else {
                    int len = strlen(browser_url);
                    if (len < (int)sizeof(browser_url)-1) {
                        browser_url[len] = c;
                        browser_url[len+1] = 0;
                    }
                }
            } else if (current_zoom == FLUX_MACRO && c == 27) { /* ESC to reload shell? No, keep focus context */
            }
        }

        /* Smoother loop: Unlocked framerate (Yield to scheduler) */
        task_yield(); 
    }
    
    terminal_initialize(NULL);
}
