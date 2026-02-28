/**
 * éterOS - Implementación del Driver VGA
 * 
 * Driver para el modo texto VGA legacy.
 * Escribe directamente al buffer de video mapeado en 0xB8000.
 * Soporta scroll automático, cursor hardware, y colores.
 */

#include <vga.h>
#include <io.h>
#include <string.h>
#include <framebuffer.h>

/* ========================================================================= */
/* Estado interno del terminal                                               */
/* ========================================================================= */
static size_t   terminal_row;
static size_t   terminal_col;
static uint8_t  terminal_color;
static volatile uint16_t* terminal_buffer;
static bool     use_framebuffer = false;
static uint32_t fb_fg = 0xFFFFFFFF; // Blanco por defecto
static uint32_t fb_bg = 0xFF000000; // Negro por defecto

/*
 * Hardware Scrolling State
 * VGA Text Mode memory is 32KB (0x8000 bytes) at 0xB8000.
 * Screen is 80x25 characters (4000 bytes).
 * We can store multiple screens in video memory and change the Start Address.
 */
#define VGA_MEMORY_SIZE     0x8000
#define VGA_MEMORY_CHARS    (VGA_MEMORY_SIZE / 2)
static uint16_t vga_buffer_offset = 0; // Offset in characters/words from 0xB8000
static terminal_hook_t active_hook = (void*)0;
static bool terminal_silent = false;

void terminal_set_hook(terminal_hook_t hook) {
    active_hook = hook;
}

void terminal_set_silent(bool silent) {
    terminal_silent = silent;
}

/* ========================================================================= */
/* Funciones VGA Legacy (Privadas)                                           */
/* ========================================================================= */

static void vga_update_cursor(void) {
    if (use_framebuffer) return; 
    /* Cursor position is relative to start of video memory */
    uint16_t pos = (uint16_t)(vga_buffer_offset + terminal_row * VGA_WIDTH + terminal_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void vga_enable_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 0);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
}

static void vga_set_start_address(uint16_t offset) {
    outb(0x3D4, 0x0C);
    outb(0x3D5, (offset >> 8) & 0xFF);
    outb(0x3D4, 0x0D);
    outb(0x3D5, offset & 0xFF);
}

static void vga_scroll(void) {
    /* Check if next screen fits in VRAM */
    if (vga_buffer_offset + VGA_SIZE + VGA_WIDTH > VGA_MEMORY_CHARS) {
        /* Wrap around: Copy current visible screen (lines 1..24) to offset 0 */
        /* lines 1..24 of current screen become lines 0..23 of new screen at offset 0 */
        volatile uint16_t* vram_base = (volatile uint16_t*)VGA_BUFFER_ADDR;
        memmove((void*)vram_base, (const void*)(terminal_buffer + VGA_WIDTH), (VGA_SIZE - VGA_WIDTH) * sizeof(uint16_t));

        vga_buffer_offset = 0;
        terminal_buffer = vram_base;
    } else {
        /* Hardware scroll: Advance buffer offset by one line */
        vga_buffer_offset += VGA_WIDTH;
        terminal_buffer += VGA_WIDTH;
    }

    /* Update Hardware Start Address */
    vga_set_start_address(vga_buffer_offset);

    /* Clear the new last line (visible at the bottom) */
    /* terminal_buffer points to top-left of visible screen, so last line is at offset (VGA_HEIGHT - 1) * VGA_WIDTH */
    const size_t last_line_offset = (VGA_HEIGHT - 1) * VGA_WIDTH;
    memset16((uint16_t*)(terminal_buffer + last_line_offset), vga_entry(' ', terminal_color), VGA_WIDTH);
}

/* ========================================================================= */
/* Implementación de la API pública                                          */
/* ========================================================================= */

void terminal_initialize(boot_info_t* info) {
    (void)info; /* Framebuffer init moved to terminal_switch_to_framebuffer() */
    terminal_row    = 0;
    terminal_col    = 0;
    terminal_color  = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = (volatile uint16_t*)VGA_BUFFER_ADDR;

    /*
     * IMPORTANT: We do NOT initialize the framebuffer here because
     * framebuffer_init() calls hal_mem_map() which requires PMM to be
     * initialized first. During hal_init(), PMM is not yet ready.
     *
     * Instead, we always start in VGA text mode. The framebuffer will
     * be activated later by gfx_init() -> framebuffer_init() after
     * PMM, VMM, and the heap are all initialized.
     */
    use_framebuffer = false;

    /* Limpiar toda la pantalla VGA */
    vga_buffer_offset = 0;
    vga_set_start_address(0);
    terminal_buffer = (volatile uint16_t*)VGA_BUFFER_ADDR;

    memset16((uint16_t*)terminal_buffer, vga_entry(' ', terminal_color), VGA_SIZE);
    vga_enable_cursor();
    vga_update_cursor();
}

/* Switch terminal from VGA text mode to Framebuffer mode.
 * Must be called AFTER PMM, VMM, and mm_init are done. */
