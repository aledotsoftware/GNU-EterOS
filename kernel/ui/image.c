#include <ui/image.h>
#include <fs/initrd.h>
#include <framebuffer.h>
#include <hal.h>

void ui_draw_image(const char* filename, int x, int y) {
    uint32_t fileSize = 0;
    uint8_t* data = (uint8_t*)initrd_read_file(filename, &fileSize);
    
    if (!data) {
        hal_console_write("[UI] Error: Image not found: ");
        hal_console_write(filename);
        hal_console_write("\n");
        return;
    }

    /* Format: 4 bytes Width, 4 bytes Height, Pixels */
    if (fileSize < 8) return;

    uint32_t width = *(uint32_t*)data;
    uint32_t height = *(uint32_t*)(data + 4);
    uint32_t* pixels = (uint32_t*)(data + 8);

    /* Blit to framebuffer */
    for (uint32_t py = 0; py < height; py++) {
        for (uint32_t px = 0; px < width; px++) {
            uint32_t color = pixels[py * width + px];
            /* Simple transparency: skip 0x00000000 or similar if needed, 
               but for now just blit */
            framebuffer_putpixel(x + px, y + py, color);
        }
    }
}
