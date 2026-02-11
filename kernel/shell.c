/**
 * éterOS - Shell Interactivo
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Terminal de comandos del kernel.
 * Modular: usa solo las APIs públicas de VGA, keyboard y string.
 * Para agregar un comando nuevo, solo hay que:
 *   1. Crear la función cmd_nombre()
 *   2. Agregar una entrada en la tabla 'commands[]'
 *
 * El shell es extensible por diseño — los comandos se resuelven
 * por tabla, no por una cadena de if/else.
 */

#include "../include/shell.h"
#include "../include/vga.h"
#include "../include/keyboard.h"
#include "../include/string.h"
#include "../include/serial.h"
#include "../include/io.h"
#include "../include/santitravel.h"
#include "../include/sysmon.h"

/* ========================================================================= */
/* Constantes del sistema                                                    */
/* ========================================================================= */
#define ETEROS_VERSION      "0.1.0"
#define ETEROS_CODENAME     "Genesis"

/* ========================================================================= */
/* Tipos                                                                     */
/* ========================================================================= */

/** Función de comando: recibe los argumentos como string. */
typedef void (*cmd_handler_t)(const char* args);

/** Entrada en la tabla de comandos. */
typedef struct {
    const char*   name;          /* Nombre del comando */
    const char*   description;   /* Descripción para help */
    cmd_handler_t handler;       /* Función que ejecuta el comando */
} shell_command_t;

/* ========================================================================= */
/* Prototipos de comandos                                                    */
/* ========================================================================= */
static void cmd_help(const char* args);
static void cmd_clear(const char* args);
static void cmd_version(const char* args);
static void cmd_sysinfo(const char* args);
static void cmd_echo(const char* args);
static void cmd_about(const char* args);
static void cmd_reboot(const char* args);
static void cmd_halt(const char* args);
static void cmd_mem(const char* args);

/* ========================================================================= */
/* Tabla de comandos (extensible — solo agregar entradas)                    */
/* ========================================================================= */
static const shell_command_t commands[] = {
    { "help",     "Muestra esta lista de comandos",              cmd_help    },
    { "clear",    "Limpia la pantalla",                          cmd_clear   },
    { "version",  "Muestra la version del sistema",              cmd_version },
    { "sysinfo",  "Informacion del sistema",                     cmd_sysinfo },
    { "echo",     "Repite el texto ingresado",                   cmd_echo    },
    { "about",    "Acerca de eterOS",                            cmd_about   },
    { "mem",      "Informacion basica de memoria",               cmd_mem     },
    { "reboot",   "Reinicia el sistema",                         cmd_reboot  },
    { "halt",     "Detiene la CPU",                              cmd_halt    },
};

#define NUM_COMMANDS  (sizeof(commands) / sizeof(commands[0]))

/* ========================================================================= */
/* Registro de Aplicaciones (separado de comandos del kernel)                 */
/* ========================================================================= */

/** Entrada en el registro de aplicaciones. */
typedef struct {
    const char*   name;          /* Nombre de la app */
    const char*   description;   /* Descripción */
    const char*   version;       /* Versión */
    void        (*run)(void);    /* Entry point de la app */
} app_entry_t;

static const app_entry_t apps[] = {
    { "santitravel", "Aventuras con Santi",     "1.1", santitravel_run },
    { "etermon",     "Monitor del Sistema",     "1.0", sysmon_run      },
};

#define NUM_APPS  (sizeof(apps) / sizeof(apps[0]))

/* ========================================================================= */
/* Helpers internos                                                          */
/* ========================================================================= */

/**
 * Compara un string con el inicio de otro (para matchear comandos con args).
 * Retorna la posición después del match, o NULL si no coincide.
 */
static const char* match_command(const char* input, const char* cmd) {
    while (*cmd) {
        if (*input != *cmd) return (void*)0;
        input++;
        cmd++;
    }
    /* El comando debe terminar en espacio, null, o fin */
    if (*input == '\0' || *input == ' ') {
        /* Saltar espacios después del comando */
        while (*input == ' ') input++;
        return input;
    }
    return (void*)0;
}