void terminal_switch_to_framebuffer(boot_info_t* info) {
    if (!info) return;
    if (info->signature != 0x544F424B || info->fb_addr == 0) return;

    framebuffer_init(info);
    use_framebuffer = true;
    framebuffer_clear(fb_bg);

    /* Re-position cursor to top */
    terminal_row = 0;
    terminal_col = 0;
}

void terminal_set_color(uint8_t color) {
    terminal_color = color;
    if (use_framebuffer) {
        // Convertir color VGA 4-bit a 32-bit (aproximado)
        static const uint32_t palette[] = {
            0xFF000000, 0xFF0000AA, 0xFF00AA00, 0xFF00AAAA,
            0xFFAA0000, 0xFFAA00AA, 0xFFAA5500, 0xFFAAAAAA,
            0xFF555555, 0xFF5555FF, 0xFF55FF55, 0xFF55FFFF,
            0xFFFF5555, 0xFFFF55FF, 0xFFFFFF55, 0xFFFFFFFF
        };
        fb_fg = palette[color & 0x0F];
        fb_bg = palette[(color >> 4) & 0x0F];
    }
}

void terminal_scroll(void) {
    if (use_framebuffer) {
        /* Optimized hardware scroll: Shift memory up and clear bottom line */
        framebuffer_scroll(16, fb_bg);

        // Mantenernos en la última fila (calculada como height / 16 - 1)
        // Usamos la misma lógica de altura que en _terminal_putchar
        size_t height = framebuffer_get_height() / 16;
        terminal_row = height - 1;
    } else {
        vga_scroll();
        terminal_row = VGA_HEIGHT - 1;
    }
}

static void _terminal_putchar(char c) {
    if (active_hook) {
        active_hook(c);
        /* Si hay hook activo, podríamos evitar escribir en pantalla VGA/FB para evitar "doble print" en la ventanita */
        /* O podemos dejar que pinte en fondo */
        // return;  <-- Si queremos silenciar el fondo
    }

    if (terminal_silent) return;

    size_t width = use_framebuffer ? (framebuffer_get_width() / 8) : VGA_WIDTH;
    size_t height = use_framebuffer ? (framebuffer_get_height() / 16) : VGA_HEIGHT;

    switch (c) {
        case '\n':
            terminal_col = 0;
            terminal_row++;
            break;
        case '\r':
            terminal_col = 0;
            break;
        case '\t':
            terminal_col = (terminal_col + 4) & ~3;
            if (terminal_col >= width) {
                terminal_col = 0;
                terminal_row++;
            }
            break;
        case '\b':
            if (terminal_col > 0) {
                terminal_col--;
                if (use_framebuffer) {
                     framebuffer_putchar(' ', terminal_col * 8, terminal_row * 16, fb_fg, fb_bg);
                } else {
                    size_t index = terminal_row * VGA_WIDTH + terminal_col;
                    terminal_buffer[index] = vga_entry(' ', terminal_color);
                }
            }
            break;
        default:
            if (use_framebuffer) {
                framebuffer_putchar(c, terminal_col * 8, terminal_row * 16, fb_fg, fb_bg);
            } else {
                size_t index = terminal_row * VGA_WIDTH + terminal_col;
                terminal_buffer[index] = vga_entry(c, terminal_color);
            }
            terminal_col++;
            if (terminal_col >= width) {
                terminal_col = 0;
                terminal_row++;
            }
            break;
    }

    if (terminal_row >= height) {
        terminal_scroll();
    }
}

void terminal_putchar(char c) {
    _terminal_putchar(c);
    if (!use_framebuffer) vga_update_cursor();
}

void terminal_write_string(const char* str) {
    while (*str) {
        _terminal_putchar(*str++);
    }
    if (!use_framebuffer) vga_update_cursor();
}

void terminal_write_colored(const char* str, vga_color_t fg, vga_color_t bg) {
    uint8_t saved_color = terminal_color;
    terminal_set_color(vga_entry_color(fg, bg)); // Actualiza fb_fg/bg también
    terminal_write_string(str);
    terminal_set_color(saved_color);
}

void terminal_clear(void) {
    if (use_framebuffer) {
        framebuffer_clear(fb_bg);
    } else {
        vga_buffer_offset = 0;
        vga_set_start_address(0);
        terminal_buffer = (volatile uint16_t*)VGA_BUFFER_ADDR;

        memset16((uint16_t*)terminal_buffer, vga_entry(' ', terminal_color), VGA_SIZE);
    }
    terminal_row = 0;
    terminal_col = 0;
    if (!use_framebuffer) vga_update_cursor();
}

void terminal_set_cursor(size_t x, size_t y) {
    size_t width = use_framebuffer ? (framebuffer_get_width() / 8) : VGA_WIDTH;
    size_t height = use_framebuffer ? (framebuffer_get_height() / 16) : VGA_HEIGHT;

    if (x < width && y < height) {
        terminal_col = x;
        terminal_row = y;
        if (!use_framebuffer) vga_update_cursor();
    }
}
