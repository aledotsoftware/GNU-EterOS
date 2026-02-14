#include <framebuffer.h>
#include <string.h>
#include <mm.h>

static uint32_t* fb_buffer = 0;
static uint32_t* back_buffer = 0; /* Doble buffer */
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static uint32_t fb_bpp = 0;
static uint32_t fb_size_bytes = 0;

void framebuffer_init(boot_info_t* info) {
    if (!info || !info->fb_addr) return;

    fb_buffer = (uint32_t*)((uint64_t)info->fb_addr);
    fb_width = info->fb_width;
    fb_height = info->fb_height;
    fb_pitch = info->fb_pitch;
    fb_bpp = info->fb_bpp;
    
    fb_size_bytes = fb_height * fb_pitch;
}

uint32_t framebuffer_get_width(void) { return fb_width; }
uint32_t framebuffer_get_height(void) { return fb_height; }


void framebuffer_enable_double_buffer(void) {
    if (back_buffer) return;
    
    /* Asignar 3MB aprox para 1024x768x32 */
    back_buffer = (uint32_t*)kmalloc(fb_size_bytes);
    if (!back_buffer) return;
    
    /* Copiar contenido actual */
    memcpy(back_buffer, fb_buffer, fb_size_bytes);
}

void framebuffer_flush(void) {
    if (back_buffer && fb_buffer) {
        memcpy(fb_buffer, back_buffer, fb_size_bytes);
    }
}

void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb_width || y >= fb_height) return;
    
    /* Si hay doble buffer, dibujar ahí. Si no, directo a VRAM */
    uint8_t* buffer_start = (uint8_t*)(back_buffer ? back_buffer : fb_buffer);
    if (!buffer_start) return;

    uint8_t* pixel_addr = buffer_start + (y * fb_pitch) + (x * (fb_bpp / 8));

    if (fb_bpp == 32) {
        *(uint32_t*)pixel_addr = color;
    } 
    else if (fb_bpp == 24) {
        pixel_addr[0] = (color) & 0xFF;
        pixel_addr[1] = (color >> 8) & 0xFF;
        pixel_addr[2] = (color >> 16) & 0xFF;
    }
    else if (fb_bpp == 16) {
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = (color) & 0xFF;
        uint16_t c = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        *(uint16_t*)pixel_addr = c;
    }
}

extern const uint8_t font8x16[];

void framebuffer_clear(uint32_t color) {
    uint32_t* target_buffer = back_buffer ? back_buffer : fb_buffer;
    if (!target_buffer) return;

    /* Optimización para 32-bit */
    if (fb_bpp == 32) {
        if (fb_pitch == fb_width * 4) {
            memset32(target_buffer, color, fb_width * fb_height);
        } else {
            for (uint32_t y = 0; y < fb_height; y++) {
                uint32_t* row = (uint32_t*)((uint8_t*)target_buffer + (y * fb_pitch));
                memset32(row, color, fb_width);
            }
        }
    } else {
        /* Genérico (lento pero seguro) */
        for (uint32_t y = 0; y < fb_height; y++) {
            for (uint32_t x = 0; x < fb_width; x++) {
                framebuffer_putpixel(x, y, color);
            }
        }
    }
}

void framebuffer_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    /* Safety Clip */
    if (x >= fb_width || y >= fb_height) return;
    if (x + w > fb_width) w = fb_width - x;
    if (y + h > fb_height) h = fb_height - y;

    uint32_t* target_buffer = back_buffer ? back_buffer : fb_buffer;
    if (!target_buffer) return;

    /* Optimized 32-bit path */
    if (fb_bpp == 32) {
        for (uint32_t i = 0; i < h; i++) {
            uint32_t* row_ptr = (uint32_t*)((uint8_t*)target_buffer + ((y + i) * fb_pitch)) + x;
            memset32(row_ptr, color, w);
        }
    } else {
        /* Fallback */
        for (uint32_t i = 0; i < h; i++) {
            for (uint32_t j = 0; j < w; j++) {
                framebuffer_putpixel(x + j, y + i, color);
            }
        }
    }
}

extern const uint8_t font8x16[];

void framebuffer_putchar(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    if (!fb_buffer) return;

    /* Usar unsigned char para acceder a los 256 caracteres del font estándar VGA */
    unsigned char uc = (unsigned char)c;

    /* Puntero al bitmap del caracter (16 bytes por char) */
    const uint8_t* glyph = &font8x16[uc * 16];

    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            /* Bit 7 es el pixel más a la izquierda */
            if (bits & (1 << (7 - col))) {
                framebuffer_putpixel(x + col, y + row, fg);
            } else {
                framebuffer_putpixel(x + col, y + row, bg);
            }
        }
    }
}
