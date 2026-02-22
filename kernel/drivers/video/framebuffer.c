#include <framebuffer.h>
#include <string.h>
#include <mm.h>
#include <serial.h>
#include <hal/mm.h>

#define FB_VIRT_ADDR 0xFFFFFFFFE0000000

static uint32_t* fb_buffer = 0;
static uint32_t* back_buffer = 0; /* Doble buffer */
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static uint32_t fb_bpp = 0;
static uint32_t fb_size_bytes = 0;

/* Cached pointer to current drawing buffer (front or back) */
static uint32_t* active_buffer = 0;

/* Dirty region tracking (inclusive min, exclusive max) */
static uint32_t dirty_min_x = 0;
static uint32_t dirty_min_y = 0;
static uint32_t dirty_max_x = 0;
static uint32_t dirty_max_y = 0;

/* Helper to mark region as dirty */
static void framebuffer_mark_dirty(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) return;

    uint32_t max_x = x + w;
    uint32_t max_y = y + h;

    if (dirty_min_x >= dirty_max_x) {
        /* First dirty rect */
        dirty_min_x = x;
        dirty_min_y = y;
        dirty_max_x = max_x;
        dirty_max_y = max_y;
    } else {
        /* Union */
        if (x < dirty_min_x) dirty_min_x = x;
        if (y < dirty_min_y) dirty_min_y = y;
        if (max_x > dirty_max_x) dirty_max_x = max_x;
        if (max_y > dirty_max_y) dirty_max_y = max_y;
    }
}

void framebuffer_init(boot_info_t* info) {
    if (!info || !info->fb_addr) return;
    if (fb_buffer) return; /* Already initialized */

    fb_width = info->fb_width;
    fb_height = info->fb_height;
    fb_pitch = info->fb_pitch;
    fb_bpp = info->fb_bpp;
    fb_size_bytes = fb_height * fb_pitch;

    /* Map Framebuffer with Write-Combining */
    uint64_t phys_addr = (uint64_t)info->fb_addr;

    /* Handle alignment */
    uint64_t phys_base = phys_addr & ~(HAL_PAGE_SIZE - 1);
    uint64_t offset = phys_addr - phys_base;
    uint64_t virt_base = FB_VIRT_ADDR;

    size_t total_size = fb_size_bytes + offset;
    size_t pages = (total_size + HAL_PAGE_SIZE - 1) / HAL_PAGE_SIZE;

#ifdef __ETEROS_HOST_TEST__
    /* In host tests, we use the provided address directly as it's a malloc pointer */
    fb_buffer = (uint32_t*)phys_addr;
    (void)virt_base;
    (void)pages;
#else
    for(size_t i = 0; i < pages; i++) {
         hal_mem_map(phys_base + i * HAL_PAGE_SIZE,
                     virt_base + i * HAL_PAGE_SIZE,
                     HAL_MEM_WRITE | HAL_MEM_WRITE_COMBINING);
    }

    fb_buffer = (uint32_t*)(virt_base + offset);
#endif
    
    char buf[64];
    serial_write_string("[FB] LFB Address: 0x");
    utoa_hex_s((uint64_t)phys_addr, buf, sizeof(buf));
    serial_write_string(" mapped to 0x");
    utoa_hex_s((uint64_t)fb_buffer, buf, sizeof(buf));
    serial_write_string(buf);
    serial_write_string("\n");

    /* Default to front buffer */
    active_buffer = fb_buffer;

    /* Initialize dirty rect to empty */
    dirty_min_x = fb_width; /* Invalid state: min >= max */
    dirty_max_x = 0;
    dirty_min_y = fb_height;
    dirty_max_y = 0;
}

uint32_t framebuffer_get_width(void) { return fb_width; }
uint32_t framebuffer_get_height(void) { return fb_height; }
uint32_t framebuffer_get_bpp(void) { return fb_bpp; }

uint32_t* framebuffer_get_buffer(void) { return active_buffer; }
uint32_t framebuffer_get_pitch(void) { return fb_pitch; }

void framebuffer_enable_double_buffer(void) {
    if (back_buffer) return;
    
    /* Asignar 3MB aprox para 1024x768x32 */
    back_buffer = (uint32_t*)kmalloc(fb_size_bytes);
    if (!back_buffer) return;
    
    /* Copiar contenido actual */
    memcpy(back_buffer, fb_buffer, fb_size_bytes);
    
    /* Switch drawing to back buffer */
    active_buffer = back_buffer;
}

void framebuffer_flush(void) {
    if (back_buffer && fb_buffer) {
        /* Check dirty region */
        if (dirty_min_x >= dirty_max_x) return;

        /* Clip to screen bounds */
        if (dirty_min_x >= fb_width) dirty_min_x = 0;
        if (dirty_min_y >= fb_height) dirty_min_y = 0;
        if (dirty_max_x > fb_width) dirty_max_x = fb_width;
        if (dirty_max_y > fb_height) dirty_max_y = fb_height;

        uint32_t w = dirty_max_x - dirty_min_x;
        uint32_t h = dirty_max_y - dirty_min_y;

        if (w == 0 || h == 0) {
             /* Reset and return */
             dirty_min_x = fb_width; dirty_max_x = 0;
             dirty_min_y = fb_height; dirty_max_y = 0;
             return;
        }

        /* If full screen update */
        if (w == fb_width && h == fb_height && fb_pitch == fb_width * (fb_bpp / 8)) {
             memcpy(fb_buffer, back_buffer, fb_size_bytes);
        } else {
             /* Partial update */
             size_t bpp_bytes = fb_bpp / 8;
             size_t row_len = w * bpp_bytes;

             uint8_t* dest = (uint8_t*)fb_buffer;
             uint8_t* src = (uint8_t*)back_buffer;

             /* Offset to starting Y */
             size_t y_offset = dirty_min_y * fb_pitch;
             dest += y_offset;
             src += y_offset;

             /* Offset to starting X */
             size_t x_offset = dirty_min_x * bpp_bytes;
             dest += x_offset;
             src += x_offset;

             for (uint32_t i = 0; i < h; i++) {
                 memcpy(dest, src, row_len);
                 dest += fb_pitch;
                 src += fb_pitch;
             }
        }

        /* Reset dirty region */
        dirty_min_x = fb_width; dirty_max_x = 0;
        dirty_min_y = fb_height; dirty_max_y = 0;
    }
}

