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
#include <klog.h>
#include <string.h>
#include <stdio.h>
#include <io.h>
#include <boot.h>
#include <mouse.h>
#include <shell.h>
#include <mm.h>
#include <pmm.h>
#include <vmm.h>
#include <fs/initrd.h>
#include <fs/vfs.h>
#include <fs/devfs.h>
#include <fs/procfs.h>
#include <fs/jfs.h>
#include <vga.h>
#include <gfx/gfx.h>
#include <task.h>
#include <net/defs.h>
#include <net/socket.h>
#include <cpu.h>

#include <net/e1000.h>
#include <acpi.h>
#include <apic.h>
#include <futex.h>
#include <sem.h>
#include <serial.h>

/* ========================================================================= */
/* Constantes del Sistema                                                    */
/* ========================================================================= */
#define ETEROS_VERSION_MAJOR    0
#define ETEROS_VERSION_MINOR    2  /* Bump for SMP support */
#define ETEROS_VERSION_PATCH    0
#define ETEROS_CODENAME         "Genesis SMP"

/* ========================================================================= */
/* Prototipos internos                                                       */
/* ========================================================================= */
static void kernel_print_banner(void);
static void kernel_print_sysinfo(void);

extern void net_poll(void);
extern uint32_t my_ip;

static void network_task(void) {
    /* Process any pending packets before entering loop */
    net_poll();

    while(1) {
        /* Wait for network interrupt (packet received) */
        sem_wait(&net_sem);

        net_poll();

        /* Update status */
        if (!network_ready && my_ip != 0) {
            network_ready = 1;
            hal_console_write("  [NET]  DHCP Bound! IP assigned.\n");
        }
    }
}

extern void dhcp_discover(void);

static void init_network(void) {
    net_init();
    dhcp_discover();
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
    
    klog(KLOG_INFO, "HAL Initialized.\n");

    /* ---- 2. Obtener Info del Bootloader (si aplica) ---- */
    /* En x86, esto está en 0xA000. En ARM, puede ser NULL o DTB. */
    #if defined(ARCH_X86_64)
        boot_info_t* boot_info = (boot_info_t*)BOOT_INFO_ADDR;
    #else
        void* boot_info = NULL;
    #endif

    /* ---- 2.5 Inicializar ACPI (Hardware Discovery) ---- */
    #if defined(ARCH_X86_64)
    acpi_init();
    /* ---- 2.6 Inicializar SMP (BSP Topology) ---- */
    cpu_init_bsp();
    /* Inicializar PAT para soportar Write-Combining en video */
    pat_init();
    #endif

    /* ---- 3. Inicializar Memory Managers (Solo Tier 2+) ---- */
    #if ETEROS_TIER >= 2
        #include <fs/bcache.h>

        /* Memory Management Unit (Paging/MPU) */

        #if defined(ARCH_X86_64)
            /* x86 specific PMM init (uses E820) */
            pmm_init();
        #endif

        #if ETEROS_HAS_MMU
            hal_mmu_init(); /* Configura paginación virtual */
        #endif

        /* Heap Manager (Generic) */
        mm_init(boot_info);

        /* Block Cache (requires heap/kmalloc) */
        bcache_init();

        /* Now that PMM, VMM, and heap are ready, switch console to framebuffer */
        if (boot_info && boot_info->fb_addr != 0) {
            terminal_switch_to_framebuffer(boot_info);
        }
    #endif

    #if defined(ARCH_X86_64)
    /* ---- 2.7 Inicializar APIC y Despertar Cores ---- */
    /* Must happen AFTER heap init so we can allocate per-CPU structures */
    lapic_init();   /* Inicializar Local APIC del core principal */
    smp_init();     /* Despertar los Application Processors (APs) */
    #endif

    #if ETEROS_TIER >= 2
        /* ---- 3.5 Inicializar Initrd ---- */
        #if defined(ARCH_X86_64)
        if (boot_info && boot_info->initrd_addr != 0) {
            fs_root = initialise_initrd(boot_info->initrd_addr, boot_info->initrd_size);
            if (fs_root) {
                hal_console_write("[VFS] Initrd mounted at /\n");

                /* Dynamic Mounts */
                vfs_mkdir("/dev", 0);
                vfs_mount("/dev", devfs_init());
                vfs_mkdir("/proc", 0);
                vfs_mount("/proc", procfs_init());
                vfs_mkdir("/data", 0);
                vfs_mount("/data", jfs_init());

                /* List files using VFS */
                struct dirent entry;
                int i = 0;
                while (readdir_fs(fs_root, i, &entry) == 0) {
                    hal_console_write("  [VFS] Found file: ");
                    hal_console_write(entry.name);
                    hal_console_write("\n");
                    i++;
                }
            }
        }
        #endif

        /* ---- 3.6 Initrd listo ---- */

    #else
        /* Tier 1 (Microcontroller): Use simple static heap or similar if needed */
        /* For now, assume simple stack usage or static buffers */
    #endif

    /* ---- 4. Inicializar Red ---- */
    hal_console_write("\n  [NET]  Escaneando dispositivos de red...\n");
    if (e1000_init(NULL) == 0) {
        hal_console_write("  [NET]  Hardware inicializado.\n");
        init_network();
        task_create("Network", network_task);
    } else {
        hal_console_write("  [NET]  Info: No se detecto tarjeta de red compatible.\n");
        hal_console_write("         (El sistema continuara sin red)\n");
    }

    /* ---- 5. Mostrar banner de éterOS ---- */
    kernel_print_banner();

    /* ---- Log de depuración ---- */
    klog(KLOG_INFO, "Kernel loaded.\n");
    klog(KLOG_INFO, "(c) 2026 Tudex Networks\n");

    /* ---- 6. Información del sistema ---- */
    kernel_print_sysinfo();

    /* ---- 6.5 Inicializar Mouse (PS/2) ---- */
    /* Nota: Se inicializa después de la IDT/PIC pero antes de la GUI */
    mouse_init();

    /* ---- 7. Inicializar Scheduler ---- */
    hal_console_write("  [INIT] Scheduler Round-Robin\n");
    scheduler_init();
    
    /* ---- 7.1 Inicializar Futex ---- */
    futex_init();

    /* ---- 7.5 Lanzar Test de Espacio de Usuario ---- */
    hal_console_write("  [INIT] Lanzando User Mode Test...\n");
    extern void user_loader_entry(void);
    task_create("UserLoader", user_loader_entry);

    /* ---- 8. Lanzar shell interactivo ---- */
    hal_interrupts_enable();

    /* Kernel shell (fallback until userspace shell is ready) */
    shell_run();

    /* Main kernel task becomes Idle loop (reached if shell exits) */
    while(1) {
        hal_cpu_halt();
        task_yield();
    }
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
    kprintf("    Version %d.%d.%d (\"%s\")\n",
            ETEROS_VERSION_MAJOR, ETEROS_VERSION_MINOR, ETEROS_VERSION_PATCH, ETEROS_CODENAME);

    /* Copyright */
    hal_console_write("    (c) 2026 Tudex Networks\n");
    
    hal_console_write("  ========================================\n");
    hal_console_write("\n");
}

/**
 * Muestra información básica del sistema.
 */
static void kernel_print_sysinfo(void) {
    kprintf("  [INFO] Arquitectura: %s\n", ETEROS_ARCH_NAME);
    kprintf("  [INFO] Board/Target: Generic\n");
}

