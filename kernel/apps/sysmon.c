/**
 * =============================================================================
 * éterOS - EterMon v1.0 (System Monitor)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 * =============================================================================
 *
 * Monitor del sistema en tiempo real para éterOS.
 * Usa CPUID para identificar la CPU y el PIT para uptime.
 *
 * Pantallas:
 *   [1] Vista general (CPU, RAM, Uptime)
 *   [2] Detalles de CPU (CPUID features)
 *   [3] Mapa de memoria
 *   [0] Salir
 *
 * =============================================================================
 */

#include "../include/sysmon.h"
#include "../include/vga.h"
#include "../include/keyboard.h"
#include "../include/string.h"
#include "../include/timer.h"
#include "../include/io.h"
#include "../include/vmm.h"

/* ========================================================================= */
/* CPUID helpers                                                             */
/* ========================================================================= */

static void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                  uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0)
    );
}

/** Obtiene el vendor string (12 chars) */
static void get_cpu_vendor(char* buf) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(0, &eax, &ebx, &ecx, &edx);

    /* EBX + EDX + ECX = vendor string */
    *(uint32_t*)(buf + 0) = ebx;
    *(uint32_t*)(buf + 4) = edx;
    *(uint32_t*)(buf + 8) = ecx;
    buf[12] = '\0';
}

/** Obtiene el brand string (48 chars, CPUID extended) */
static void get_cpu_brand(char* buf) {
    uint32_t eax, ebx, ecx, edx;

    /* Verificar si extended CPUID está soportado */
    cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax < 0x80000004) {
        /* No soportado - poner string genérico */
        const char* fallback = "Unknown CPU";
        size_t i = 0;
        while (fallback[i]) { buf[i] = fallback[i]; i++; }
        buf[i] = '\0';
        return;
    }

    /* Leer las 3 hojas del brand string */
    for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
        cpuid(leaf, &eax, &ebx, &ecx, &edx);
        uint32_t offset = (leaf - 0x80000002) * 16;
        *(uint32_t*)(buf + offset + 0)  = eax;
        *(uint32_t*)(buf + offset + 4)  = ebx;
        *(uint32_t*)(buf + offset + 8)  = ecx;
        *(uint32_t*)(buf + offset + 12) = edx;
    }
    buf[48] = '\0';

    /* Trim leading spaces */
    char* src = buf;
    while (*src == ' ') src++;
    if (src != buf) {
        size_t i = 0;
        while (src[i]) { buf[i] = src[i]; i++; }
        buf[i] = '\0';
    }
}

/** Obtiene feature flags de la CPU */
typedef struct {
    uint8_t sse;
    uint8_t sse2;
    uint8_t sse3;
    uint8_t sse41;
    uint8_t sse42;
    uint8_t avx;
    uint8_t avx2;
    uint8_t fpu;
    uint8_t apic;
    uint8_t nx;
    uint8_t lm;       /* Long Mode (64-bit) */
    uint8_t hypervisor;
} cpu_features_t;

static cpu_features_t get_cpu_features(void) {
    cpu_features_t feat = {0};
    uint32_t eax, ebx, ecx, edx;

    /* Leaf 1: Feature flags */
    cpuid(1, &eax, &ebx, &ecx, &edx);
    feat.fpu   = (edx >> 0)  & 1;
    feat.apic  = (edx >> 9)  & 1;
    feat.sse   = (edx >> 25) & 1;
    feat.sse2  = (edx >> 26) & 1;
    feat.sse3  = (ecx >> 0)  & 1;
    feat.sse41 = (ecx >> 19) & 1;
    feat.sse42 = (ecx >> 20) & 1;
    feat.avx   = (ecx >> 28) & 1;
    feat.hypervisor = (ecx >> 31) & 1;

    /* Leaf 7: Extended features */
    cpuid(7, &eax, &ebx, &ecx, &edx);
    feat.avx2 = (ebx >> 5) & 1;

    /* Extended leaf: NX, LM */
    cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
    feat.nx = (edx >> 20) & 1;
    feat.lm = (edx >> 29) & 1;

    return feat;
}

/* ========================================================================= */
/* Helpers de pantalla                                                       */
/* ========================================================================= */

static void format_hex(uint64_t val, char* buf, size_t size) {
    if (size < 3) return;
    buf[0] = '0';
    buf[1] = 'x';
    itoa_s(val, buf + 2, size - 2, 16);
}

