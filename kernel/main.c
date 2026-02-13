/**
 * =============================================================================
 * éterOS - Kernel Main
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 * =============================================================================
 * 
 * Punto de entrada principal del kernel de éterOS.
 * 
 * =============================================================================
 */

#include <types.h>
#include <hal.h>
#include <string.h>
#include <io.h>
#include <boot.h>
#include <shell.h>
#include <mm.h>

/* Forward declarations for non-HAL kernel services */
void pmm_init(void);
void vmm_init(void);
int e1000_init(void* pci_dev);
void scheduler_init(void);

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
 * Invocada por el bootloader o startup code.
 * Inicializa el HAL y los subsistemas del kernel.
 */
void __attribute__((section(".text.boot"))) kmain(void) {
    /* ---- 1. Inicializar Hardware Abstraction Layer (HAL) ---- */
    /* Esto configura relojes, interrupciones, consola, timer, etc. */
    hal_init();
    
    hal_console_write("\n");
    hal_debug_write("[eterOS] HAL Inicializada.\n");

    /* ---- 2. Obtener Info del Bootloader (si aplica) ---- */
    /* En x86, esto está en 0xA000. En ARM, puede ser NULL o DTB. */
    #if defined(ARCH_X86_64)
        boot_info_t* boot_info = (boot_info_t*)BOOT_INFO_ADDR;
    #else
        void* boot_info = NULL;
    #endif

    /* ---- 3. Inicializar Memory Managers (Solo Tier 2+) ---- */
    #if ETEROS_TIER >= 2
        /* Memory Management Unit (Paging/MPU) */
        /* Nota: pmm_init y vmm_init suelen ser específicos de la arquitectura
           o requieren boot_info. Por ahora los mantenemos aquí si hay MMU. */

        #if defined(ARCH_X86_64)
            /* x86 specific PMM init (uses E820) */
            pmm_init();
        #endif

        #if ETEROS_HAS_MMU
            hal_mmu_init(); /* Configura paginación virtual */
        #endif

        /* Heap Manager (Generic) */
        mm_init(boot_info);
    #else
        /* Tier 1 (Microcontroller): Use simple static heap or similar if needed */
        /* For now, assume simple stack usage or static buffers */
    #endif

    /* ---- 4. Inicializar Red (Solo Tier 3+) ---- */
    #if ETEROS_TIER >= 3
        hal_console_write("\n  [NET]  Escaneando dispositivos de red...\n");
        /* Attempt to init E1000 (Generic Driver but requires PCI) */
        if (e1000_init(NULL) == 0) {
            // Success
        } else {
            hal_console_write("  [NET]  Info: No se detecto tarjeta de red compatible.\n");
            hal_console_write("         (El sistema continuara sin red)\n");
        }
    #endif

    /* ---- 5. Mostrar banner de éterOS ---- */
    kernel_print_banner();

    /* ---- Log de depuración ---- */
    hal_debug_write("[eterOS] Kernel cargado.\n");
    hal_debug_write("[eterOS] (c) 2026 Tudex Networks\n");

    /* ---- 6. Información del sistema ---- */
    kernel_print_sysinfo();

    /* ---- 7. Inicializar Scheduler ---- */
    hal_console_write("  [INIT] Scheduler Round-Robin\n");
    scheduler_init();

    hal_console_write("  [INIT] Lanzando shell...\n");

    /* ---- 8. Lanzar shell interactivo ---- */
    shell_run();  /* Nunca retorna (tarea 0 del kernel) */

    /* Si por alguna razón retorna, halt */
    kernel_halt();
}

/* ========================================================================= */
/* Funciones internas                                                        */
/* ========================================================================= */

/**
 * Muestra el banner de inicio de éterOS.
 */
static void kernel_print_banner(void) {
    hal_console_write("\n");
    
    /* Logo de éterOS en arte ASCII (Monocromático para portabilidad) */
    hal_console_write("          _             ___  ____  \n");
    hal_console_write("     ___ | |_ ___  _ _ / _ \\/ ___| \n");
    hal_console_write("    / _ \\| __/ _ \\| '_| | | \\___ \\ \n");
    hal_console_write("   |  __/| ||  __/| | | |_| |___) |\n");
    hal_console_write("    \\___| \\__\\___||_|  \\___/|____/ \n");

    hal_console_write("\n");
    hal_console_write("  ========================================\n");
    
    /* Versión y codename */
    hal_console_write("    Version ");
    
    char version_str[32];
    itoa_s(ETEROS_VERSION_MAJOR, version_str, sizeof(version_str), 10);
    hal_console_write(version_str);
    hal_console_write(".");
    itoa_s(ETEROS_VERSION_MINOR, version_str, sizeof(version_str), 10);
    hal_console_write(version_str);
    hal_console_write(".");
    itoa_s(ETEROS_VERSION_PATCH, version_str, sizeof(version_str), 10);
    hal_console_write(version_str);
    
    hal_console_write(" (\"");
    hal_console_write(ETEROS_CODENAME);
    hal_console_write("\")\n");

    /* Copyright */
    hal_console_write("    (c) 2026 Tudex Networks\n");
    
    hal_console_write("  ========================================\n");
    hal_console_write("\n");
}

/**
 * Muestra información básica del sistema.
 */
static void kernel_print_sysinfo(void) {
    hal_console_write("  [INFO] Arquitectura: ");
    hal_console_write(ETEROS_ARCH_NAME);
    hal_console_write("\n");

    hal_console_write("  [INFO] Board/Target: Generic\n");
}

/**
 * Detiene la CPU.
 */
static void kernel_halt(void) {
    for (;;) {
        hal_cpu_halt();
    }
}
