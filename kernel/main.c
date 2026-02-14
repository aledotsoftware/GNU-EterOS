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
#include <mouse.h>
#include <shell.h>
#include <mm.h>
#include <gui_demo.h>
#include <fs/initrd.h>
#include <fs/vfs.h>
#include <vga.h>
#include <task.h>
#include <net/socket.h>

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/timeouts.h"
#include "lwip/ip_addr.h"
#include "netif/ethernet.h"
#include <net/e1000.h>
#include "ethernetif.h"


/* Forward declarations for non-HAL kernel services */
void pmm_init(void);
void vmm_init(void);

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

static void network_task(void) {
    while(1) {
        net_poll();
        task_yield();
    }
}

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

    /* ---- 1.5 Mostrar Logo de Arranque ---- */
    /* Mostramos el logo de eterOS mientras cargamos el resto */
    gui_draw_boot_logo();

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

        /* ---- 3.5 Inicializar Initrd ---- */
        #if defined(ARCH_X86_64)
        if (boot_info && boot_info->initrd_addr != 0) {
            fs_root = initialise_initrd(boot_info->initrd_addr, boot_info->initrd_size);
            if (fs_root) {
                hal_console_write("[VFS] Initrd mounted at /\n");

                /* List files using VFS */
                struct dirent *node = 0;
                int i = 0;
                while ((node = readdir_fs(fs_root, i)) != 0) {
                    hal_console_write("  [VFS] Found file: ");
                    hal_console_write(node->name);
                    hal_console_write("\n");
                    i++;
                }
            }
        }
        #endif
    #else
        /* Tier 1 (Microcontroller): Use simple static heap or similar if needed */
        /* For now, assume simple stack usage or static buffers */
    #endif

    /* ---- 4. Inicializar Red ---- */
    hal_console_write("\n  [NET]  Escaneando dispositivos de red...\n");
    /* Attempt to init E1000 (Generic Driver but requires PCI) */
    if (e1000_init(NULL) == 0) {
        hal_console_write("  [NET]  Hardware inicializado.\n");
        net_init();
        extern void dhcp_discover(void);
        dhcp_discover();
        task_create("Network", network_task);
    } else {
        hal_console_write("  [NET]  Info: No se detecto tarjeta de red compatible.\n");
        hal_console_write("         (El sistema continuara sin red)\n");
    }

    /* ---- 5. Mostrar banner de éterOS ---- */
    kernel_print_banner();

    /* ---- Log de depuración ---- */
    hal_debug_write("[eterOS] Kernel cargado.\n");
    hal_debug_write("[eterOS] (c) 2026 Tudex Networks\n");

    /* ---- 6. Información del sistema ---- */
    kernel_print_sysinfo();

    /* ---- 6.5 Inicializar Mouse (PS/2) ---- */
    /* Nota: Se inicializa después de la IDT/PIC pero antes de la GUI */
    mouse_init();

    /* ---- 7. Inicializar Scheduler ---- */
    hal_console_write("  [INIT] Scheduler Round-Robin\n");
    scheduler_init();

    /* ---- 7.5 Lanzar Test de Espacio de Usuario ---- */
    hal_console_write("  [INIT] Lanzando User Mode Test...\n");
    extern void user_loader_entry(void);
    task_create("UserLoader", user_loader_entry);

    /* ---- 8. Lanzar Entorno de Escritorio (Flux UI) como Tarea Separada ---- */
    hal_console_write("  [INIT] Lanzando Flux UI...\n");
    
    /* Crear la tarea para la GUI (será PID 1) */
    int gui_pid = task_create("FluxUI", gui_demo_run);
    
    hal_interrupts_enable(); 

    /* La Tarea 0 (Kernel) espera a que la GUI termine o sea 'matada' 
       para evitar conflictos de teclado y recursos. */
    while (gui_pid >= 0) {
        task_t* t = task_get_by_id(gui_pid); 
        /* Si la tarea ya no existe o está muerta, salimos del loop */
        if (!t || t->state == TASK_DEAD) {
            hal_debug_write("[MAIN] GUI Task DEAD or NULL. Exiting loop.\n");
            break;
        }
        // hal_debug_write("[MAIN] GUI running...\n"); /* Uncomment for spam debug */
        task_sleep(200); /* No desperdiciar ciclos */
    }

    /* Al cerrar o matar la GUI, volvemos al modo texto */
    hal_console_write("\n  [INFO] FluxUI finalizado. Retornando a consola...\n");
    terminal_initialize(NULL); /* Reset terminal state & screen */
    shell_run(); 

    /* ---- 9. Lanzar shell interactivo (Fallback) ---- */
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