static void draw_separator(void) {
    terminal_write_colored(
        "  --------------------------------------------------------\n",
        VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
}

static void draw_header(const char* subtitle) {
    terminal_initialize(NULL);
    terminal_write_string("\n");
    terminal_write_colored("  EterMon", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(" > ", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored(subtitle, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_string("\n");
    draw_separator();
}

/** Imprime una línea key: value con colores */
static void print_kv(const char* key, const char* value) {
    terminal_write_colored("    ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored(key, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_colored(value, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("\n");
}

/** Imprime un número como string */
static void print_kv_num(const char* key, uint32_t value) {
    char buf[12];
    itoa_s(value, buf, sizeof(buf), 10);
    print_kv(key, buf);
}

/** Imprime una feature flag como tag coloreado */
static void print_feature(const char* name, uint8_t supported) {
    if (supported) {
        terminal_write_colored(" [", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        terminal_write_colored(name, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal_write_colored("]", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    } else {
        terminal_write_colored(" [", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        terminal_write_colored(name, VGA_COLOR_RED, VGA_COLOR_BLACK);
        terminal_write_colored("]", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    }
}

/* ========================================================================= */
/* Pantalla 1: Vista General                                                 */
/* ========================================================================= */

static void show_overview(void) {
    char vendor[16];
    char brand[52];
    char buf[16];

    get_cpu_vendor(vendor);
    get_cpu_brand(brand);

    uint32_t secs = timer_get_uptime_seconds();
    uint32_t mins = secs / 60;
    uint32_t hrs  = mins / 60;

    draw_header("Vista General");
    terminal_write_string("\n");

    /* ---- CPU ---- */
    terminal_write_colored("  CPU\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    print_kv("  Procesador:  ", brand);
    print_kv("  Fabricante:  ", vendor);

    cpu_features_t f = get_cpu_features();
    terminal_write_colored("    Modo:          ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_colored(f.lm ? "64-bit (Long Mode)" : "32-bit", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("\n");

    terminal_write_colored("    Virtualizado:  ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_colored(f.hypervisor ? "Si (VM)" : "No (Bare Metal)", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("\n\n");

    /* ---- Memoria ---- */
    terminal_write_colored("  MEMORIA\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    print_kv("  Kernel:      ", "~18 KB @ 0x10000");
    print_kv("  Stack:       ", "64 KB @ 0x80000-0x90000");
    print_kv("  VGA Buffer:  ", "4 KB @ 0xB8000");
    print_kv("  Page Tables: ", "12 KB @ 0x70000");
    terminal_write_string("\n");

    /* ---- Uptime ---- */
    terminal_write_colored("  SISTEMA\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_colored("    Uptime:    ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    itoa_s(hrs, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("h ");
    itoa_s(mins % 60, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("m ");
    itoa_s(secs % 60, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("s\n");

    print_kv("  Timer:       ", "PIT @ 100 Hz");

    itoa_s((uint32_t)(timer_get_ticks() & 0xFFFFFFFF), buf, sizeof(buf), 10);
    terminal_write_colored("    Ticks:     ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("\n");

    terminal_write_string("\n");
    draw_separator();
    terminal_write_colored("  [R] Refrescar  [0] Volver al menu\n",
                          VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
}

/* ========================================================================= */
/* Pantalla 2: Detalles de CPU                                               */
/* ========================================================================= */

static void show_cpu_details(void) {
    char vendor[16];
    char brand[52];
    get_cpu_vendor(vendor);
    get_cpu_brand(brand);

    cpu_features_t f = get_cpu_features();

    draw_header("CPU - Detalles");
    terminal_write_string("\n");

    print_kv("  Procesador:  ", brand);
    print_kv("  Fabricante:  ", vendor);

    terminal_write_string("\n");
    terminal_write_colored("  Instrucciones soportadas:\n\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);

    terminal_write_string("   ");
    print_feature("FPU",   f.fpu);
    print_feature("SSE",   f.sse);
    print_feature("SSE2",  f.sse2);
    print_feature("SSE3",  f.sse3);
    print_feature("SSE4.1",f.sse41);
    print_feature("SSE4.2",f.sse42);
    terminal_write_string("\n   ");
    print_feature("AVX",   f.avx);
    print_feature("AVX2",  f.avx2);
    print_feature("NX",    f.nx);
    print_feature("LM",    f.lm);
    print_feature("APIC",  f.apic);
    terminal_write_string("\n\n");

    terminal_write_colored("  Estado:\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_colored("    Virtualizado:  ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    if (f.hypervisor) {
        terminal_write_colored("Si", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        terminal_write_colored(" (corriendo en maquina virtual)\n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    } else {
        terminal_write_colored("No", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal_write_colored(" (hardware real / bare metal)\n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    }

    terminal_write_colored("    Arquitectura:  ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_colored("x86_64\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    terminal_write_colored("    Modo actual:   ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_colored("Ring 0 (Kernel)\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    terminal_write_string("\n");
    draw_separator();
    terminal_write_colored("  Presiona cualquier tecla para volver...\n",
                          VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    keyboard_getchar();
}

/* ========================================================================= */
/* Pantalla 3: Mapa de memoria                                               */
/* ========================================================================= */

static void show_memory_map(void) {
    draw_header("Mapa de Memoria");
    terminal_write_string("\n");

    terminal_write_colored("  Direccion           Tamanio    Descripcion\n",
                          VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    draw_separator();

    terminal_write_colored("  0x00000 - 0x003FF", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_write_string("   1 KB      IVT (Interrupt Vector Table)\n");

    terminal_write_colored("  0x00400 - 0x004FF", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_write_string("   256 B     BDA (BIOS Data Area)\n");

    terminal_write_colored("  0x07C00 - 0x07DFF", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("   512 B     Stage 1 Bootloader (MBR)\n");

    terminal_write_colored("  0x07E00 - 0x09DFF", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("   8 KB      Stage 2 Bootloader\n");

    /* ---- Kernel (Dynamic) ---- */
    extern char _kernel_start[];
    extern char _kernel_end[];
    uint64_t k_start = (uint64_t)_kernel_start;
    uint64_t k_end   = (uint64_t)_kernel_end;
    uint64_t k_size  = k_end - k_start;
    char s_buf[20], e_buf[20], sz_buf[16];

    format_hex(k_start, s_buf, sizeof(s_buf));
    format_hex(k_end, e_buf, sizeof(e_buf));

    terminal_write_colored("  ", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_colored(s_buf, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_colored(" - ", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_colored(e_buf, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);

    terminal_write_string("   ");
    itoa_s(k_size / 1024, sz_buf, sizeof(sz_buf), 10);
    terminal_write_string(sz_buf);
    terminal_write_string(" KB     Ether-Core (Kernel)\n");

    /* ---- Page Tables (Dynamic) ---- */
    uint64_t pt_start = BOOT_PML4_ADDR;
    uint64_t pt_size  = BOOT_PAGE_TABLE_SIZE;
    uint64_t pt_end   = pt_start + pt_size - 1;

    format_hex(pt_start, s_buf, sizeof(s_buf));
    format_hex(pt_end, e_buf, sizeof(e_buf));

    terminal_write_colored("  ", VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    terminal_write_colored(s_buf, VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    terminal_write_colored(" - ", VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    terminal_write_colored(e_buf, VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);

    terminal_write_string("   ");
    itoa_s(pt_size / 1024, sz_buf, sizeof(sz_buf), 10);
    terminal_write_string(sz_buf);
    terminal_write_string(" KB     Page Tables (PML4/PDPT/PD)\n");

    terminal_write_colored("  0x80000 - 0x8FFFF", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_string("   64 KB     Kernel Stack\n");

    terminal_write_colored("  0xB8000 - 0xB8F9F", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("   4 KB      VGA Text Buffer\n");

    terminal_write_string("\n");
    terminal_write_colored("  Identity mapped: ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("primeros 8 MB (2 MB pages)\n");

    terminal_write_string("\n");
    draw_separator();
    terminal_write_colored("  Presiona cualquier tecla para volver...\n",
                          VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    keyboard_getchar();
}

/* ========================================================================= */
/* Menú principal de EterMon                                                 */
/* ========================================================================= */

static void show_menu(void) {
    terminal_initialize(NULL);
    terminal_write_string("\n");

    terminal_write_colored(
        "    _____ _            __  __\n",
        VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(
        "   | ____| |_ ___ _ _|  \\/  | ___  _ __\n",
        VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(
        "   |  _| | __/ _ \\ '_| |\\/| |/ _ \\| '_ \\\n",
        VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(
        "   | |___| ||  __/ | | |  | | (_) | | | |\n",
        VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(
        "   |_____|\\__\\___|_| |_|  |_|\\___/|_| |_|\n",
        VGA_COLOR_BLUE, VGA_COLOR_BLACK);

    terminal_write_colored(
        "   ========== Monitor del Sistema ==========\n",
        VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);

    terminal_write_string("\n");
    draw_separator();
    terminal_write_string("\n");

    terminal_write_colored("   [1] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("Vista General (CPU, RAM, Uptime)\n\n");

    terminal_write_colored("   [2] ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_string("Detalles de CPU (CPUID)\n\n");

    terminal_write_colored("   [3] ", VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    terminal_write_string("Mapa de Memoria\n\n");

    terminal_write_colored("   [0] ", VGA_COLOR_RED, VGA_COLOR_BLACK);
    terminal_write_string("Volver al Shell\n");

    terminal_write_string("\n");
    draw_separator();
    terminal_write_colored("  Presiona 1, 2, 3 o 0: ",
                          VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
}

/* ========================================================================= */
/* Entry Point                                                               */
/* ========================================================================= */

void sysmon_run(void) {
    for (;;) {
        show_menu();

        char c = keyboard_getchar();

        switch (c) {
            case '1': {
                /* Vista general con refresh */
                for (;;) {
                    show_overview();
                    char k = keyboard_getchar();
                    if (k == 'r' || k == 'R') continue;
                    break;
                }
                break;
            }
            case '2':
                show_cpu_details();
                break;
            case '3':
                show_memory_map();
                break;
            case '0':
            case 27:  /* ESC */
                return;
            default:
                break;
        }
    }
}
