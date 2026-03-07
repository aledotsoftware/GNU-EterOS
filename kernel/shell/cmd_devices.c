#include "shell_internal.h"
#include "../../include/keyboard.h"
#include "../../include/mouse.h"
#include "../../include/drivers/disk.h"
#include "../../include/fs/initrd.h"

void cmd_keymap(const char* args) {
    if (!args || !*args) {
        terminal_write_string("Uso: keymap <en|es>\n");
        return;
    }
    if (strcmp(args, "es") == 0) {
        keyboard_set_layout(1);
        terminal_write_string("Distribucion cambiada a: Español (ES)\n");
    } else if (strcmp(args, "en") == 0) {
        keyboard_set_layout(0);
        terminal_write_string("Distribucion cambiada a: Ingles (US)\n");
    } else {
        terminal_write_string("Opcion invalida. Use 'en' o 'es'.\n");
    }
}

void cmd_typematic(const char* args) {
    if (!args || !*args) {
        terminal_write_string("Uso: typematic <delay> <rate>\n");
        terminal_write_string("  delay: 0-3 (0=250ms, 3=1000ms)\n");
        terminal_write_string("  rate:  0-31 (0=30Hz, 31=2Hz)\n");
        return;
    }

    int32_t delay = 0, rate = 0;

    // Parse arguments simply since we don't have strtok easily accessible here
    // Just manual parsing
    const char* p = args;
    while (*p == ' ') p++;

    if (*p >= '0' && *p <= '3') {
        delay = *p - '0';
        p++;
    } else {
        terminal_write_string("Error: Delay debe ser entre 0 y 3.\n");
        return;
    }

    while (*p == ' ') p++;

    if (atoi_s(p, &rate) != 0 || rate < 0 || rate > 31) {
        terminal_write_string("Error: Rate debe ser un numero entre 0 y 31.\n");
        return;
    }

    keyboard_set_typematic((uint8_t)delay, (uint8_t)rate);
    terminal_write_string("Configuracion de teclado actualizada.\n");
}

void cmd_mouse(const char* args) {
    if (!args || !*args) {
        terminal_write_string("Uso: mouse [sens <1-10> | handed <left|right>]\n");
        return;
    }

    if (strncmp(args, "sens ", 5) == 0) {
        int32_t val;
        if (atoi_s(args + 5, &val) == 0 && val >= 1 && val <= 10) {
            mouse_set_sensitivity((uint8_t)val);
            terminal_write_string("Sensibilidad del mouse ajustada.\n");
        } else {
            terminal_write_string("Valor invalido para sens (1-10).\n");
        }
    } else if (strncmp(args, "handed ", 7) == 0) {
        const char* val = args + 7;
        if (strcmp(val, "left") == 0) {
            mouse_set_handedness(true);
            terminal_write_string("Mouse configurado para zurdos.\n");
        } else if (strcmp(val, "right") == 0) {
            mouse_set_handedness(false);
            terminal_write_string("Mouse configurado para diestros.\n");
        } else {
            terminal_write_string("Valor invalido para handed (left o right).\n");
        }
    } else {
        terminal_write_string("Comando mouse desconocido.\n");
    }
}

void cmd_storage(const char* args) {
    (void)args;
    terminal_write_string("\n  [Almacenamiento]\n");

    #include "../../include/nvram.h"
    uint8_t active_id = nvram_get_boot_partition();
    uint8_t passive_id = (active_id == 0) ? 1 : 0;

    terminal_write_string("  Slot Activo:  ");
    terminal_write_string(active_id == 0 ? "A (part0)" : "B (part1)");
    terminal_write_string("\n  Slot Pasivo:  ");
    terminal_write_string(passive_id == 0 ? "A (part0)" : "B (part1)");
    terminal_write_string("\n\n");

    /* Show Initrd status as requested by prompt "espacio libre en el Initrd/VFS" */
    /* Because Initrd is read-only, free space is 0, but we'll show size */
    terminal_write_string("  [VFS / Initrd]\n");
    terminal_write_string("  Initrd cargado en memoria RAM.\n");
    terminal_write_string("  Estado: Solo-Lectura (Read-Only)\n");

    char size_buf[32];
    itoa_s(initrd_get_size(), size_buf, sizeof(size_buf), 10);
    terminal_write_string("  Tamano Total: ");
    terminal_write_string(size_buf);
    terminal_write_string(" bytes\n");

    terminal_write_string("  Espacio Libre: 0 bytes\n");
    terminal_write_string("\n");
}
