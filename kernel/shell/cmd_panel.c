#include "shell_internal.h"
#include "../../include/keyboard.h"
#include "../../include/net/defs.h"
#include "../../include/mouse.h"
#include "../../include/task.h"
#include "../../include/rtc.h"
#include "../../include/nvram.h"
#include "../../include/net/nic.h"
#include "../../include/fs/initrd.h"
#include "../../include/vga.h"

static volatile bool panel_running = false;
static volatile int panel_mouse_x = 40;
static volatile int panel_mouse_y = 12;
static volatile int panel_mouse_acc_x = 0;
static volatile int panel_mouse_acc_y = 0;
static volatile bool panel_mouse_clicked = false;
static volatile bool panel_mouse_moved = false;

static void panel_mouse_callback(int8_t dx, int8_t dy, uint8_t buttons) {
    if (!panel_running) return;

    panel_mouse_acc_x += dx;
    panel_mouse_acc_y += dy;

    // Adjust scale for text mode (mouse delta is usually high res, so scale down)
    int move_x = panel_mouse_acc_x / 4;
    int move_y = panel_mouse_acc_y / 4;

    if (move_x != 0 || move_y != 0) {
        int new_x = panel_mouse_x + move_x;
        int new_y = panel_mouse_y + move_y;

        if (new_x < 0) new_x = 0;
        if (new_x >= 80) new_x = 79;
        if (new_y < 0) new_y = 0;
        if (new_y >= 25) new_y = 24;

        if (new_x != panel_mouse_x || new_y != panel_mouse_y) {
            panel_mouse_x = new_x;
            panel_mouse_y = new_y;
            panel_mouse_moved = true;
        }

        panel_mouse_acc_x -= move_x * 4;
        panel_mouse_acc_y -= move_y * 4;
    }

    if (buttons & 1) { // Left click
        panel_mouse_clicked = true;
    }
}

