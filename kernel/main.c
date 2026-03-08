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
#include <fs/shmfs.h>
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
#include <framebuffer.h>
#include <timer.h>

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
static void show_splash(void);

extern void net_poll(void);
extern uint32_t my_ip;

extern void dhcp_discover(void);

static void network_task(void) {
    /* Ejecutar DHCP Discover ahora que las interrupciones y el scheduler están activos */
    dhcp_discover();

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

static void init_network(void) {
    net_init();
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
    char dbg_buf[64];
    /* ---- 0. Limpiar BSS (el bootloader no lo hace) ---- */
    {
        extern uint8_t _kernel_start[], _bss_start[], _kernel_end[];
        serial_write_string("[DEBUG] _kernel_start: 0x"); utoa_hex_s((uint64_t)_kernel_start, dbg_buf, sizeof(dbg_buf)); serial_write_string(dbg_buf); serial_write_string("\n");
        serial_write_string("[DEBUG] _bss_start: 0x"); utoa_hex_s((uint64_t)_bss_start, dbg_buf, sizeof(dbg_buf)); serial_write_string(dbg_buf); serial_write_string("\n");
        serial_write_string("[DEBUG] _kernel_end: 0x"); utoa_hex_s((uint64_t)_kernel_end, dbg_buf, sizeof(dbg_buf)); serial_write_string(dbg_buf); serial_write_string("\n");
        serial_write_string("[DEBUG] kmain: 0x"); utoa_hex_s((uint64_t)kmain, dbg_buf, sizeof(dbg_buf)); serial_write_string(dbg_buf); serial_write_string("\n");

        uint8_t* p = _bss_start;
        while (p < _kernel_end) *p++ = 0;
    }

    /* ---- 0.5. Inicializar SMP (BSP Topology) ---- */
    #if defined(ARCH_X86_64)
    _Static_assert(__builtin_offsetof(cpu_info_t, kernel_stack_top) == 64, "ASM offset mismatch!");
    _Static_assert(__builtin_offsetof(cpu_info_t, user_stack_scratch) == 72, "ASM offset mismatch!");
    cpu_init_bsp();
    #endif

    /* ---- 1. Inicializar Hardware Abstraction Layer (HAL) ---- */
    /* Esto configura relojes, interrupciones, consola, timer, etc. */
    hal_init();
    
    serial_write_string("[DEBUG] HAL Initialized returned to main.\n");

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
            // terminal_set_silent(true); /* Deshabilitado para depuración visual completa */
        }
    #endif

    #if defined(ARCH_X86_64)
    /* ---- 2.7 Inicializar Scheduler y APIC ---- */
    /* Scheduler must be ready before APs boot because they create Idle tasks */
    scheduler_init();

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
                vfs_mkdir("/dev/shm", 0);
                vfs_mount("/dev/shm", shmfs_init());
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

    /* Network initialization moved after scheduler_init */

    /* ---- 5. Mostrar banner de éterOS ---- */
    kernel_print_banner();

    /* ---- Log de depuración ---- */
    klog(KLOG_INFO, "Kernel loaded.\n");
    klog(KLOG_INFO, "(c) 2026 Tudex Networks\n");

    /* ---- 6. Información del sistema ---- */
    kernel_print_sysinfo();

    /* Mouse se inicializa ahora en el HAL (hal_input_init) */

    
    /* ---- 7.1 Inicializar Red (Ahora podemos crear tareas) ---- */
    hal_console_write("\n  [NET]  Escaneando dispositivos de red...\n");
    if (e1000_init(NULL) == 0) {
        hal_console_write("  [NET]  Hardware inicializado.\n");
        init_network();
        task_create("Network", network_task);
    } else {
        hal_console_write("  [NET]  Info: No se detecto tarjeta de red compatible.\n");
        hal_console_write("         (El sistema continuara sin red)\n");
    }
    
    serial_write_string("[DEBUG] boot_info ptr: 0x");
    utoa_hex_s((uint64_t)boot_info, dbg_buf, sizeof(dbg_buf));
    serial_write_string(dbg_buf);
    serial_write_string("\n");

    if (boot_info) {
        serial_write_string("[DEBUG] boot_info->fb_addr: 0x");
        utoa_hex_s((uint64_t)boot_info->fb_addr, dbg_buf, sizeof(dbg_buf));
        serial_write_string(dbg_buf);
        serial_write_string("\n");
    }

    /* ---- 7.1 Inicializar Futex ---- */
    futex_init();

    /* ---- 7.5 Lanzar Test de Espacio de Usuario ---- */
    hal_console_write("  [INIT] Lanzando User Mode Test...\n");
    extern void user_loader_entry(void);
    task_create("UserLoader", user_loader_entry);
 
    /* ---- 8. Lanzar shell interactivo ---- */
    hal_interrupts_enable();
 
    /* Show system splash screen */
    // show_splash();
 
    /* Kernel shell (fallback until userspace shell is ready) */
    terminal_set_silent(true); /* Silence kernel terminal for Marea */
    terminal_clear();
 
    serial_write_string("[ETER] System ready, entering idle loop (Shell disabled for Userspace Marea)...\n");
    serial_write_string("  [INIT] Sistema listo. Cediendo control a Marea Shell...\n");
 
    /* shell_run(); // Disabled so it doesn't fight with Marea Shell over fb0 */

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

static void show_splash(void) {
    uint32_t size = 0;
    void* logo_data = initrd_read_file("logo.raw", &size);
    if (!logo_data) {
        hal_console_write("[SPLASH] Logo not found in initrd.\n");
        return;
    }

    /* Use framebuffer functions to draw */
    uint32_t screen_w = framebuffer_get_width();
    uint32_t screen_h = framebuffer_get_height();

    /* Clear to White */
    framebuffer_clear(0xFFFFFFFF);

    /* Center image (200x200) */
    int32_t start_x = (screen_w - 200) / 2;
    int32_t start_y = (screen_h - 200) / 2;

    if (start_x < 0) start_x = 0;
    if (start_y < 0) start_y = 0;

    uint32_t* pixel_data = (uint32_t*)logo_data;

    /* ⚡ BOLT Optimization: Direct memory access for 32bpp Framebuffer */
    if (framebuffer_get_bpp() == 32) {
        uint32_t* fb_buf = framebuffer_get_buffer();
        uint32_t fb_pitch = framebuffer_get_pitch();

        if (fb_buf) {
            for (int y = 0; y < 200; y++) {
                int draw_y = start_y + y;
                if (draw_y >= (int)screen_h) break;

                uint32_t* dest_row = (uint32_t*)((uint8_t*)fb_buf + (draw_y * fb_pitch) + (start_x * 4));
                uint32_t* src_row = pixel_data + (y * 200);

                /* Check if entire row fits */
                if (start_x + 200 <= (int)screen_w) {
                    /* Fast path: full row copy */
                    memcpy(dest_row, src_row, 200 * 4);
                } else {
                    /* Slow path: clipping required */
                    for (int x = 0; x < 200; x++) {
                        int draw_x = start_x + x;
                        if (draw_x >= (int)screen_w) break;
                        dest_row[x] = src_row[x];
                    }
                }
            }
        }
    } else {
        /* Fallback for other depths */
        for (int y = 0; y < 200; y++) {
            for (int x = 0; x < 200; x++) {
                 uint32_t color = pixel_data[y * 200 + x];
                 framebuffer_putpixel(start_x + x, start_y + y, color);
            }
        }
    }

    /* Wait ~2 seconds (busy wait on timer ticks to keep scheduler running) */
    uint64_t end_ticks = timer_get_ticks() + (2 * TIMER_HZ);
    while (timer_get_ticks() < end_ticks) {
        __asm__ volatile("hlt");
    }
}
