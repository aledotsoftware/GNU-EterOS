#include <ui/image.h>
#include <fs/initrd.h>
#include <framebuffer.h>
#include <hal.h>
#include <serial.h>

#include "upng.h"
#include <mm.h>

void ui_draw_image(const char* filename, int x, int y) {
    uint32_t fileSize = 0;
    uint8_t* data = (uint8_t*)initrd_read_file(filename, &fileSize);
    
    if (!data) {
        serial_write_string("[UI] Error: Image not found: ");
        serial_write_string(filename);
        serial_write_string("\n");
        return;
    }

    /* Check PNG Header: 89 50 4E 47 0D 0A 1A 0A */
    if (fileSize > 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
        upng_t* upng = upng_new_from_bytes(data, fileSize);
        if (upng) {
            upng_decode(upng);
            if (upng_get_error(upng) == UPNG_EOK) {
                unsigned width = upng_get_width(upng);
                unsigned height = upng_get_height(upng);
                unsigned format = upng_get_format(upng);
                const uint8_t* buffer = upng_get_buffer(upng);
                
                /* Draw it! */
                /* Assuming standard RGBA/RGB */
                for (unsigned py = 0; py < height; py++) {
                    for (unsigned px = 0; px < width; px++) {
                        uint32_t color = 0;
                        if (format == UPNG_RGBA8) {
                            unsigned idx = (py * width + px) * 4;
                            uint8_t r = buffer[idx];
                            uint8_t g = buffer[idx+1];
                            uint8_t b = buffer[idx+2];
                            uint8_t a = buffer[idx+3];
                            /* Simple alpha blending check: if a < 128 skip */
                            if (a < 10) continue; 
                            color = (0xFF << 24) | (r << 16) | (g << 8) | b;
                        } else if (format == UPNG_RGB8) {
                            unsigned idx = (py * width + px) * 3;
                            uint8_t r = buffer[idx];
                            uint8_t g = buffer[idx+1];
                            uint8_t b = buffer[idx+2];
                            color = (0xFF << 24) | (r << 16) | (g << 8) | b;
                        } else {
                            /* Unsupported format fallback color */
                            color = 0xFF00FF;
                        }
                        
                        framebuffer_putpixel(x + px, y + py, color);
                    }
                }
            } else {
                serial_write_string("[UI] PNG Decode Error\n");
            }
            upng_free(upng);
        }
    } else {
        /* Legacy Raw Format: 4 bytes Width, 4 bytes Height, Pixels */
        if (fileSize < 8) return;
    
        uint32_t width = *(uint32_t*)data;
        uint32_t height = *(uint32_t*)(data + 4);
        uint32_t* pixels = (uint32_t*)(data + 8);
    
        /* Blit to framebuffer */
        for (uint32_t py = 0; py < height; py++) {
            for (uint32_t px = 0; px < width; px++) {
                uint32_t color = pixels[py * width + px];
                framebuffer_putpixel(x + px, y + py, color);
            }
        }
    }
}
