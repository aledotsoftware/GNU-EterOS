/**
 * =============================================================================
 * éterOS - Kernel Main
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 * =============================================================================
 * 
 * Punto de entrada principal del kernel de éterOS.
 * Refactorizado para utilizar la Hardware Abstraction Layer (HAL).
 * 
 * =============================================================================
 */

#include "../include/hal.h"
#include "../include/string.h"
#include "../include/shell.h"
#include "../include/pmm.h"
#include "../include/mm.h"
#include "../include/task.h"
#include "../include/boot.h"

#ifdef ARCH_X86_64
#include "../include/net/e1000.h"
#endif

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
static void kernel_halt_loop(void);

/* ========================================================================= */
/* Punto de entrada del kernel                                               */
/* ========================================================================= */

/**
 * kmain - Función principal del kernel de éterOS
 * 
 * Invocada por el bootloader (boot.asm o boot.S).
 * Inicializa el hardware a través de la HAL y lanza el shell.
 */
void __attribute__((section(".text.boot"))) kmain(void) {
    /* ---- 1. Inicializar Hardware Core (GDT, IDT, PIC/GIC) ---- */
    hal_init();

    /* ---- 2. Inicializar Consola (VGA/UART) ---- */
    hal_console_init();

    /* ---- 3. Inicializar Gestores de Memoria ---- */
    pmm_init();         /* Physical Memory Manager */
    hal_mmu_init();     /* MMU / Paging (Virtual Memory) */
    
    /* Inicializar Heap (requiere boot_info para mapas de memoria) */
    boot_info_t* boot_info = (boot_info_t*)BOOT_INFO_ADDR;
    mm_init(boot_info);

    /* ---- 4. Inicializar Red (Solo si hay hardware soportado) ---- */
    hal_console_write("\n");
    hal_console_write("  [NET]  Escaneando dispositivos de red...\n");

    #ifdef ARCH_X86_64
    /* E1000 es específico de x86/PCI por ahora */
    if (e1000_init(NULL) == 0) {
        // Success
    } else {
        hal_console_write("  [NET]  Info: No se detecto tarjeta Intel E1000.\n");
        hal_console_write("         (El sistema continuara sin red)\n");
    }
    #endif

    /* ---- 5. Mostrar Banner ---- */
    kernel_print_banner();

    /* ---- 6. Inicializar Periféricos de Entrada/Salida ---- */

    /* Timer (100 Hz) */
    hal_console_write("  [INIT] Timer System (~100 Hz)\n");
    hal_timer_init(100);

    /* Input (Teclado/Serial) */
    hal_console_write("  [INIT] Input System\n");
    hal_input_init();

    /* ---- 7. Habilitar Interrupciones ---- */
    hal_console_write("  [INIT] Habilitando interrupciones...\n");
    hal_interrupts_enable();

    hal_console_write("[eterOS] Todos los subsistemas inicializados.\n");

    /* ---- 8. Info del Sistema ---- */
    kernel_print_sysinfo();

    /* ---- 9. Inicializar Scheduler ---- */
    hal_console_write("  [INIT] Scheduler Round-Robin\n");
    scheduler_init();

    hal_console_write("  [INIT] Lanzando shell...\n");

    /* ---- 10. Lanzar Shell Interactivo ---- */
    shell_run();

    /* Si el shell retorna, detener sistema */
    kernel_halt_loop();
}

/* ========================================================================= */
/* Funciones internas                                                        */
/* ========================================================================= */

static void kernel_print_banner(void) {
    hal_console_write("\n");
    hal_console_write("          _             ___  ____  \n");
    hal_console_write("     ___ | |_ ___  _ _ / _ \\/ ___| \n");
    hal_console_write("    / _ \\| __/ _ \\| '_| | | \\___ \\ \n");
    hal_console_write("   |  __/| ||  __/| | | |_| |___) |\n");
    hal_console_write("    \\___| \\__\\___||_|  \\___/|____/ \n");
    hal_console_write("\n");
    
    hal_console_write("  ========================================\n");
    hal_console_write("    Version ");
    
    char buf[32];
    itoa_s(ETEROS_VERSION_MAJOR, buf, sizeof(buf), 10);
    hal_console_write(buf);
    hal_console_write(".");
    itoa_s(ETEROS_VERSION_MINOR, buf, sizeof(buf), 10);
    hal_console_write(buf);
    hal_console_write(".");
    itoa_s(ETEROS_VERSION_PATCH, buf, sizeof(buf), 10);
    hal_console_write(buf);
    
    hal_console_write(" (\"");
    hal_console_write(ETEROS_CODENAME);
    hal_console_write("\")\n");

    hal_console_write("    (c) 2026 Tudex Networks\n");
    hal_console_write("  ========================================\n\n");
}

static void kernel_print_sysinfo(void) {
    hal_console_write("  [INFO] Arquitectura: ");
    hal_console_write(ETEROS_ARCH_NAME);
    hal_console_write("\n");

    hal_console_write("  [INFO] Word Size: ");
    char buf[16];
    itoa_s(ETEROS_WORD_SIZE, buf, sizeof(buf), 10);
    hal_console_write(buf);
    hal_console_write("-bit\n");
}

static void kernel_halt_loop(void) {
    for (;;) {
        hal_cpu_halt();
    }
}