/**
 * Imprime el prompt del shell.
 */
static void shell_print_prompt(void) {
    terminal_write_colored(SHELL_PROMPT, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_colored("> ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
}

/* ========================================================================= */
/* Implementación de Comandos                                                */
/* ========================================================================= */

static void cmd_help(const char* args) {
    (void)args;
    terminal_write_string("\n");
    terminal_write_colored("  Comandos del sistema:\n\n",
                          VGA_COLOR_YELLOW, VGA_COLOR_BLACK);

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        terminal_write_colored("    ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        terminal_write_colored(commands[i].name,
                              VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

        /* Padding para alinear descripciones */
        size_t name_len = strlen(commands[i].name);
        size_t pad = (name_len < 12) ? 12 - name_len : 1;
        while (pad--) terminal_write_string(" ");

        terminal_write_string(commands[i].description);
        terminal_write_string("\n");
    }

    terminal_write_string("\n");
    terminal_write_colored("  Aplicaciones instaladas:\n\n",
                          VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);

    for (size_t i = 0; i < NUM_APPS; i++) {
        terminal_write_colored("    ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        terminal_write_colored(apps[i].name,
                              VGA_COLOR_YELLOW, VGA_COLOR_BLACK);

        size_t name_len = strlen(apps[i].name);
        size_t pad = (name_len < 12) ? 12 - name_len : 1;
        while (pad--) terminal_write_string(" ");

        terminal_write_string(apps[i].description);
        terminal_write_colored(" v", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        terminal_write_colored(apps[i].version, VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        terminal_write_string("\n");
    }

    terminal_write_string("\n");
}

static void cmd_clear(const char* args) {
    (void)args;
    terminal_initialize();
}

static void cmd_version(const char* args) {
    (void)args;
    terminal_write_string("\n  eterOS v");
    terminal_write_colored(ETEROS_VERSION, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string(" (\"");
    terminal_write_colored(ETEROS_CODENAME, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_string("\")\n\n");
}

static void cmd_sysinfo(const char* args) {
    (void)args;
    terminal_write_string("\n");
    terminal_write_colored("  Arquitectura: ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("x86_64 (Long Mode)\n");
    terminal_write_colored("  Video:        ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("VGA Texto 80x25\n");
    terminal_write_colored("  Serial:       ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("COM1 @ 38400 baud\n");
    terminal_write_colored("  Teclado:      ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("PS/2 (IRQ1)\n");
    terminal_write_colored("  Paginacion:   ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("Identity mapped 8 MB\n");
    terminal_write_colored("  Kernel size:  ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("< 10 KB\n");
    terminal_write_string("\n");
}

static void cmd_echo(const char* args) {
    terminal_write_string(args);
    terminal_write_string("\n");
}

static void cmd_about(const char* args) {
    (void)args;
    terminal_write_string("\n");
    terminal_write_colored(
        "          _             ___  ____  \n",
        VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(
        "     ___ | |_ ___  _ _ / _ \\/ ___| \n",
        VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(
        "    / _ \\| __/ _ \\| '_| | | \\___ \\ \n",
        VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(
        "   |  __/| ||  __/| | | |_| |___) |\n",
        VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(
        "    \\___| \\__\\___||_|  \\___/|____/ \n",
        VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);

    terminal_write_string("\n");
    terminal_write_string("  El Sistema Operativo de la Nueva Era\n");
    terminal_write_colored("  (c) 2026 Tudex Networks\n",
                          VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    terminal_write_string("\n");
    terminal_write_string("  Kernel hibrido bare-metal para x86_64.\n");
    terminal_write_string("  Disenado para ser ligero, modular y extensible.\n");
    terminal_write_string("\n");
}

static void cmd_mem(const char* args) {
    (void)args;
    terminal_write_string("\n");
    terminal_write_colored("  Mapa de memoria:\n\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_string("    0x00000 - 0x07BFF  IVT / BDA\n");
    terminal_write_string("    0x07C00 - 0x07DFF  Stage 1 (MBR)\n");
    terminal_write_string("    0x07E00 - 0x09DFF  Stage 2\n");
    terminal_write_string("    0x10000 - 0x1FFFF  Ether-Core (Kernel)\n");
    terminal_write_string("    0x70000 - 0x72FFF  Page Tables (12 KB)\n");
    terminal_write_string("    0x90000            Stack Top\n");
    terminal_write_string("    0xB8000 - 0xBFFFF  VGA Buffer\n");
    terminal_write_string("\n");
}

static void cmd_reboot(const char* args) {
    (void)args;
    terminal_write_colored("  Reiniciando...\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    serial_write_string("[eterOS] Reboot solicitado.\n");

    /* Triple fault: cargar IDT vacía y disparar interrupción */
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) null_idt = { 0, 0 };

    __asm__ volatile ("lidt %0" : : "m"(null_idt));
    __asm__ volatile ("int $0x03");

    /* Si por alguna razón no funciona, usar reset vía 8042 */
    outb(0x64, 0xFE);

    for (;;) { __asm__ volatile ("hlt"); }
}

static void cmd_halt(const char* args) {
    (void)args;
    terminal_write_colored("  CPU detenida (HLT).\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    serial_write_string("[eterOS] Halt solicitado.\n");
    __asm__ volatile ("cli");
    for (;;) { __asm__ volatile ("hlt"); }
}

/* ========================================================================= */
/* Loop principal del Shell                                                  */
/* ========================================================================= */

void shell_run(void) {
    char input[SHELL_MAX_INPUT];
    size_t pos = 0;

    terminal_write_string("\n");
    terminal_write_colored("  Terminal interactiva lista. ",
                          VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("Escribe ");
    terminal_write_colored("help", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_string(" para ver comandos.\n\n");

    shell_print_prompt();

    for (;;) {
        char c = keyboard_getchar();

        if (c == '\n') {
            /* ---- Enter: procesar comando ---- */
            terminal_write_string("\n");
            input[pos] = '\0';

            if (pos > 0) {
                /* Log al serial */
                serial_write_string("[shell] > ");
                serial_write_string(input);
                serial_write_string("\n");

                /* Buscar comando en la tabla */
                bool found = false;
                for (size_t i = 0; i < NUM_COMMANDS; i++) {
                    const char* args = match_command(input, commands[i].name);
                    if (args) {
                        commands[i].handler(args);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    /* Buscar en registro de aplicaciones */
                    for (size_t i = 0; i < NUM_APPS; i++) {
                        if (match_command(input, apps[i].name)) {
                            serial_write_string("[eterOS] Ejecutando app: ");
                            serial_write_string(apps[i].name);
                            serial_write_string("\n");
                            apps[i].run();
                            terminal_initialize();
                            found = true;
                            break;
                        }
                    }
                }

                if (!found) {
                    terminal_write_colored("  Comando no encontrado: ",
                                          VGA_COLOR_RED, VGA_COLOR_BLACK);
                    terminal_write_string(input);
                    terminal_write_string("\n  Escribe ");
                    terminal_write_colored("help",
                                          VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
                    terminal_write_string(" para ver comandos disponibles.\n");
                }
            }

            pos = 0;
            shell_print_prompt();

        } else if (c == '\b') {
            /* ---- Backspace ---- */
            if (pos > 0) {
                pos--;
                terminal_putchar('\b');
            }

        } else if (c == 3) {
            /* ---- Ctrl+C: cancelar línea ---- */
            terminal_write_colored("^C\n", VGA_COLOR_RED, VGA_COLOR_BLACK);
            pos = 0;
            shell_print_prompt();

        } else if (c == 12) {
            /* ---- Ctrl+L: clear screen ---- */
            terminal_initialize();
            pos = 0;
            shell_print_prompt();

        } else if (c >= 32 && c < 127) {
            /* ---- Carácter imprimible ---- */
            if (pos < SHELL_MAX_INPUT - 1) {
                input[pos++] = c;
                terminal_putchar(c);
            }
        }
        /* Caracteres especiales no manejados se ignoran */
    }
}
