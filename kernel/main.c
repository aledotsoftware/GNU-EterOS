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
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <boot.h>
#include <shell.h>
#include <mm.h>
#include <pmm.h>
#include <vmm.h>
#include <fs/initrd.h>
#include <fs/devfs.h>
#include <fs/shmfs.h>
#include <fs/procfs.h>
#include <fs/jfs.h>
#include <fs/bcache.h>
#include <vga.h>
#include <gfx/gfx.h>
#include <gfx/drm.h>
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
#define ETEROS_VERSION_PATCH    19
#define ETEROS_CODENAME         "Genesis SMP"

/* ========================================================================= */
/* Prototipos internos                                                       */
/* ========================================================================= */
static void kernel_print_banner(void);
static void kernel_print_sysinfo(void);
static void show_splash(void) __attribute__((unused));

extern void net_poll(void);
extern uint32_t my_ip;

// extern void dhcp_discover(void);
static bool desktop_autostart = false;

static void network_task(void) {
    /* Process any pending packets before entering loop */
    net_poll();

    while(1) {
        sem_wait(&net_sem);

        /* Drain queue */
        for(int i = 0; i < 5; i++) {
            net_poll();
        }

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
    extern bool mm_initialized;
    extern bool pmm_initialized_flag;
    extern bool vmm_initialized_flag;
    ASSERT(mm_initialized == false && "Heap initialized before HAL!");
    ASSERT(pmm_initialized_flag == false && "PMM initialized before HAL!");
    ASSERT(vmm_initialized_flag == false && "VMM initialized before HAL!");
    hal_init();
    
    serial_write_string("[DEBUG] HAL Initialized returned to main.\n");
    ASSERT(1); /* Basic sanity check for assert macro */

    /* ---- 2. Obtener Info del Bootloader (si aplica) ---- */
    /* En x86, esto está en 0xA000. En ARM, puede ser NULL o DTB. */
    #if defined(ARCH_X86_64)
        extern void ata_init(void);
        ata_init();
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

        /* Memory Management Unit (Paging/MPU) */

        #if defined(ARCH_X86_64)
            /* x86 specific PMM init (uses E820) */
            ASSERT(mm_initialized == false && "Heap initialized before PMM!");
            ASSERT(vmm_initialized_flag == false && "VMM initialized before PMM!");
            pmm_init();
            ASSERT(pmm_get_total_ram() > 0);

            if (boot_info && boot_info->initrd_addr) {
                pmm_mark_region_used(boot_info->initrd_addr, boot_info->initrd_size);
            }
            if (boot_info && boot_info->fb_addr) {
                /* We don't have the exact FB size here, but typically it's less than 16MB */
                pmm_mark_region_used(boot_info->fb_addr, 16 * 1024 * 1024);
            }

            serial_write_string("[DEBUG] PMM Initialized.\n");
        #endif

        #if ETEROS_HAS_MMU
            ASSERT(mm_initialized == false && "Heap initialized before VMM!");
            ASSERT(pmm_initialized_flag == true && "VMM initialized before PMM!");
            hal_mmu_init(); /* Configura paginación virtual */
            serial_write_string("[DEBUG] VMM Initialized.\n");
        #endif

        /* Heap Manager (Generic) */
        ASSERT(pmm_initialized_flag == true && "Heap initialized before PMM!");
        #if ETEROS_HAS_MMU
        ASSERT(vmm_initialized_flag == true && "Heap initialized before VMM!");
        #endif
        mm_init(boot_info);
        ASSERT(mm_get_total_memory() > 0);
        serial_write_string("[DEBUG] Heap Initialized.\n");

        mm_initialized = true;

        /* Block Cache (requires heap/kmalloc) */
        bcache_init();

        /* Now that PMM, VMM, and heap are ready, switch console to framebuffer */
        if (boot_info && boot_info->fb_addr != 0) {
            terminal_switch_to_framebuffer(boot_info);
            terminal_set_silent(true);
        }
    #endif

    #if defined(ARCH_X86_64)
    /* ---- 2.7 Inicializar Scheduler y APIC ---- */
    /* Scheduler must be ready before APs boot because they create Idle tasks */
    scheduler_init();

    lapic_init();   /* Inicializar Local APIC del core principal */
    smp_init();     /* Despertar los Application Processors (APs) */
    serial_write_string("[DEBUG] Interrupts still deferred after scheduler/APIC init.\n");
    #endif

    #if ETEROS_TIER >= 2
        /* ---- 3.5 Inicializar Initrd ---- */
        #if defined(ARCH_X86_64)
        if (boot_info && boot_info->initrd_addr != 0) {
            fs_root = initialise_initrd(boot_info->initrd_addr, boot_info->initrd_size);
            if (fs_root) {
                hal_console_write("[VFS] Initrd mounted at /\n");
                desktop_autostart = false; /* boot_info &&
                                    boot_info->signature == 0x544F424B &&
                                    boot_info->fb_addr != 0 &&
                                    boot_info->fb_width != 0 &&
                                    boot_info->fb_height != 0 &&
                                    (finddir_fs(fs_root, "marea_shell.elf") != NULL); */

                /* Dynamic Mounts */
                vfs_mkdir("/dev", 0);
                vfs_mount("/dev", devfs_init());
                vfs_mkdir("/proc", 0);
                vfs_mount("/proc", procfs_init());
                fs_node_t* writable_fs = jfs_init();
                vfs_mkdir("/data", 0);
                vfs_mount("/data", writable_fs);
                vfs_mkdir("/gnu", 0);
                vfs_mount("/gnu", writable_fs);
                vfs_mkdir("/tmp", 0);
                vfs_mount("/tmp", shmfs_init());

                /* Copy initrd /etc contents to volatile shmfs /etc */
                fs_node_t* initrd_etc = vfs_lookup(fs_root, "/etc");
                uint8_t* passwd_buf = NULL;
                uint8_t* shadow_buf = NULL;
                uint8_t* autologin_buf = NULL;
                int p_len = 0, s_len = 0, a_len = 0;

                if (initrd_etc) {
                    fs_node_t* p_node = vfs_lookup(fs_root, "/etc/passwd");
                    if (p_node) {
                        p_len = p_node->length;
                        passwd_buf = kmalloc(p_len + 1);
                        if (passwd_buf) read_fs(p_node, 0, p_len, passwd_buf);
                        else p_len = 0;
                    }
                    fs_node_t* s_node = vfs_lookup(fs_root, "/etc/shadow");
                    if (s_node) {
                        s_len = s_node->length;
                        shadow_buf = kmalloc(s_len + 1);
                        if (shadow_buf) read_fs(s_node, 0, s_len, shadow_buf);
                        else s_len = 0;
                    }
                    fs_node_t* a_node = vfs_lookup(fs_root, "/etc/autologin");
                    if (a_node) {
                        a_len = a_node->length;
                        autologin_buf = kmalloc(a_len + 1);
                        if (autologin_buf) read_fs(a_node, 0, a_len, autologin_buf);
                        else a_len = 0;
                    }
                }

                vfs_mkdir("/etc", 0);
                vfs_mount("/etc", shmfs_init());

                fs_node_t* etc_node = vfs_lookup(fs_root, "/etc");
                if (etc_node) {
                    if (a_len > 0 && autologin_buf) {
                        create_fs(etc_node, "autologin", 0644);
                        fs_node_t* a_n = vfs_lookup(fs_root, "/etc/autologin");
                        if (a_n) write_fs(a_n, 0, a_len, autologin_buf);
                    } else {
                        create_fs(etc_node, "autologin", 0644);
                        fs_node_t* a_n = vfs_lookup(fs_root, "/etc/autologin");
                        if (a_n) write_fs(a_n, 0, 2, (uint8_t*)"1\n");
                    }

                    if (p_len > 0 && passwd_buf) {
                        create_fs(etc_node, "passwd", 0644);
                        fs_node_t* p_n = vfs_lookup(fs_root, "/etc/passwd");
                        if (p_n) write_fs(p_n, 0, p_len, passwd_buf);
                    }

                    if (s_len > 0 && shadow_buf) {
                        create_fs(etc_node, "shadow", 0600);
                        fs_node_t* s_n = vfs_lookup(fs_root, "/etc/shadow");
                        if (s_n) write_fs(s_n, 0, s_len, shadow_buf);
                    }
                }

                if (passwd_buf) kfree(passwd_buf);
                if (shadow_buf) kfree(shadow_buf);
                if (autologin_buf) kfree(autologin_buf);

                mkdir_fs(writable_fs, "bin", 0755);
                mkdir_fs(writable_fs, "lib", 0755);
                mkdir_fs(writable_fs, "include", 0755);
                mkdir_fs(writable_fs, "tmp", 01777);

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
        serial_write_string("[DEBUG] main: calling init_network()\n");
        init_network();
        serial_write_string("[DEBUG] main: creating network task\n");
        task_create("Network", network_task);
        serial_write_string("[DEBUG] main: network task created\n");
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
    serial_write_string("[DEBUG] main: futex_init()\n");
    futex_init();

    /* ---- 7.5 Lanzar Test de Espacio de Usuario ---- */
    hal_console_write("  [INIT] Lanzando User Mode Test...\n");
    extern void user_loader_entry(void);
    serial_write_string("[DEBUG] main: creating user loader task\n");
    task_create("UserLoader", user_loader_entry);
    serial_write_string("[DEBUG] main: user loader task created\n");
 
    /* ---- 8. Lanzar shell interactivo ---- */
    serial_write_string("[DEBUG] main: entering final shell/desktop phase\n");
    hal_interrupts_enable();
    serial_write_string("[DEBUG] Interrupts enabled for runtime scheduling.\n");

    if (desktop_autostart) {
        terminal_set_silent(true);
        serial_write_string("[ETER] Desktop autostart detected. Kernel framebuffer terminal disabled.\n");
        while(1) {
            hal_cpu_enable_interrupts_and_halt();
            task_yield();
        }
    }
 
    /* Show system splash screen */
    // show_splash();
 
    /* Kernel shell (fallback until userspace shell is ready) */
    terminal_set_silent(false); 
    terminal_clear();
 
    serial_write_string("[ETER] System ready, starting interactive terminal...\n");
    serial_write_string("  [INIT] Sistema listo. Iniciando Terminal...\n");
 
    shell_run(); 

    /* Main kernel task becomes Idle loop (reached if shell exits) */
    while(1) {
        hal_cpu_enable_interrupts_and_halt();
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
    
    /* Logo de éterOS en arte ASCII (Austral Aurora theme - Premium) */
    hal_console_write("\033[38;2;85;255;255m   :::::::::: ::::::::::: :::::::::: :::::::::   ::::::::   ::::::::  \033[0m\n");
    hal_console_write("\033[38;2;75;230;255m  :+:             :+:     :+:        :+:    :+: :+:    :+: :+:    :+: \033[0m\n");
    hal_console_write("\033[38;2;65;200;255m  +:+             +:+     +:+        +:+    +:+ +:+    +:+ +:+        \033[0m\n");
    hal_console_write("\033[38;2;55;175;255m  +#++:++#        +#+     +#++:++#   +#++:++#:  +#+    +:+ +#++:++#++ \033[0m\n");
    hal_console_write("\033[38;2;45;150;255m  +#+             +#+     +#+        +#+    +#+ +#+    +#+        +#+ \033[0m\n");
    hal_console_write("\033[38;2;35;120;255m  #+#             #+#     #+#        #+#    #+# #+#    #+# #+#    #+# \033[0m\n");
    hal_console_write("\033[38;2;25;90;255m  ##########      ###     ########## ###    ###  ########   ########  \033[0m\n");

    hal_console_write("\n");
    hal_console_write("\033[38;2;0;200;255m  ====================================================================\033[0m\n");
    
    /* Versión y codename */
    kprintf("       \033[38;2;255;255;255mVersion %d.%d.%d \033[38;2;150;150;150m// \033[38;2;0;255;200m\"%s\"\033[0m\n",
            ETEROS_VERSION_MAJOR, ETEROS_VERSION_MINOR, ETEROS_VERSION_PATCH, ETEROS_CODENAME);

    /* Copyright */
    hal_console_write("       \033[38;2;120;120;120m(c) 2026 Tudex Networks \033[38;2;80;80;80m| Premium Edition\033[0m\n");
    
    hal_console_write("\033[38;2;0;200;255m  ====================================================================\033[0m\n");
    hal_console_write("\n");
}

/**
 * Muestra información básica del sistema.
 */
static void kernel_print_sysinfo(void) {
    kprintf("  [INFO] Arquitectura: %s\n", ETEROS_ARCH_NAME);
    kprintf("  [INFO] Board/Target: Generic\n");
}

static void show_splash(void) __attribute__((unused));
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
        /* ⚡ BOLT Optimization: Use linear pointer advancement to avoid multiplication */
        uint32_t* src_pixel = pixel_data;
        for (int y = 0; y < 200; y++) {
            for (int x = 0; x < 200; x++) {
                 uint32_t color = *src_pixel++;
                 framebuffer_putpixel(start_x + x, start_y + y, color);
            }
        }
    }

    /* Wait ~2 seconds (busy wait on timer ticks to keep scheduler running) */
    uint64_t end_ticks = timer_get_ticks() + (2 * TIMER_HZ);
    while (timer_get_ticks() < end_ticks) {
        hal_cpu_enable_interrupts_and_halt();
    }
}