void framebuffer_flush_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!back_buffer || !fb_buffer) return;

    /* Clip coords */
    if (x >= fb_width) return;
    if (y >= fb_height) return;
    if (x + w > fb_width) w = fb_width - x;
    if (y + h > fb_height) h = fb_height - y;

    if (w == 0 || h == 0) return;

    /* Copy row by row to support pitch */
    for (uint32_t i = 0; i < h; i++) {
        uint8_t* dest = (uint8_t*)fb_buffer + ((y + i) * fb_pitch) + (x * (fb_bpp / 8));
        uint8_t* src  = (uint8_t*)back_buffer + ((y + i) * fb_pitch) + (x * (fb_bpp / 8));
        size_t row_len = w * (fb_bpp / 8);
        
        memcpy(dest, src, row_len);
    }
}

void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    /* Fast path for bounds check removal? No, safety first. */
    if (x >= fb_width || y >= fb_height) return;
    
    /* Use cached active_buffer */
    if (!active_buffer) return;

    /* Mark pixel as dirty */
    framebuffer_mark_dirty(x, y, 1, 1);

    /* 32-bit optimization (Most common case) */
    if (fb_bpp == 32) {
         /* Assumption: fb_pitch is bytes. Pixel address = y*pitch + x*4 */
         /* Avoid multiplication if possible? No easy way without Y lookup table. */
         /* Compiler should optimize this multiply-add */
         uint32_t* pixel = (uint32_t*)((uint8_t*)active_buffer + (y * fb_pitch) + (x * 4));
         *pixel = color;
    } 
    else {
        /* Fallback for other depths */
        uint8_t* pixel_addr = (uint8_t*)active_buffer + (y * fb_pitch) + (x * (fb_bpp / 8));
        if (fb_bpp == 24) {
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
}

extern const uint8_t font8x16[];

void framebuffer_clear(uint32_t color) {
    if (!active_buffer) return;

    /* Mark full screen as dirty */
    framebuffer_mark_dirty(0, 0, fb_width, fb_height);

    /* Optimización para 32-bit */
    if (fb_bpp == 32) {
        if (fb_pitch == fb_width * 4) {
            memset32(active_buffer, color, fb_width * fb_height);
        } else {
            for (uint32_t y = 0; y < fb_height; y++) {
                uint32_t* row = (uint32_t*)((uint8_t*)active_buffer + (y * fb_pitch));
                memset32(row, color, fb_width);
            }
        }
    } else {
        /* Genérico */
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

    if (!active_buffer) return;

    /* Mark rect as dirty */
    framebuffer_mark_dirty(x, y, w, h);

    /* Optimized 32-bit path */
    if (fb_bpp == 32) {
        for (uint32_t i = 0; i < h; i++) {
            /* Calculate row pointer directly */
            uint32_t* row_ptr = (uint32_t*)((uint8_t*)active_buffer + ((y + i) * fb_pitch));
            row_ptr += x; /* Offset x */
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
    if (!active_buffer) return;

    /* Clip basic check */
    if (x >= fb_width || y >= fb_height - 16) return;

    /* Mark char bounds as dirty */
    framebuffer_mark_dirty(x, y, 8, 16);

    unsigned char uc = (unsigned char)c;
    const uint8_t* glyph = &font8x16[uc * 16];

    /* Optimized 32-bit drawing */
    if (fb_bpp == 32) {
        for (int row = 0; row < 16; row++) {
            /* Get pointer to start of row in buffer */
            uint32_t* row_ptr = (uint32_t*)((uint8_t*)active_buffer + ((y + row) * fb_pitch));
            row_ptr += x; /* Add X offset */
            
            uint8_t bits = glyph[row];
            
            /* Unroll loop manually for 8 pixels - massive speedup over loops + checks + calls */
            /* Bit 7 */
            *row_ptr++ = (bits & 0x80) ? fg : bg;
            /* Bit 6 */
            *row_ptr++ = (bits & 0x40) ? fg : bg;
            /* Bit 5 */
            *row_ptr++ = (bits & 0x20) ? fg : bg;
            /* Bit 4 */
            *row_ptr++ = (bits & 0x10) ? fg : bg;
            /* Bit 3 */
            *row_ptr++ = (bits & 0x08) ? fg : bg;
            /* Bit 2 */
            *row_ptr++ = (bits & 0x04) ? fg : bg;
            /* Bit 1 */
            *row_ptr++ = (bits & 0x02) ? fg : bg;
            /* Bit 0 */
            *row_ptr++ = (bits & 0x01) ? fg : bg;
        }
    } else {
        /* Fallback legacy loop */
        for (int row = 0; row < 16; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (bits & (1 << (7 - col))) {
                    framebuffer_putpixel(x + col, y + row, fg);
                } else {
                    framebuffer_putpixel(x + col, y + row, bg);
                }
            }
        }
    }
}
