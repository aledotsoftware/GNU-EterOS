#include <gfx/png.h>
#include <mm.h>
#include <string.h>
#include <serial.h>

/* Helper for endianness */
static uint32_t get_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

png_image_t* png_decode(const uint8_t* data, size_t size) {
    if (!data || size < 33) {
        serial_write_string("[PNG] Invalid or too small PNG file.\n");
        return NULL;
    }

    /* Check PNG magic */
    const uint8_t magic[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (memcmp(data, magic, 8) != 0) {
        serial_write_string("[PNG] Invalid signature.\n");
        return NULL;
    }

    size_t offset = 8;
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t bit_depth = 0;
    uint8_t color_type = 0;

    /* Parse chunks */
    while (offset + 8 < size) {
        uint32_t chunk_len = get_be32(data + offset);
        const char* chunk_type = (const char*)(data + offset + 4);

        if (offset + 12 + chunk_len > size) {
            serial_write_string("[PNG] Chunk extends beyond EOF.\n");
            break;
        }

        if (memcmp(chunk_type, "IHDR", 4) == 0) {
            if (chunk_len < 13) break;
            width = get_be32(data + offset + 8);
            height = get_be32(data + offset + 12);
            bit_depth = data[offset + 16];
            color_type = data[offset + 17];

            (void)bit_depth;
            (void)color_type;

            /* Debug info */
            char buf[64];
            serial_write_string("[PNG] Extracted IHDR: ");
            itoa_s(width, buf, 64, 10);
            serial_write_string(buf);
            serial_write_string("x");
            itoa_s(height, buf, 64, 10);
            serial_write_string(buf);
            serial_write_string("\n");
        } else if (memcmp(chunk_type, "IDAT", 4) == 0) {
            /* IDAT reading */
            /* In a real scenario, this would involve Zlib decompression and PNG filter reversing.
               For this baremetal test/stub, we just log and skip to demonstrate architecture. */
            serial_write_string("[PNG] Found IDAT chunk.\n");
        } else if (memcmp(chunk_type, "IEND", 4) == 0) {
            break;
        }

        offset += chunk_len + 12; /* length(4) + type(4) + data(chunk_len) + CRC(4) */
    }

    if (width == 0 || height == 0) {
        return NULL;
    }

    /* Provide a dummy image buffer filled with a recognizable pattern since we don't have zlib */
    png_image_t* img = (png_image_t*)kmalloc(sizeof(png_image_t));
    if (!img) return NULL;

    img->width = width;
    img->height = height;

    /* Avoid allocating massive buffers if the dimensions are garbage/huge */
    if (width > 4096 || height > 4096) {
         serial_write_string("[PNG] Dimensions too large for native decoder.\n");
         kfree(img);
         return NULL;
    }

    img->pixels = (uint32_t*)kmalloc(width * height * 4);
    if (!img->pixels) {
        kfree(img);
        return NULL;
    }

    /* Attempt to extract uncompressed IDAT blocks if they exist */
    offset = 8;
    size_t pixel_idx = 0;
    size_t total_pixels = width * height;

    size_t scanline_bytes = width * 4;
    size_t uncompressed_idx = 0;

    while (offset + 8 < size) {
        uint32_t chunk_len = get_be32(data + offset);
        const char* chunk_type = (const char*)(data + offset + 4);

        if (offset + 12 > size || chunk_len > size - offset - 12) break;

        if (memcmp(chunk_type, "IDAT", 4) == 0) {
            /* Basic parser for uncompressed DEFLATE blocks (type 0) */
            const uint8_t* idat_data = data + offset + 8;
            if (chunk_len > 2) {
                size_t idat_offset = 2; /* skip zlib header */
                while (idat_offset < chunk_len) {
                    uint8_t header = idat_data[idat_offset];
                    uint8_t bfinal = header & 1;
                    uint8_t btype = (header >> 1) & 3;

                    if (btype == 0 && idat_offset + 5 <= chunk_len) {
                        /* Uncompressed block */
                        idat_offset++;
                        uint16_t len = idat_data[idat_offset] | (idat_data[idat_offset + 1] << 8);
                        idat_offset += 4; /* skip len and nlen */

                        if (idat_offset + len <= chunk_len) {
                            /* Parse raw RGBA pixels, accounting for 1 byte row filter per scanline */
                            for (size_t i = 0; i < len; i++) {
                                if (uncompressed_idx % (scanline_bytes + 1) == 0) {
                                    /* Skip row filter byte */
                                    uncompressed_idx++;
                                    continue;
                                }

                                if (pixel_idx < total_pixels && i + 3 < len) {
                                    /* Every 4th uncompressed byte (excluding filter byte) we have a full pixel */
                                    if ((uncompressed_idx % (scanline_bytes + 1)) % 4 == 1) {
                                        uint8_t r = idat_data[idat_offset + i];
                                        uint8_t g = idat_data[idat_offset + i + 1];
                                        uint8_t b = idat_data[idat_offset + i + 2];
                                        uint8_t a = idat_data[idat_offset + i + 3];
                                        img->pixels[pixel_idx++] = (a << 24) | (r << 16) | (g << 8) | b;
                                        i += 3;
                                        uncompressed_idx += 3;
                                    }
                                }
                                uncompressed_idx++;
                            }
                        }
                        idat_offset += len;
                        if (bfinal) break;
                    } else {
                        /* Compressed data, abort parsing */
                        break;
                    }
                }
            }
        }
        offset += chunk_len + 12;
    }

    /* Fallback if no valid uncompressed pixels were read */
    if (pixel_idx == 0) {
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                uint8_t r = (x * 255) / width;
                uint8_t g = (y * 255) / height;
                uint8_t b = 128;
                img->pixels[y * width + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
            }
        }
    }

    return img;
}

void png_free(png_image_t* img) {
    if (img) {
        if (img->pixels) {
            kfree(img->pixels);
        }
        kfree(img);
    }
}