static void draw_panel_menu(void) {
    cmd_clear("");
    terminal_write_string("\n");
    terminal_write_colored("  +----------------------------------------------+\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored("  | Panel de Control - eterOS v0.2.0 Genesis SMP |\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored("  +----------------------------------------------+\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

    // Items are roughly at lines 4, 5, 6, 7, 8, 9, 10
    terminal_write_string("    [1] Configurar Teclado (Layout & Typematic)\n"); // Y=4
    terminal_write_string("    [2] Configurar Mouse (Sensibilidad & Botones)\n"); // Y=5
    terminal_write_string("    [3] Estado de Almacenamiento (Slots & Initrd)\n"); // Y=6
    terminal_write_string("    [4] Configurar Tiempo (Zona Horaria & NTP)\n"); // Y=7
    terminal_write_string("    [5] Usuarios y Seguridad (Auto-login & Permisos)\n"); // Y=8
    terminal_write_string("    [6] Estado de Red (IP & DNS)\n"); // Y=9
    terminal_write_string("    [7] Salir del Panel de Control\n"); // Y=10
    terminal_write_string("\n  Use teclas [1-7] o haga click para seleccionar.\n");

    terminal_set_cursor(panel_mouse_x, panel_mouse_y);
}

static void wait_for_enter(void) {
    terminal_write_string("\n  [Presione Enter o Haga Click para continuar]");
    while (1) {
        if (keyboard_has_input()) {
            char c = keyboard_getchar();
            if (c == '\n' || c == '\r' || c == KB_KEY_ESCAPE) {
                break;
            }
        }
        if (panel_mouse_clicked) {
            panel_mouse_clicked = false;
            break;
        }
        __asm__ volatile("cli");
        if (!keyboard_has_input() && !panel_mouse_clicked && !panel_mouse_moved) {
            __asm__ volatile("sti; hlt");
        } else {
            __asm__ volatile("sti");
        }
    }
}

static void panel_keyboard(void) {
    cmd_clear("");
    terminal_write_string("\n  -- Teclado --\n");
    terminal_write_string("  1. Layout: Espanol (es)\n"); // Y=2
    terminal_write_string("  2. Layout: Ingles (en)\n"); // Y=3
    terminal_write_string("  3. Ajustar Typematic Rate\n"); // Y=4
    terminal_write_string("  Elija [1-3] o presione ESC para volver.\n");

    char c = 0;
    while (1) {
        terminal_set_cursor(panel_mouse_x, panel_mouse_y);

        if (keyboard_has_input()) {
            c = keyboard_getchar();
            if (c == '1' || c == '2' || c == '3' || c == KB_KEY_ESCAPE) break;
        }

        if (panel_mouse_clicked) {
            panel_mouse_clicked = false;
            if (panel_mouse_x >= 2 && panel_mouse_x <= 40) {
                if (panel_mouse_y == 2) c = '1';
                else if (panel_mouse_y == 3) c = '2';
                else if (panel_mouse_y == 4) c = '3';
                else c = KB_KEY_ESCAPE; // Click outside returns
            } else {
                c = KB_KEY_ESCAPE;
            }
            if (c) break;
        }
        __asm__ volatile("cli");
        if (!keyboard_has_input() && !panel_mouse_clicked && !panel_mouse_moved) {
            __asm__ volatile("sti; hlt");
        } else {
            __asm__ volatile("sti");
        }
    }

    terminal_write_string("\n");

    if (c == '1') {
        keyboard_set_layout(1);
        terminal_write_string("  Layout cambiado a Espanol.\n");
    } else if (c == '2') {
        keyboard_set_layout(0);
        terminal_write_string("  Layout cambiado a Ingles.\n");
    } else if (c == '3') {
        terminal_write_string("  (Nota: Delay=1 (500ms), Rate=15 (10Hz))\n");
        keyboard_set_typematic(1, 15);
        terminal_write_string("  Typematic Rate ajustado por defecto.\n");
    }
    if (c != KB_KEY_ESCAPE) wait_for_enter();
}

static void panel_mouse_cfg(void) {
    cmd_clear("");
    terminal_write_string("\n  -- Mouse --\n");
    terminal_write_string("  1. Sensibilidad: Baja (Multiplicador 1)\n"); // Y=2
    terminal_write_string("  2. Sensibilidad: Media (Multiplicador 5)\n"); // Y=3
    terminal_write_string("  3. Sensibilidad: Alta (Multiplicador 10)\n"); // Y=4
    terminal_write_string("  4. Modo Diestro (Normal)\n"); // Y=5
    terminal_write_string("  5. Modo Zurdo (Invertido)\n"); // Y=6
    terminal_write_string("  Elija [1-5] o haga click para seleccionar, o ESC para volver.\n");

    char c = 0;
    while (1) {
        terminal_set_cursor(panel_mouse_x, panel_mouse_y);

        if (keyboard_has_input()) {
            c = keyboard_getchar();
            if (c >= '1' && c <= '5') break;
            if (c == KB_KEY_ESCAPE) break;
        }

        if (panel_mouse_clicked) {
            panel_mouse_clicked = false;
            if (panel_mouse_x >= 2 && panel_mouse_x <= 40) {
                if (panel_mouse_y >= 2 && panel_mouse_y <= 6) {
                    c = '1' + (panel_mouse_y - 2);
                    break;
                } else {
                    c = KB_KEY_ESCAPE;
                    break;
                }
            } else {
                c = KB_KEY_ESCAPE;
                break;
            }
        }
        __asm__ volatile("cli");
        if (!keyboard_has_input() && !panel_mouse_clicked && !panel_mouse_moved) {
            __asm__ volatile("sti; hlt");
        } else {
            __asm__ volatile("sti");
        }
    }

    terminal_write_string("\n");

    if (c == '1') {
        mouse_set_sensitivity(1);
        terminal_write_string("  Sensibilidad Baja.\n");
    } else if (c == '2') {
        mouse_set_sensitivity(5);
        terminal_write_string("  Sensibilidad Media.\n");
    } else if (c == '3') {
        mouse_set_sensitivity(10);
        terminal_write_string("  Sensibilidad Alta.\n");
    } else if (c == '4') {
        mouse_set_handedness(false);
        terminal_write_string("  Modo Diestro activado.\n");
    } else if (c == '5') {
        mouse_set_handedness(true);
        terminal_write_string("  Modo Zurdo activado.\n");
    }
    if (c != KB_KEY_ESCAPE) wait_for_enter();
}

static void panel_storage(void) {
    cmd_clear("");
    terminal_write_string("\n  -- Almacenamiento --\n");

    uint8_t active_id = nvram_get_boot_partition();
    uint8_t passive_id = (active_id == 0) ? 1 : 0;

    terminal_write_string("\n  [A/B Slots - NVRAM]\n");
    terminal_write_string("  Slot Activo:  ");
    terminal_write_string(active_id == 0 ? "A (part0)" : "B (part1)");
    terminal_write_string("\n  Slot Pasivo:  ");
    terminal_write_string(passive_id == 0 ? "A (part0)" : "B (part1)");
    terminal_write_string("\n\n");

    terminal_write_string("  [Estado de Discos y VFS]\n");
    terminal_write_string("  Sistema de Archivos Principal: Initrd (RAM Disk)\n");
    terminal_write_string("  Estado de Salud: OK (Solo-Lectura)\n");

    char size_buf[32];
    itoa_s(initrd_get_size(), size_buf, sizeof(size_buf), 10);
    terminal_write_string("  Tamano Total Initrd: ");
    terminal_write_string(size_buf);
    terminal_write_string(" bytes\n");

    terminal_write_string("  Espacio Libre: 0 bytes\n");
    terminal_write_string("\n");

    wait_for_enter();
}

static void panel_time(void) {
    cmd_clear("");
    terminal_write_string("\n  -- Tiempo --\n");
    cmd_time("");
    terminal_write_string("  1. Sincronizar via red (NTP / Resolucion DNS)\n"); // Y = varies based on cmd_time output length, usually 5-6 lines
    // Let's print fixed options and read keyboard for simplicity to not hardcode dynamic Y positions here
    terminal_write_string("  2. Zona Horaria: Argentina (UTC-3)\n");
    terminal_write_string("  3. Zona Horaria: UTC (0)\n");
    terminal_write_string("  4. Sincronizacion Manual Avanzada (HH:MM:SS)\n");
    terminal_write_string("  Elija [1-4] usando el teclado numerico (o ESC para cancelar y volver).\n");

    char c = 0;
    while (1) {
        terminal_set_cursor(panel_mouse_x, panel_mouse_y);

        if (keyboard_has_input()) {
            c = keyboard_getchar();
            if (c >= '1' && c <= '4') break;
            if (c == KB_KEY_ESCAPE) break;
        }

        if (panel_mouse_clicked) {
            panel_mouse_clicked = false;
            // The output of cmd_time is about 6 lines long.
            // Menu starts at roughly y=10.
            // "1. Sincronizar via red"
            // "2. Zona Horaria"
            // "3. Zona Horaria UTC"
            // "4. Sincronizacion Manual"
            if (panel_mouse_x >= 2 && panel_mouse_x <= 50) {
                // Approximate mapping based on typical cmd_time length
                if (panel_mouse_y >= 8 && panel_mouse_y <= 13) {
                    c = '1' + (panel_mouse_y - 8);
                    if (c > '4') c = '4'; // Clamping
                    break;
                } else {
                    c = KB_KEY_ESCAPE;
                    break;
                }
            } else {
                c = KB_KEY_ESCAPE;
                break;
            }
        }
        __asm__ volatile("cli");
        if (!keyboard_has_input() && !panel_mouse_clicked && !panel_mouse_moved) {
            __asm__ volatile("sti; hlt");
        } else {
            __asm__ volatile("sti");
        }
    }

    terminal_write_string("\n");

    if (c == '1') {
        cmd_ntp("");
    } else if (c == '2') {
        rtc_set_timezone(-3);
        terminal_write_string("  Zona horaria configurada a Argentina (UTC-3).\n");
    } else if (c == '3') {
        rtc_set_timezone(0);
        terminal_write_string("  Zona horaria configurada a UTC.\n");
    } else if (c == '4') {
        rtc_time_t t;
        rtc_get_time(&t);

        terminal_write_string("\n  [Sincronizacion Manual RTC]\n");
        char buf[8];

        // Very basic prompt for hours
        terminal_write_string("  Hora (0-23) [00]: ");
        int idx = 0;
        memset(buf, 0, sizeof(buf));
        while (1) {
            if (keyboard_has_input()) {
                char k = keyboard_getchar();
                if (k == '\n' || k == '\r') break;
                if (k == '\b' && idx > 0) {
                    idx--;
                    buf[idx] = '\0';
                    terminal_write_string("\b \b");
                } else if (k >= '0' && k <= '9' && idx < 2) {
                    buf[idx++] = k;
                    terminal_putchar(k);
                }
            }
            __asm__ volatile("cli");
            if (!keyboard_has_input() && !panel_mouse_clicked && !panel_mouse_moved) {
                __asm__ volatile("sti; hlt");
            } else {
                __asm__ volatile("sti");
            }
        }
        terminal_write_string("\n");
        int32_t val = 0;
        if (idx > 0) {
            atoi_s(buf, &val);
            if (val >= 0 && val <= 23) t.hours = val;
        }

        // Prompt for minutes
        terminal_write_string("  Minuto (0-59) [00]: ");
        idx = 0;
        memset(buf, 0, sizeof(buf));
        while (1) {
            if (keyboard_has_input()) {
                char k = keyboard_getchar();
                if (k == '\n' || k == '\r') break;
                if (k == '\b' && idx > 0) {
                    idx--;
                    buf[idx] = '\0';
                    terminal_write_string("\b \b");
                } else if (k >= '0' && k <= '9' && idx < 2) {
                    buf[idx++] = k;
                    terminal_putchar(k);
                }
            }
            __asm__ volatile("cli");
            if (!keyboard_has_input() && !panel_mouse_clicked && !panel_mouse_moved) {
                __asm__ volatile("sti; hlt");
            } else {
                __asm__ volatile("sti");
            }
        }
        terminal_write_string("\n");
        if (idx > 0) {
            atoi_s(buf, &val);
            if (val >= 0 && val <= 59) t.minutes = val;
        }

        // Prompt for seconds
        terminal_write_string("  Segundo (0-59) [00]: ");
        idx = 0;
        memset(buf, 0, sizeof(buf));
        while (1) {
            if (keyboard_has_input()) {
                char k = keyboard_getchar();
                if (k == '\n' || k == '\r') break;
                if (k == '\b' && idx > 0) {
                    idx--;
                    buf[idx] = '\0';
                    terminal_write_string("\b \b");
                } else if (k >= '0' && k <= '9' && idx < 2) {
                    buf[idx++] = k;
                    terminal_putchar(k);
                }
            }
            __asm__ volatile("cli");
            if (!keyboard_has_input() && !panel_mouse_clicked && !panel_mouse_moved) {
                __asm__ volatile("sti; hlt");
            } else {
                __asm__ volatile("sti");
            }
        }
        terminal_write_string("\n");
        if (idx > 0) {
            atoi_s(buf, &val);
            if (val >= 0 && val <= 59) t.seconds = val;
        }

        rtc_set_time(&t);
        terminal_write_string("  Hora manual guardada en el RTC.\n");
    }
    if (c != KB_KEY_ESCAPE) wait_for_enter();
}

void cmd_panel(const char* args) {
    (void)args;

    panel_running = true;
    panel_mouse_clicked = false;
    panel_mouse_moved = true; // force initial draw

    mouse_set_callback(panel_mouse_callback);

    draw_panel_menu();

    while (panel_running) {
        char opt = 0;

        if (keyboard_has_input()) {
            opt = keyboard_getchar();
            if (opt == KB_KEY_ESCAPE || opt == 'q') {
                opt = '7'; // Exit
            }
        }

        if (panel_mouse_clicked) {
            panel_mouse_clicked = false;
            if (panel_mouse_x >= 2 && panel_mouse_x <= 50) {
                if (panel_mouse_y == 4) opt = '1';
                else if (panel_mouse_y == 5) opt = '2';
                else if (panel_mouse_y == 6) opt = '3';
                else if (panel_mouse_y == 7) opt = '4';
                else if (panel_mouse_y == 8) opt = '5';
                else if (panel_mouse_y == 9) opt = '6';
                else if (panel_mouse_y == 10) opt = '7';
            }
        }

        if (panel_mouse_moved) {
            panel_mouse_moved = false;
            terminal_set_cursor(panel_mouse_x, panel_mouse_y);
        }

        if (opt != 0) {
            if (opt == '1') {
                panel_keyboard();
                draw_panel_menu();
            } else if (opt == '2') {
                panel_mouse_cfg();
                draw_panel_menu();
            } else if (opt == '3') {
                panel_storage();
                draw_panel_menu();
            } else if (opt == '4') {
                panel_time();
                draw_panel_menu();
            } else if (opt == '5') {
                cmd_clear("");
                terminal_write_string("\n  -- Usuarios y Seguridad --\n");
                terminal_write_string("  1. Habilitar Auto-login (autologin on)\n"); // y=2
                terminal_write_string("  2. Deshabilitar Auto-login (autologin off)\n"); // y=3
                terminal_write_string("  3. Crear Usuario (add)\n"); // y=4
                terminal_write_string("  4. Eliminar Usuario (del)\n"); // y=5
                terminal_write_string("  5. Cambiar Contrasena (passwd)\n"); // y=6
                terminal_write_string("\n  Elija [1-5] o ESC para volver.\n");
                char c = 0;
                while (1) {
                    terminal_set_cursor(panel_mouse_x, panel_mouse_y);
                    if (keyboard_has_input()) {
                        c = keyboard_getchar();
                        if ((c >= '1' && c <= '5') || c == KB_KEY_ESCAPE) break;
                    }
                    if (panel_mouse_clicked) {
                        panel_mouse_clicked = false;
                        if (panel_mouse_x >= 2 && panel_mouse_x <= 50) {
                            if (panel_mouse_y >= 2 && panel_mouse_y <= 6) {
                                c = '1' + (panel_mouse_y - 2);
                                break;
                            } else {
                                c = KB_KEY_ESCAPE;
                                break;
                            }
                        } else {
                            c = KB_KEY_ESCAPE;
                            break;
                        }
                    }
                    __asm__ volatile("cli");
                    if (!keyboard_has_input() && !panel_mouse_clicked && !panel_mouse_moved) __asm__ volatile("sti; hlt");
                    else __asm__ volatile("sti");
                }
                terminal_write_string("\n");
                if (c == '1') cmd_user("autologin on");
                else if (c == '2') cmd_user("autologin off");
                else if (c == '3') {
                    terminal_write_string("  Ejecute en la linea de comandos: user add <usuario> <password>\n");
                }
                else if (c == '4') {
                    terminal_write_string("  Ejecute en la linea de comandos: user del <usuario>\n");
                }
                else if (c == '5') {
                    terminal_write_string("  Ejecute en la linea de comandos: user passwd <usuario> <nuevo_password>\n");
                }
                if (c != KB_KEY_ESCAPE) wait_for_enter();
                draw_panel_menu();
            } else if (opt == '6') {
                if (!current_nic) {
                    cmd_clear("");
                    terminal_write_string("\n  -- Red y Conectividad --\n");
                    terminal_write_string("  Error: Adaptador de red no activo o no detectado.\n");
                    wait_for_enter();
                    draw_panel_menu();
                    continue;
                }
                cmd_clear("");
                terminal_write_string("\n  -- Red y Conectividad --\n");
                cmd_net("");
                // cmd_net outputs ~4-5 lines, so options start roughly at y=7 or y=8
                terminal_write_string("\n  1. Renovar DHCP\n");
                terminal_write_string("  2. Probar conexion (wget tudexgames.com)\n");
                terminal_write_string("\n  Elija [1-2] o ESC para volver.\n");
                char c = 0;
                while (1) {
                    terminal_set_cursor(panel_mouse_x, panel_mouse_y);
                    if (keyboard_has_input()) {
                        c = keyboard_getchar();
                        if ((c >= '1' && c <= '2') || c == KB_KEY_ESCAPE) break;
                    }
                    if (panel_mouse_clicked) {
                        panel_mouse_clicked = false;
                        if (panel_mouse_x >= 2 && panel_mouse_x <= 50) {
                            if (panel_mouse_y >= 7 && panel_mouse_y <= 9) { // rough estimate
                                c = '1' + (panel_mouse_y - 7);
                                if (c > '2') c = '2';
                                break;
                            } else {
                                c = KB_KEY_ESCAPE;
                                break;
                            }
                        } else {
                            c = KB_KEY_ESCAPE;
                            break;
                        }
                    }
                    __asm__ volatile("cli");
                    if (!keyboard_has_input() && !panel_mouse_clicked && !panel_mouse_moved) {
                        __asm__ volatile("sti; hlt");
                    } else {
                        __asm__ volatile("sti");
                    }
                }
                terminal_write_string("\n");
                if (c == '1') {
                    cmd_dhcp("");
                } else if (c == '2') {
                    if (!network_ready) {
                        terminal_write_string("  Error: Red no lista. Ejecute DHCP primero.\n");
                    } else {
                        cmd_wget("tudexgames.com");
                    }
                }
                if (c != KB_KEY_ESCAPE) wait_for_enter();
                draw_panel_menu();
            } else if (opt == '7') {
                terminal_write_string("\nSaliendo del Panel de Control...\n");
                panel_running = false;
                break;
            }
        }

        __asm__ volatile("cli");
        if (!keyboard_has_input() && !panel_mouse_clicked && !panel_mouse_moved && panel_running) {
            __asm__ volatile("sti; hlt");
        } else {
            __asm__ volatile("sti");
        }
    }

    mouse_set_callback((void*)0); // Remove callback
}
