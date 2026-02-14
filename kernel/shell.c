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
#include "../include/idt.h"
#include "../include/pci.h"
#include "../include/mm.h"
#include "../include/pmm.h"
#include "../include/timer.h"
#include "../include/net/e1000.h"
#include "../include/net/dhcp.h"
#include "../include/task.h"
#include "../include/gui_demo.h"
#include "../include/rtc.h"
#include "../include/apps/wget.h"

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
static void cmd_lspci(const char* args);
static void cmd_net(const char* args);
static void cmd_dhcp(const char* args);
static void cmd_uptime(const char* args);
static void cmd_ps(const char* args);
static void cmd_kill(const char* args);
static void cmd_demo(const char* args);
static void cmd_date(const char* args);
static void cmd_wget(const char* args);

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
    { "lspci",    "Lista dispositivos PCI",                      cmd_lspci   },
    { "net",      "Informacion de red (MAC)",                    cmd_net     },
    { "dhcp",     "Obtener IP via DHCP",                         cmd_dhcp    },
    { "uptime",   "Tiempo desde el arranque",                    cmd_uptime  },
    { "ps",       "Lista tareas activas",                         cmd_ps      },
    { "kill",     "Matar tarea (uso: kill <pid>)",                cmd_kill    },
    { "demo",     "Crear tarea de fondo (test scheduler)",        cmd_demo    },
    { "date",     "Muestra fecha y hora (UTC / Argentina)",      cmd_date    },
    { "wget",     "Descarga archivo HTTP (uso: wget url)",       cmd_wget    },
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
    { "flux",        "Flux UI Multitask Demo",  "0.1", gui_demo_run    },
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
    terminal_initialize(NULL);
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
    char buf[32];
    terminal_write_string("\n");
    terminal_write_colored("  Arquitectura: ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("x86_64 (Long Mode)\n");
    terminal_write_colored("  Video:        ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("VGA Texto 80x25\n");
    terminal_write_colored("  RAM Total:    ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    itoa_s(pmm_get_total_ram() / (1024*1024), buf, sizeof(buf), 10);
    terminal_write_string(buf);
    terminal_write_string(" MB\n");
    terminal_write_colored("  RAM Libre:    ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    itoa_s(pmm_get_free_ram() / (1024*1024), buf, sizeof(buf), 10);
    terminal_write_string(buf);
    terminal_write_string(" MB\n");
    terminal_write_colored("  Teclado:      ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("PS/2 (IRQ1, Set 1, Extended)\n");
    terminal_write_colored("  Timer:        ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("PIT @ 100 Hz\n");
    terminal_write_colored("  Serial:       ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("COM1 @ 38400 baud\n");
    terminal_write_colored("  Uptime:       ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    uint32_t secs = timer_get_uptime_seconds();
    itoa_s(secs / 60, buf, sizeof(buf), 10);
    terminal_write_string(buf);
    terminal_write_string("m ");
    itoa_s(secs % 60, buf, sizeof(buf), 10);
    terminal_write_string(buf);
    terminal_write_string("s\n");
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
    terminal_write_colored("  Mapa de Memoria del Sistema:\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_string("    0x00000 - 0x07BFF  IVT / BDA\n");
    terminal_write_string("    0x07C00 - 0x07DFF  Stage 1 (MBR)\n");
    terminal_write_string("    0x07E00 - 0x09DFF  Stage 2\n");
    terminal_write_string("    0x10000 - 0x1FFFF  Ether-Core (Kernel)\n");
    terminal_write_string("    0x70000 - 0x72FFF  Page Tables\n");
    terminal_write_string("    0x90000            Stack Top\n");
    terminal_write_string("    0xB8000 - 0xBFFFF  VGA Buffer\n");
    terminal_write_string("    0x400000 - 0xC00000 Kernel Heap (8 MB)\n");

    terminal_write_string("\n");
    terminal_write_colored("  Estado del Heap (Kernel):\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    
    char buffer[32];
    size_t total = mm_get_total_memory();
    size_t used = mm_get_used_memory();
    
    terminal_write_string("    Total:  ");
    itoa_s((int64_t)total, buffer, sizeof(buffer), 10);
    terminal_write_colored(buffer, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string(" bytes\n");

    terminal_write_string("    Usado:  ");
    itoa_s((int64_t)used, buffer, sizeof(buffer), 10);
    terminal_write_colored(buffer, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string(" bytes\n");
    
    terminal_write_string("    Libre:  ");
    itoa_s((int64_t)(total - used), buffer, sizeof(buffer), 10);
    terminal_write_colored(buffer, VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    terminal_write_string(" bytes\n");
    
    terminal_write_string("\n");
}

static void cmd_net(const char* args) {
    (void)args;
    terminal_write_string("\n");
    terminal_write_colored("  [Interfaz eth0]\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_string("    Driver:  ");
    terminal_write_colored("Intel PRO/1000 (e1000)\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    
    uint8_t* mac = e1000_get_mac();
    
    terminal_write_string("    MAC:     ");
    for (int i = 0; i < 6; i++) {
        uint8_t val = mac[i];
        char hex[3];
        
        char h = (val >> 4) & 0xF;
        char l = val & 0xF;
        
        hex[0] = (h < 10) ? '0' + h : 'A' + (h - 10);
        hex[1] = (l < 10) ? '0' + l : 'A' + (l - 10);
        hex[2] = 0;
        
        terminal_write_string(hex);
        if (i < 5) terminal_write_string(":");
    }
    terminal_write_string("\n\n");
}

static void cmd_dhcp(const char* args) {
    (void)args;
    terminal_write_string("\n");
    terminal_write_colored("  [DHCP Client]\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    dhcp_discover();
    terminal_write_string("\n");
}

static void cmd_reboot(const char* args) {
    (void)args;
    terminal_write_colored("  Reiniciando...\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    serial_write_string("[eterOS] Reboot solicitado.\n");

    /* Triple fault: cargar IDT vacía y disparar interrupción */
    struct idt_ptr null_idt = { 0, 0 };

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

static void cmd_lspci(const char* args) {
    (void)args;
    pci_scan_bus();
}

static void cmd_uptime(const char* args) {
    (void)args;
    char buf[32];
    uint32_t total_secs = timer_get_uptime_seconds();
    uint32_t hours = total_secs / 3600;
    uint32_t mins  = (total_secs % 3600) / 60;
    uint32_t secs  = total_secs % 60;
    
    terminal_write_string("\n  Sistema activo: ");
    if (hours > 0) {
        itoa_s(hours, buf, sizeof(buf), 10);
        terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        terminal_write_string("h ");
    }
    itoa_s(mins, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("m ");
    itoa_s(secs, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("s\n\n");
}
static void print_time(rtc_time_t* t, const char* label) {
    char buf[16];
    terminal_write_string(label);

    // YYYY-MM-DD
    itoa_s(t->year, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("-");

    if (t->month < 10) terminal_write_string("0");
    itoa_s(t->month, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("-");

    if (t->day < 10) terminal_write_string("0");
    itoa_s(t->day, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    terminal_write_string(" ");

    // HH:MM:SS
    if (t->hours < 10) terminal_write_string("0");
    itoa_s(t->hours, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string(":");

    if (t->minutes < 10) terminal_write_string("0");
    itoa_s(t->minutes, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string(":");

    if (t->seconds < 10) terminal_write_string("0");
    itoa_s(t->seconds, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    terminal_write_string("\n");
}

static void cmd_date(const char* args) {
    (void)args;
    rtc_time_t utc, local;

    rtc_get_time(&utc);
    rtc_to_argentina(&utc, &local);

    terminal_write_string("\n");
    print_time(&utc,   "  UTC:       ");
    print_time(&local, "  Argentina: ");
    terminal_write_string("\n");
}

static void cmd_wget(const char* args) {
    if (!args || *args == '\0') {
        terminal_write_string("Uso: wget <url>\n");
        terminal_write_string("Ejemplo: wget google.com\n");
        return;
    }

    /* Skip spaces */
    while (*args == ' ') args++;

    wget_run(args);
}

/* ========================================================================= */
/* Comando `ps` — Listar tareas del scheduler                                */
/* ========================================================================= */

static const char* task_state_str(task_state_t s) {
    switch (s) {
        case TASK_RUNNING:  return "RUNNING";
        case TASK_READY:    return "READY";
        case TASK_BLOCKED:  return "BLOCKED";
        case TASK_DEAD:     return "DEAD";
        default:            return "???";
    }
}

static void cmd_ps(const char* args) {
    (void)args;
    char buf[32];

    terminal_write_string("\n");
    terminal_write_colored("  PID  Estado    Nombre\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored("  ---  --------  ----------------\n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);

    int max_tasks = task_get_max();
    int active_count = 0;
    uint32_t current_id = task_get_current()->id;

    for (int i = 0; i < max_tasks; i++) {
        task_t* t = task_get_at(i);
        if (t && t->state != 0 && t->state != TASK_DEAD) {
            active_count++;
            
            /* PID */
            terminal_write_string("  ");
            itoa_s(t->id, buf, sizeof(buf), 10);
            terminal_write_string(buf);
            
            /* Padding for PID */
            int pid_len = strlen(buf);
            for(int p=0; p < (4 - pid_len); p++) terminal_write_string(" ");

            /* Estado */
            terminal_write_colored(task_state_str(t->state), 
                                  (t->id == current_id) ? VGA_COLOR_LIGHT_GREEN : VGA_COLOR_WHITE, 
                                  VGA_COLOR_BLACK);
            terminal_write_string("  ");

            /* Nombre */
            terminal_write_string(t->name);
            if (t->id == current_id) {
                terminal_write_colored(" *", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            }
            terminal_write_string("\n");
        }
    }

    terminal_write_string("\n  Total tareas activas: ");
    itoa_s(active_count, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("\n\n");
}

static void cmd_kill(const char* args) {
    if (!args || *args == '\0') {
        terminal_write_string("Uso: kill <pid>\n");
        return;
    }

    /* Skip spaces */
    while (*args == ' ') args++;

    /* Simple atoi implementation */
    uint32_t pid = 0;
    const char* ptr = args;
    while (*ptr >= '0' && *ptr <= '9') {
        pid = pid * 10 + (*ptr - '0');
        ptr++;
    }

    if (pid == 0) {
        terminal_write_colored("Error: PID invalido o es kernel (PID 0).\n", VGA_COLOR_RED, VGA_COLOR_BLACK);
        return;
    }

    if (task_kill(pid) == 0) {
        terminal_write_colored("Tarea terminada.\n", VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    } else {
        terminal_write_colored("Error: No se pudo terminar la tarea (PID no existe?).\n", VGA_COLOR_RED, VGA_COLOR_BLACK);
    }
}

/* ========================================================================= */
/* Comando `demo` — Crear tarea de fondo para demostrar multitarea           */
/* ========================================================================= */

static volatile int demo_task_counter = 0;

static void demo_background_task(void) {
    /* Esta tarea corre en background, escribiendo al serial cada ~1 segundo */
    char buf[32];
    for (int i = 0; i < 10; i++) {
        demo_task_counter++;
        serial_write_string("[DEMO] Tick #");
        itoa_s(demo_task_counter, buf, sizeof(buf), 10);
        serial_write_string(buf);
        serial_write_string(" desde tarea background\n");
        /* Esperar ~1 segundo (100 ticks a 100Hz) usando yield */
        for (volatile int j = 0; j < 500000; j++) { }
        task_yield();
    }
    serial_write_string("[DEMO] Tarea de fondo terminada!\n");
    /* task_exit() se llama automáticamente al retornar */
}

static void cmd_demo(const char* args) {
    (void)args;
    int id = task_create("demo-bg", demo_background_task);
    if (id >= 0) {
        char buf[32];
        terminal_write_string("\n  Tarea de fondo creada (PID ");
        itoa_s(id, buf, sizeof(buf), 10);
        terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        terminal_write_string(")\n");
        terminal_write_string("  Revisa serial (COM1) para ver output del scheduler.\n\n");
    } else {
        terminal_write_colored("\n  Error al crear tarea.\n\n", VGA_COLOR_RED, VGA_COLOR_BLACK);
    }
}

/* ========================================================================= */
/* Loop principal del Shell                                                  */
/* =========================================================================*/

#define HISTORY_SIZE    8
#define HISTORY_LEN     SHELL_MAX_INPUT

static char history[HISTORY_SIZE][HISTORY_LEN];
static unsigned int  history_count = 0;
static unsigned int  history_idx   = 0;

/* Ejecuta un comando (sin prompt, ni history management) */
int shell_exec(char* input) {
    if (!input || !*input) return 0;

    /* 1. Buscar en comandos internos */
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        const char* args = match_command(input, commands[i].name);
        if (args) {
            /* Ejecutar handler */
            commands[i].handler(args);
            return 0; /* Exito */
        }
    }

    /* 2. Buscar en aplicaciones */
    for (size_t i = 0; i < NUM_APPS; i++) {
        const char* args = match_command(input, apps[i].name);
        if (args) {
            serial_write_string("[eterOS] Ejecutando app: ");
            serial_write_string(apps[i].name);
            serial_write_string("\n");
            apps[i].run();
            /* Restaurar terminal (opcional, depende de la app) */
            // terminal_initialize(NULL); 
            return 0;
        }
    }

    /* 3. No encontrado */
    terminal_write_colored("  Comando no encontrado: ", VGA_COLOR_RED, VGA_COLOR_BLACK);
    terminal_write_string(input);
    terminal_write_string("\n  Escribe ");
    terminal_write_colored("help", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_string(" para ver comandos disponibles.\n");
    
    return -1;
}

static void shell_replace_line(char* input, size_t* pos, const char* new_text) {
    /* Borrar línea actual en pantalla */
    while (*pos > 0) {
        terminal_putchar('\b');
        (*pos)--;
    }
    /* Escribir nueva línea */
    strlcpy(input, new_text, SHELL_MAX_INPUT);
    *pos = strlen(input);
    terminal_write_string(input);
}

void shell_run(void) {
    char input[SHELL_MAX_INPUT];
    size_t pos = 0;

    /* Redirigir output a serial también (para modo sin gráficos) */
    terminal_set_hook(serial_putchar);

    terminal_write_string("\n");
    terminal_write_colored("  Terminal interactiva lista. ",
                          VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("Escribe ");
    terminal_write_colored("help", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_string(" para ver comandos.\n\n");

    shell_print_prompt();

    for (;;) {
        char c = 0;

        /* Esperar input de Teclado O Serial */
        while (1) {
            if (keyboard_has_input()) {
                c = keyboard_getchar();
                break;
            }
            if (serial_received()) {
                c = serial_read();
                /* Mapear Enter de Serial (\r) a \n */
                if (c == '\r') c = '\n';
                /* Mapear Backspace de Serial (DEL=127) a \b */
                if (c == 127) c = '\b';
                break;
            }
            /* Dormir hasta la próxima interrupción */
            __asm__ volatile("hlt"); 
        }

        if (c == '\n') {
            /* ---- Enter: procesar comando ---- */
            terminal_write_string("\n");
            input[pos] = '\0';

            if (pos > 0) {
                /* Agregar al historial */
                if (history_count == 0 || strcmp(history[(history_count - 1) % HISTORY_SIZE], input) != 0) {
                     strlcpy(history[history_count % HISTORY_SIZE], input, SHELL_MAX_INPUT);
                     history_count++;
                }
                history_idx = history_count;
                
                /* Ejecutar */
                shell_exec(input);
            }
            
            pos = 0;
            shell_print_prompt();

        } else if (c == '\b') {
            /* ---- Backspace ---- */
            if (pos > 0) {
                pos--;
                terminal_putchar('\b');
            }

        } else if ((unsigned char)c == KEY_UP) {
            /* ---- Flecha Arriba: historial anterior ---- */
            if (history_count > 0 && history_idx > 0) {
                history_idx--;
                unsigned int idx = history_idx % HISTORY_SIZE;
                shell_replace_line(input, &pos, history[idx]);
            }

        } else if ((unsigned char)c == KEY_DOWN) {
            /* ---- Flecha Abajo: historial siguiente ---- */
            if (history_idx + 1 < history_count) {
                history_idx++;
                unsigned int idx = history_idx % HISTORY_SIZE;
                shell_replace_line(input, &pos, history[idx]);
            } else if (history_idx < history_count) {
                history_idx = history_count;
                shell_replace_line(input, &pos, "");
            }

        } else if (c == 3) {
            /* ---- Ctrl+C: cancelar línea ---- */
            terminal_write_colored("^C\n", VGA_COLOR_RED, VGA_COLOR_BLACK);
            pos = 0;
            history_idx = history_count;
            shell_print_prompt();

        } else if (c == 12) {
            /* ---- Ctrl+L: clear screen ---- */
            terminal_initialize(NULL);
            pos = 0;
            shell_print_prompt();

        } else if (c >= 32 && c < 127) {
            /* ---- Carácter imprimible ---- */
            if (pos < SHELL_MAX_INPUT - 1) {
                input[pos++] = c;
                terminal_putchar(c);
            }
        }
        /* Otras teclas especiales (LEFT, RIGHT, etc.) se ignoran por ahora */
    }
}
