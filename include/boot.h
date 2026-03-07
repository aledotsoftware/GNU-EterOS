#ifndef BOOT_H
#define BOOT_H

#include "types.h"

/* Estructura de información pasada por el bootloader al kernel */
/* Ubicación fija en memoria: 0xA000 */
#define BOOT_INFO_ADDR 0x9E00

typedef struct {
    uint32_t signature;       /* "KBOT" (0x544F424B) */
    uint32_t mem_map_count;
    uint64_t mem_map_addr;
    uint64_t fb_addr;         /* Dirección física del Linear Framebuffer */
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;        /* Bytes por línea */
    uint32_t fb_bpp;          /* Bits por pixel */
    uint64_t initrd_addr;     /* Dirección física del Initrd */
    uint32_t initrd_size;     /* Tamaño en bytes */
} __attribute__((packed)) boot_info_t;

/* Función helper para obtener la info */
static inline boot_info_t* get_boot_info(void) {
    return (boot_info_t*)BOOT_INFO_ADDR;
}

#endif
