#include "shell_internal.h"
#include "../../include/keyboard.h"
#include "../../include/serial.h"
#include "../../include/string.h"
#include "../../include/timer.h"

static void shell_print_prompt(void) {
    terminal_write_colored(SHELL_PROMPT, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_colored("> ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
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
    unsigned int history_nav_idx = 0;

    shell_history_init();

    /* Redirigir output a serial también (para modo sin gráficos) */
    terminal_set_hook(serial_putchar);

    terminal_write_string("\n");
    terminal_write_colored("  Terminal interactiva lista. ",
                          VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("Escribe ");
    terminal_write_colored("help", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_string(" para ver comandos.\n\n");

    shell_print_prompt();

    history_nav_idx = shell_history_count();

    for (;;) {
        char c = 0;
        bool last_cursor_visible = false;

        /* Esperar input de Teclado O Serial */
        while (1) {
            /* Add a visual text input affordance (Blinking Cursor) */
            bool cursor_visible = (timer_get_ticks() % 1000) < 500;
            if (cursor_visible != last_cursor_visible) {
                terminal_putchar(cursor_visible ? '_' : ' ');
                terminal_putchar('\b');
                last_cursor_visible = cursor_visible;
            }

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

        /* Clear the cursor before processing the new character */
        terminal_putchar(' ');
        terminal_putchar('\b');

        if (c == '\n') {
            /* ---- Enter: procesar comando ---- */
            terminal_write_string("\n");
            input[pos] = '\0';

            if (pos > 0) {
                /* Agregar al historial */
                shell_history_add(input);

                /* Ejecutar */
                shell_exec(input);
            }

            pos = 0;
            shell_print_prompt();
            history_nav_idx = shell_history_count();

        } else if (c == '\t') {
            /* ---- Tab: Autocompletar comando ---- */
            if (pos > 0) {
                input[pos] = '\0';
                const char* match = shell_autocomplete(input);
                if (match) {
                    shell_replace_line(input, &pos, match);
                    if (pos < SHELL_MAX_INPUT - 1) {
                        input[pos++] = ' ';
                        terminal_putchar(' ');
                    }
                }
            }

        } else if (c == '\b') {
            /* ---- Backspace ---- */
            if (pos > 0) {
                pos--;
                terminal_putchar('\b');
            }

        } else if ((unsigned char)c == KB_KEY_UP) {
            /* ---- Flecha Arriba: historial anterior ---- */
            unsigned int count = shell_history_count();
            if (count > 0 && history_nav_idx > 0) {
                history_nav_idx--;
                const char* entry = shell_history_get(history_nav_idx);
                if (entry) shell_replace_line(input, &pos, entry);
            }

        } else if ((unsigned char)c == KB_KEY_DOWN) {
            /* ---- Flecha Abajo: historial siguiente ---- */
            unsigned int count = shell_history_count();
            if (history_nav_idx + 1 < count) {
                history_nav_idx++;
                const char* entry = shell_history_get(history_nav_idx);
                if (entry) shell_replace_line(input, &pos, entry);
            } else if (history_nav_idx < count) {
                history_nav_idx = count;
                shell_replace_line(input, &pos, "");
            }

        } else if (c == 3) {
            /* ---- Ctrl+C: cancelar línea ---- */
            terminal_write_colored("^C\n", VGA_COLOR_RED, VGA_COLOR_BLACK);
            pos = 0;
            history_nav_idx = shell_history_count();
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
