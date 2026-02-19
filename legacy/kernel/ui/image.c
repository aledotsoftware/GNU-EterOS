/*
 * éterOS - Image Loading & Rendering
 * 
 * Fase 5.1: Optimized to use Omni engine for direct buffer access
 * instead of per-pixel framebuffer_putpixel calls.
 */
#include <ui/image.h>
#include <fs/initrd.h>
#include <framebuffer.h>
#include <hal.h>
#include <serial.h>

#include "upng.h"
#include <mm.h>
#include <ui/omni.h>

void ui_draw_image(const char* filename, int x, int y) {
    /* Delegate to optimized Omni engine */
    omni_draw_image_from_path(filename, x, y);
}
