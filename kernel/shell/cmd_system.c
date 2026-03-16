#include "shell_internal.h"
#include "../../include/pmm.h"
#include "../../include/mm.h"
#include "../../include/hal.h"
#include "../../include/timer.h"
#include "../../include/rtc.h"
#include "../../include/serial.h"
#include "../../include/io.h"
#include "../../include/idt.h"
#include "../../include/user_mode.h"
#include "../../include/string.h"
#include "../../include/elf.h"

#define ETEROS_VERSION      "0.1.0"
#define ETEROS_CODENAME     "Genesis"

void cmd_version(const char* args) {
    (void)args;
    terminal_write_string("\n  eterOS v");
    terminal_write_colored(ETEROS_VERSION, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string(" (\"");
    terminal_write_colored(ETEROS_CODENAME, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_string("\")\n\n");
}

void cmd_sysinfo(const char* args) {
    (void)args;
    char buf[32];
    terminal_write_string("\n");
    terminal_write_colored("  Arquitectura: ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("x86_64 (Long Mode)\n");
    terminal_write_colored("  Video:        ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("VGA Texto 80x25\n");
    terminal_write_colored("  RAM Total:    ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    itoa_s(pmm_get_total_ram() / (1024*1024), buf, sizeof(buf), 10);
    terminal_write_string(buf);
    terminal_write_string(" MB\n");
    terminal_write_colored("  RAM Libre:    ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    itoa_s(pmm_get_free_ram() / (1024*1024), buf, sizeof(buf), 10);
    terminal_write_string(buf);
    terminal_write_string(" MB\n");
    terminal_write_colored("  Teclado:      ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("PS/2 (IRQ1, Set 1, Extended)\n");
    terminal_write_colored("  Timer:        ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("PIT @ 100 Hz\n");
    terminal_write_colored("  Serial:       ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("COM1 @ 38400 baud\n");
    terminal_write_colored("  Uptime:       ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    uint32_t secs = timer_get_uptime_seconds();
    itoa_s(secs / 60, buf, sizeof(buf), 10);
    terminal_write_string(buf);
    terminal_write_string("m ");
    itoa_s(secs % 60, buf, sizeof(buf), 10);
    terminal_write_string(buf);
    terminal_write_string("s\n");

    #include "../../include/nvram.h"
    uint8_t boot_part = nvram_get_boot_partition();
    terminal_write_colored("  Boot Slot:    ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string(boot_part == 0 ? "A (Active) / B (Pending)\n" : "B (Active) / A (Pending)\n");
    terminal_write_colored("  Build Date:   ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string(__DATE__);
    terminal_write_string(" ");
    terminal_write_string(__TIME__);
    terminal_write_string("\n");
    terminal_write_colored("  Commit Hash:  ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("5715f15ae4c99ca17e6d3d5a6eb61faaddd05d6c\n");

    terminal_write_string("\n");
}

void cmd_about(const char* args) {
    (void)args;
    terminal_write_string("\n");
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
    terminal_write_string("  El Sistema Operativo de la Nueva Era\n");
    terminal_write_colored("  (c) 2026 Tudex Networks\n",
                          VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    terminal_write_string("\n");
    terminal_write_string("  Kernel hibrido bare-metal para x86_64.\n");
    terminal_write_string("  Disenado para ser ligero, modular y extensible.\n");
    terminal_write_string("\n");
}

void cmd_reboot(const char* args) {
    (void)args;
    terminal_write_colored("  Reiniciando...\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    serial_write_string("[eterOS] Reboot solicitado.\n");

    /* Triple fault: cargar IDT vacía y disparar interrupción */
    struct idt_ptr null_idt = { 0, 0 };

    __asm__ volatile ("lidt %0" : : "m"(null_idt));
    __asm__ volatile ("int $0x03");

    /* Si por alguna razón no funciona, usar reset vía 8042 */
    outb(0x64, 0xFE);

    for (;;) { __asm__ volatile ("hlt"); }
}

void cmd_halt(const char* args) {
    (void)args;
    terminal_write_colored("  CPU detenida (HLT).\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    serial_write_string("[eterOS] Halt solicitado.\n");
    __asm__ volatile ("cli");
    for (;;) { __asm__ volatile ("hlt"); }
}

void cmd_uptime(const char* args) {
    (void)args;
    char buf[32];
    uint32_t total_secs = timer_get_uptime_seconds();
    uint32_t hours = total_secs / 3600;
    uint32_t mins  = (total_secs % 3600) / 60;
    uint32_t secs  = total_secs % 60;

    terminal_write_string("\n  Sistema activo: ");
    if (hours > 0) {
        itoa_s(hours, buf, sizeof(buf), 10);
        terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        terminal_write_string("h ");
    }
    itoa_s(mins, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("m ");
    itoa_s(secs, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("s\n\n");
}

static void print_time(rtc_time_t* t, const char* label) {
    char buf[16];
    terminal_write_string(label);

    // YYYY-MM-DD
    itoa_s(t->year, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("-");

    if (t->month < 10) terminal_write_string("0");
    itoa_s(t->month, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("-");

    if (t->day < 10) terminal_write_string("0");
    itoa_s(t->day, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    terminal_write_string(" ");

    // HH:MM:SS
    if (t->hours < 10) terminal_write_string("0");
    itoa_s(t->hours, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string(":");

    if (t->minutes < 10) terminal_write_string("0");
    itoa_s(t->minutes, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string(":");

    if (t->seconds < 10) terminal_write_string("0");
    itoa_s(t->seconds, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    terminal_write_string("\n");
}

void cmd_date(const char* args) {
    (void)args;
    rtc_time_t utc, local;

    rtc_get_time(&utc);
    rtc_to_argentina(&utc, &local);

    terminal_write_string("\n");
    print_time(&utc,   "  UTC:       ");
    print_time(&local, "  Argentina: ");
    terminal_write_string("\n");
}

void cmd_usermode(const char* args) {
    (void)args;
    terminal_write_string("\n  [USER] Preparing to enter User Mode (Ring 3)...\n");

    /* 1. Allocate a page for User Code */
    void* code_page = pmm_alloc_page();
    if (!code_page) {
        terminal_write_string("  Error: No memory for user code.\n");
        return;
    }

    /* 2. Allocate a page for User Stack */
    void* stack_page = pmm_alloc_page();
    if (!stack_page) {
        terminal_write_string("  Error: No memory for user stack.\n");
        return;
    }

    /* 3. Map pages as USER (Ring 3 accessible) */
    /* Use addresses above 4GB to avoid colliding with Bootloader's Kernel Huge Pages */
    uint64_t code_virt = 0x200000000;
    uint64_t stack_virt = 0x300000000;

    /* Ensure pages are mapped with USER flag */
    /* Code page: READ | WRITE | EXEC | USER */
    hal_mem_map((uint64_t)code_page, code_virt, HAL_MEM_READ | HAL_MEM_WRITE | HAL_MEM_USER | HAL_MEM_EXEC);
    /* Stack page: READ | WRITE | USER (No Exec) */
    hal_mem_map((uint64_t)stack_page, stack_virt, HAL_MEM_READ | HAL_MEM_WRITE | HAL_MEM_USER);

    /* 4. Write User Code */
    /* Payload:
       mov rax, 0xCAFEBABE
       syscall
       jmp $
    */
    uint8_t* code = (uint8_t*)code_virt;

    /* mov rax, 0xCAFEBABE */
    /* 48 C7 C0 BE BA FE CA */
    code[0] = 0x48; code[1] = 0xC7; code[2] = 0xC0;
    code[3] = 0xBE; code[4] = 0xBA; code[5] = 0xFE; code[6] = 0xCA;

    /* syscall (Opcode 0x0F 0x05) */
    code[7] = 0x0F; code[8] = 0x05;

    /* jmp $ (EB FE) */
    code[9] = 0xEB; code[10] = 0xFE;

    terminal_write_string("  [USER] Code loaded at 0x");
    char buf[32];
    utoa_hex_s(code_virt, buf, sizeof(buf));
    terminal_write_string(buf);
    terminal_write_string("\n  [USER] Stack at 0x");
    utoa_hex_s(stack_virt, buf, sizeof(buf));
    terminal_write_string(buf);
    terminal_write_string("\n  [USER] JUMPING TO RING 3!\n");

    /* 5. Enter User Mode */
    /* Stack grows down, so stack pointer is at the END of the page */
    void* stack_top = (void*)(stack_virt + PAGE_SIZE);

    /* Disable interrupts to prevent scheduler from interrupting us before we are fully in user mode?
       Actually enter_user_mode does CLI. */

    enter_user_mode((void*)code_virt, stack_top);

    /* Should not return here (user code loops) */
    terminal_write_string("  [USER] Returned? (Should not happen)\n");
}

void cmd_run(const char* args) {
    if (!args || *args == '\0') {
        terminal_write_string("  Uso: run <path_to_elf>\n");
        return;
    }

    /* 1. Load ELF */
    uint64_t entry_point = elf_load_file(args, 0x200000000);
    if (entry_point == 0) {
        terminal_write_string("  Error: No se pudo cargar el archivo ELF: ");
        terminal_write_string(args);
        terminal_write_string("\n");
        return;
    }

    terminal_write_string("  [ELF] Cargado exitosamente. Preparando modo usuario...\n");
    terminal_write_string("  [RUN] Punto de entrada detectado: 0x");
    char buf[32]; utoa_hex_s(entry_point, buf, sizeof(buf));
    terminal_write_string(buf);
    terminal_write_string("\n");
    terminal_write_string("  [RUN] ADVERTENCIA: Ejecucion directa desde el Kernel Shell es experimental.\n");
}

#include "../../include/task.h"
#include "../../include/fs/vfs.h"
#include "../../include/mm.h"

void cmd_ls(const char* args) {
    task_t* current = task_get_current();
    char path[256];
    
    if (!args || *args == '\0') {
        strlcpy(path, current->cwd, 256);
    } else {
        if (vfs_normalize_path(path, 256, args, current->cwd) != 0) {
            terminal_write_string("  Error: Ruta invalida.\n");
            return;
        }
    }

    fs_node_t* node = vfs_lookup(fs_root, path);
    if (!node) {
        terminal_write_string("  Error: No se encontro el directorio: ");
        terminal_write_string(path);
        terminal_write_string("\n");
        return;
    }

    if (!(node->flags & FS_DIRECTORY)) {
        terminal_write_string("  Error: No es un directorio.\n");
        kfree(node);
        return;
    }

    terminal_write_string("\n  Contenido de ");
    terminal_write_colored(path, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_string(":\n\n");

    struct dirent entry;
    int i = 0;
    while (readdir_fs(node, i++, &entry) == 0) {
        /* Lookup to get type */
        fs_node_t* item = finddir_fs(node, entry.name);
        if (item) {
            if (item->flags & FS_DIRECTORY) {
                terminal_write_colored("    [DIR] ", VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
            } else {
                terminal_write_colored("    [FIL] ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            }
            terminal_write_string(entry.name);
            
            if (!(item->flags & FS_DIRECTORY)) {
                terminal_write_string(" (");
                char buf[16]; itoa_s(item->length, buf, sizeof(buf), 10);
                terminal_write_string(buf);
                terminal_write_string(" bytes)");
            }
            terminal_write_string("\n");
            kfree(item);
        } else {
            terminal_write_string("    [???] ");
            terminal_write_string(entry.name);
            terminal_write_string("\n");
        }
    }
    terminal_write_string("\n");
    kfree(node);
}

void cmd_cd(const char* args) {
    if (!args || *args == '\0') return;

    task_t* current = task_get_current();
    char path[256];
    
    if (vfs_normalize_path(path, 256, args, current->cwd) != 0) {
        terminal_write_string("  Error: Ruta invalida.\n");
        return;
    }

    fs_node_t* node = vfs_lookup(fs_root, path);
    if (!node) {
        terminal_write_string("  Error: No se encontro el directorio: ");
        terminal_write_string(path);
        terminal_write_string("\n");
        return;
    }

    if (!(node->flags & FS_DIRECTORY)) {
        terminal_write_string("  Error: No es un directorio.\n");
        kfree(node);
        return;
    }

    /* Actualizar CWD de la tarea actual */
    strlcpy(current->cwd, path, sizeof(current->cwd));
    kfree(node);
}

void cmd_pwd(const char* args) {
    (void)args;
    task_t* current = task_get_current();
    terminal_write_string("  ");
    terminal_write_string(current->cwd);
    terminal_write_string("\n");
}

void cmd_cat(const char* args) {
    if (!args || *args == '\0') {
        terminal_write_string("  Uso: cat <file>\n");
        return;
    }

    task_t* current = task_get_current();
    char path[256];
    
    if (vfs_normalize_path(path, 256, args, current->cwd) != 0) {
        terminal_write_string("  Error: Ruta invalida.\n");
        return;
    }

    fs_node_t* node = vfs_lookup(fs_root, path);
    if (!node) {
        terminal_write_string("  Error: No se encontro el archivo.\n");
        return;
    }

    if (node->flags & FS_DIRECTORY) {
        terminal_write_string("  Error: Es un directorio.\n");
        kfree(node);
        return;
    }

    uint8_t* buffer = (uint8_t*)kmalloc(node->length + 1);
    if (!buffer) {
        terminal_write_string("  Error: No hay memoria para leer el archivo.\n");
        kfree(node);
        return;
    }

    ssize_t read = read_fs(node, 0, node->length, buffer);
    if (read > 0) {
        buffer[read] = '\0';
        terminal_write_string((char*)buffer);
        terminal_write_string("\n");
    }

    kfree(buffer);
    kfree(node);
}


