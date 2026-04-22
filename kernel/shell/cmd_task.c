#include "shell_internal.h"
#include "../../include/task.h"
#include "../../include/serial.h"
#include "../../include/string.h"

static const char* task_state_str(task_state_t s) {
    switch (s) {
        case TASK_RUNNING:  return "RUNNING";
        case TASK_READY:    return "READY";
        case TASK_BLOCKED:  return "BLOCKED";
        case TASK_SLEEPING: return "SLEEPING";
        case TASK_STOPPED:  return "STOPPED";
        case TASK_DEAD:     return "DEAD";
        default:            return "???";
    }
}

void cmd_ps(const char* args) {
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

void cmd_kill(const char* args) {
    if (!args || *args == '\0') {
        terminal_write_string("Uso: kill <pid>\n");
        return;
    }

    /* Skip spaces */
    while (*args == ' ') args++;

    char pid_buf[32];
    size_t i = 0;
    while (*args >= '0' && *args <= '9' && i < 31) {
        pid_buf[i++] = *args++;
    }
    pid_buf[i] = '\0';

    if (i == 0) {
        terminal_write_colored("Error: PID invalido o es kernel (PID 0).\n", VGA_COLOR_RED, VGA_COLOR_BLACK);
        return;
    }

    int32_t pid_val;
    if (atoi_s(pid_buf, &pid_val) != 0) {
        terminal_write_colored("Error: Formato de PID invalido (overflow).\n", VGA_COLOR_RED, VGA_COLOR_BLACK);
        return;
    }

    if (pid_val <= 0) {
        terminal_write_colored("Error: PID invalido o es kernel (PID 0).\n", VGA_COLOR_RED, VGA_COLOR_BLACK);
        return;
    }

    uint32_t pid = (uint32_t)pid_val;

    if (task_kill(pid) == 0) {
        terminal_write_colored("Tarea terminada.\n", VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    } else {
        terminal_write_colored("Error: No se pudo terminar la tarea (PID no existe?).\n", VGA_COLOR_RED, VGA_COLOR_BLACK);
    }
}

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

void cmd_demo(const char* args) {
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
