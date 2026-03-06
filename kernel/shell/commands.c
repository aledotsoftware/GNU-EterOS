#include "shell_internal.h"
#include "../../include/santitravel.h"
#include "../../include/sysmon.h"
#include "../../include/serial.h"

// Command table structure
typedef void (*cmd_handler_t)(const char* args);

typedef struct {
    const char*   name;
    const char*   description;
    cmd_handler_t handler;
} shell_command_t;

// Apps table structure
typedef struct {
    const char*   name;
    const char*   description;
    const char*   version;
    void        (*run)(void);
} app_entry_t;

void gui_demo_run(void);

// Command table
static const shell_command_t commands[] = {
    { "help",     "Muestra esta lista de comandos",              cmd_help    },
    { "usermode", "Entrar a modo usuario (Ring 3)",              cmd_usermode},
    { "clear",    "Limpia la pantalla",                          cmd_clear   },
    { "version",  "Muestra la version del sistema",              cmd_version },
    { "sysinfo",  "Informacion del sistema",                     cmd_sysinfo },
    { "echo",     "Repite el texto ingresado",                   cmd_echo    },
    { "about",    "Acerca de eterOS",                            cmd_about   },
    { "mem",      "Informacion basica de memoria",               cmd_mem     },
    { "lspci",    "Lista dispositivos PCI",                      cmd_lspci   },
    { "net",      "Informacion de red (MAC/IP)",                 cmd_net     },
    { "dhcp",     "Obtener IP via DHCP",                         cmd_dhcp    },
    { "wget",     "Descargar archivo via HTTP",                  cmd_wget    },
    { "ota",      "Actualizaciones de sistema (OTA)",            cmd_ota     },
    { "uptime",   "Tiempo desde el arranque",                    cmd_uptime  },
    { "ps",       "Lista tareas activas",                         cmd_ps      },
    { "kill",     "Matar tarea (uso: kill <pid>)",                cmd_kill    },
    { "demo",     "Crear tarea de fondo (test scheduler)",        cmd_demo    },
    { "date",     "Muestra fecha y hora (UTC / Argentina)",      cmd_date    },
    { "reboot",   "Reinicia el sistema",                         cmd_reboot  },
    { "halt",     "Detiene la CPU",                              cmd_halt    },
    { "test_compositor", "Test Basic Compositor",                cmd_test_compositor },
    { "gui_demo", "Test Batch Drawing",                          (cmd_handler_t)gui_demo_run },
    { "keymap",   "Cambiar distribucion (uso: keymap <en|es>)",  cmd_keymap  },
    { "typematic","Ajuste de teclado (uso: typematic <d> <r>)",  cmd_typematic },
    { "mouse",    "Configurar mouse (sens / handed)",            cmd_mouse   },
    { "storage",  "Estado de almacenamiento / A/B Slots",        cmd_storage },
    { "time",     "Ver la hora actual del sistema",              cmd_time    },
    { "timezone", "Configurar zona horaria (offset UTC)",        cmd_timezone},
    { "ntp",      "Sincronizar RTC via red (NTP)",               cmd_ntp     },
};

#define NUM_COMMANDS  (sizeof(commands) / sizeof(commands[0]))

static const app_entry_t apps[] = {
    { "santitravel", "Aventuras con Santi",     "1.1", santitravel_run },
    { "etermon",     "Monitor del Sistema",     "1.0", sysmon_run      },
};

#define NUM_APPS  (sizeof(apps) / sizeof(apps[0]))

const char* match_command(const char* input, const char* cmd) {
    while (*cmd) {
        if (*input != *cmd) return (void*)0;
        input++;
        cmd++;
    }
    if (*input == '\0' || *input == ' ') {
        while (*input == ' ') input++;
        return input;
    }
    return (void*)0;
}

const char* shell_autocomplete(const char* prefix) {
    if (!prefix || !*prefix) return (void*)0;
    size_t len = strlen(prefix);
    const char* match = (void*)0;
    int matches = 0;

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strncmp(prefix, commands[i].name, len) == 0) {
            match = commands[i].name;
            matches++;
        }
    }

    for (size_t i = 0; i < NUM_APPS; i++) {
        if (strncmp(prefix, apps[i].name, len) == 0) {
            match = apps[i].name;
            matches++;
        }
    }

    return (matches == 1) ? match : (void*)0;
}

void cmd_help(const char* args) {
    (void)args;
    terminal_write_string("\n");
    terminal_write_colored("  Comandos del sistema:\n\n",
                          VGA_COLOR_YELLOW, VGA_COLOR_BLACK);

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        terminal_write_colored("    ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        terminal_write_colored(commands[i].name,
                              VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

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
    terminal_write_colored("  Atajos de teclado:\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("    [Tab]      Autocompletar comando\n");
    terminal_write_string("    [Up/Down]  Navegar historial\n");
    terminal_write_string("    [Ctrl+L]   Limpiar pantalla\n");
    terminal_write_string("    [Ctrl+C]   Cancelar linea\n");
    terminal_write_string("\n");
}

int shell_exec(char* input) {
    if (!input || !*input) return 0;

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        const char* args = match_command(input, commands[i].name);
        if (args) {
            commands[i].handler(args);
            return 0;
        }
    }

    for (size_t i = 0; i < NUM_APPS; i++) {
        const char* args = match_command(input, apps[i].name);
        if (args) {
            serial_write_string("[eterOS] Ejecutando app: ");
            serial_write_string(apps[i].name);
            serial_write_string("\n");
            apps[i].run();
            return 0;
        }
    }

    terminal_write_colored("  Comando no encontrado: ", VGA_COLOR_RED, VGA_COLOR_BLACK);
    terminal_write_string(input);
    terminal_write_string("\n  Escribe ");
    terminal_write_colored("help", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_string(" para ver comandos disponibles.\n");

    return -1;
}
