/**
 * =============================================================================
 * éterOS - Kernel Main
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 * =============================================================================
 * 
 * Punto de entrada principal del kernel de éterOS.
 * Esta función es invocada por el bootloader después de configurar
 * el Modo Largo (64-bit) con paginación activa.
 * 
 * Responsabilidades iniciales:
 *   1. Inicializar el terminal VGA
 *   2. Inicializar el puerto serie para depuración
 *   3. Mostrar información de inicio del sistema
 *   4. Entrar en el bucle principal del kernel
 * 
 * =============================================================================
 */

#include "../include/types.h"
#include "../include/vga.h"
#include "../include/string.h"
#include "../include/serial.h"
#include "../include/io.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/keyboard.h"
#include "../include/shell.h"
#include "../include/timer.h"

/* ========================================================================= */
/* Constantes del Sistema                                                    */
/* ========================================================================= */
#define ETEROS_VERSION_MAJOR    0
#define ETEROS_VERSION_MINOR    1
#define ETEROS_VERSION_PATCH    0
#define ETEROS_CODENAME         "Genesis"

/* ========================================================================= */
/* Prototipos internos                                                       */
/* ========================================================================= */
static void kernel_print_banner(void);
static void kernel_print_sysinfo(void);
static void kernel_halt(void);

/* ========================================================================= */
/* Punto de entrada del kernel                                               */
/* ========================================================================= */

/**
 * kmain - Función principal del kernel de éterOS
 * 
 * Invocada por el bootloader desde Long Mode (64-bit).
 * Inicializa los subsistemas principales y entra en el loop del kernel.
 */
void kmain(void) {
    /* ---- 1. Inicializar el terminal VGA ---- */
    terminal_initialize();

    /* ---- 2. Inicializar el puerto serie para depuración ---- */
    int serial_status = serial_init();

    /* ---- 3. Mostrar banner de éterOS ---- */
    kernel_print_banner();

    /* ---- Log de depuración por serial ---- */
    if (serial_status == 0) {
        serial_write_string("[eterOS] Puerto serie COM1 inicializado.\n");
        serial_write_string("[eterOS] Kernel cargado en modo de 64 bits.\n");
        serial_write_string("[eterOS] (c) 2026 Tudex Networks\n");
    }

    /* ---- 4. Inicializar el PIC (remapear IRQs) ---- */
    terminal_write_colored("  [INIT] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("PIC remapeado (IRQ 32-47)\n");
    pic_init();

    /* ---- 5. Inicializar la IDT ---- */
    terminal_write_colored("  [INIT] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("IDT cargada (256 vectores)\n");
    idt_init();

    /* ---- 6. Inicializar el teclado PS/2 ---- */
    terminal_write_colored("  [INIT] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("Teclado PS/2 (IRQ1)\n");
    keyboard_init();
    pic_unmask_irq(1);   /* Habilitar IRQ1 (teclado) */

    /* ---- 7. Inicializar el Timer (PIT) ---- */
    terminal_write_colored("  [INIT] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("Timer PIT (~100 Hz)\n");
    timer_init();
    pic_unmask_irq(0);   /* Habilitar IRQ0 (timer) */

    /* ---- 8. Habilitar interrupciones ---- */
    terminal_write_colored("  [INIT] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("Interrupciones habilitadas (STI)\n");
    __asm__ volatile ("sti");

    serial_write_string("[eterOS] Todos los subsistemas inicializados.\n");
    serial_write_string("[eterOS] Lanzando shell interactivo.\n");

    /* ---- 8. Información del sistema ---- */
    kernel_print_sysinfo();

    /* ---- 9. Lanzar shell interactivo ---- */
    shell_run();  /* Nunca retorna */

    /* Si por alguna razón retorna, halt */
    kernel_halt();
}

/* ========================================================================= */
/* Funciones internas                                                        */
/* ========================================================================= */

/**
 * Muestra el banner de inicio de éterOS con arte ASCII y colores.
 */
static void kernel_print_banner(void) {
    terminal_write_string("\n");
    
    /* Logo de éterOS en arte ASCII */
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
    
    /* Línea separadora */
    terminal_write_colored(
        "  ========================================\n",
        VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    
    /* Versión y codename */
    terminal_write_string("    Version ");
    
    char version_str[32];
    itoa_s(ETEROS_VERSION_MAJOR, version_str, sizeof(version_str), 10);
    terminal_write_colored(version_str, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string(".");
    itoa_s(ETEROS_VERSION_MINOR, version_str, sizeof(version_str), 10);
    terminal_write_colored(version_str, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string(".");
    itoa_s(ETEROS_VERSION_PATCH, version_str, sizeof(version_str), 10);
    terminal_write_colored(version_str, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    terminal_write_string(" (\"");
    terminal_write_colored(ETEROS_CODENAME, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_string("\")\n");

    /* Copyright */
    terminal_write_string("    ");
    terminal_write_colored("(c) 2026 Tudex Networks",
                          VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    terminal_write_string("\n");
    
    terminal_write_colored(
        "  ========================================\n",
        VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_string("\n");
}

/**
 * Muestra información básica del sistema.
 */
static void kernel_print_sysinfo(void) {
    terminal_write_colored("  [INFO] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("Arquitectura: x86_64 (Long Mode)\n");

    terminal_write_colored("  [INFO] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("Video: VGA Modo Texto (80x25)\n");

    terminal_write_colored("  [INFO] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("Serial: COM1 @ 38400 baud\n");

    terminal_write_colored("  [INFO] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("Paginacion: Identity mapped (4 MB)\n");
}

/**
 * Detiene la CPU en un loop infinito de HLT.
 * HLT pone la CPU en estado de bajo consumo hasta la próxima interrupción.
 */
static void kernel_halt(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
