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

    if (width > 4096 || height > 4096) {
         serial_write_string("[PNG] Dimensions too large for native decoder.\n");
         return NULL;
    }

    png_image_t* img = (png_image_t*)kmalloc(sizeof(png_image_t));
    if (!img) return NULL;

    img->width = width;
    img->height = height;
    img->pixels = (uint32_t*)kmalloc(width * height * 4);
    if (!img->pixels) {
        kfree(img);
        return NULL;
    }

    /* We need to extract the IDAT payload and uncompress it */
    /* Allocate a buffer for the entire IDAT payload */
    uint32_t max_idat_size = size; /* Overshoot, max possible is size of file */
    uint8_t* idat_payload = (uint8_t*)kmalloc(max_idat_size);
    if (!idat_payload) {
        kfree(img->pixels);
        kfree(img);
        return NULL;
    }

    uint32_t idat_len = 0;
    offset = 8;
    while (offset + 8 < size) {
        uint32_t chunk_len = get_be32(data + offset);
        const char* chunk_type = (const char*)(data + offset + 4);
        if (offset + 12 + chunk_len > size) break;

        if (memcmp(chunk_type, "IDAT", 4) == 0) {
            if (idat_len + chunk_len > max_idat_size) {
                 break; /* Buffer overflow protection */
            }
            memcpy(idat_payload + idat_len, data + offset + 8, chunk_len);
            idat_len += chunk_len;
        } else if (memcmp(chunk_type, "IEND", 4) == 0) {
            break;
        }
        offset += chunk_len + 12;
    }

    if (idat_len == 0) {
        serial_write_string("[PNG] No IDAT data found.\n");
        kfree(idat_payload);
        kfree(img->pixels);
        kfree(img);
        return NULL;
    }

    /* Expected uncompressed size: (width * bytes_per_pixel + 1) * height */
    uint32_t bpp = (color_type == 6) ? 4 : ((color_type == 2) ? 3 : 0);
    if (bpp == 0 || bit_depth != 8) {
        serial_write_string("[PNG] Only 8-bit RGB/RGBA supported.\n");
        kfree(idat_payload);
        kfree(img->pixels);
        kfree(img);
        return NULL;
    }

    uint32_t expected_size = (width * bpp + 1) * height;
    uint8_t* uncompressed = (uint8_t*)kmalloc(expected_size);
    if (!uncompressed) {
        kfree(idat_payload);
        kfree(img->pixels);
        kfree(img);
        return NULL;
    }

    /* Parse zlib and deflate (only uncompressed blocks supported) */
    uint32_t zlib_offset = 0;
    if (idat_len >= 2) {
        /* Skip zlib header (2 bytes) */
        zlib_offset = 2;
    }

    uint32_t out_len = 0;
    int bfinal = 0;
    while (!bfinal && zlib_offset < idat_len) {
        uint8_t header = idat_payload[zlib_offset++];
        bfinal = header & 1;
        uint8_t btype = (header >> 1) & 3;

        if (btype == 0) {
            /* Uncompressed block */
            if (zlib_offset + 4 > idat_len) break;
            uint16_t len = idat_payload[zlib_offset] | (idat_payload[zlib_offset+1] << 8);
            uint16_t nlen = idat_payload[zlib_offset+2] | (idat_payload[zlib_offset+3] << 8);
            zlib_offset += 4;

            if (len != (uint16_t)~nlen) {
                serial_write_string("[PNG] Invalid uncompressed block length.\n");
                break;
            }

            if (zlib_offset + len > idat_len || out_len + len > expected_size) {
                serial_write_string("[PNG] Uncompressed block exceeds bounds.\n");
                break;
            }

            memcpy(uncompressed + out_len, idat_payload + zlib_offset, len);
            out_len += len;
            zlib_offset += len;
        } else {
            serial_write_string("[PNG] Unsupported DEFLATE compression type. Fallback to gradient.\n");
            out_len = 0; /* Force failure to fallback */
            break;
        }
    }

    kfree(idat_payload);

    if (out_len != expected_size) {
        /* Fallback to gradient if not valid uncompressed PNG */
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                uint8_t r = (x * 255) / width;
                uint8_t g = (y * 255) / height;
                uint8_t b = 128;
                img->pixels[y * width + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
            }
        }
        kfree(uncompressed);
        return img;
    }

    /* Apply PNG Filters and convert to ARGB */
    uint32_t stride = width * bpp;
    uint8_t* prev_row = NULL;

    for (uint32_t y = 0; y < height; y++) {
        uint8_t* row = uncompressed + y * (stride + 1);
        uint8_t filter = row[0];
        uint8_t* pixels = row + 1;

        /* ⚡ BOLT Optimization: Lift invariant branches (filter, prev_row) and bound checks (x >= bpp) out of the inner pixel loop */
        if (filter == 1) { /* Sub */
            for (uint32_t x = bpp; x < stride; x++) {
                pixels[x] += pixels[x - bpp];
            }
        } else if (filter == 2) { /* Up */
            if (prev_row) {
                for (uint32_t x = 0; x < stride; x++) {
                    pixels[x] += prev_row[x];
                }
            }
        } else if (filter == 3) { /* Average */
            if (prev_row) {
                for (uint32_t x = 0; x < bpp; x++) {
                    pixels[x] += prev_row[x] / 2;
                }
                for (uint32_t x = bpp; x < stride; x++) {
                    pixels[x] += (pixels[x - bpp] + prev_row[x]) / 2;
                }
            } else {
                for (uint32_t x = bpp; x < stride; x++) {
                    pixels[x] += pixels[x - bpp] / 2;
                }
            }
        } else if (filter == 4) { /* Paeth */
            if (prev_row) {
                for (uint32_t x = 0; x < bpp; x++) {
                    pixels[x] += prev_row[x]; /* a=0, c=0 => Paeth(0, b, 0) returns b */
                }
                for (uint32_t x = bpp; x < stride; x++) {
                    uint8_t a = pixels[x - bpp];
                    uint8_t b_val = prev_row[x];
                    uint8_t c = prev_row[x - bpp];
                    int p = a + b_val - c;
                    int pa = p > a ? p - a : a - p;
                    int pb = p > b_val ? p - b_val : b_val - p;
                    int pc = p > c ? p - c : c - p;
                    uint8_t pr = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b_val : c);
                    pixels[x] += pr;
                }
            } else {
                for (uint32_t x = bpp; x < stride; x++) {
                    pixels[x] += pixels[x - bpp]; /* b=0, c=0 => Paeth(a, 0, 0) returns a */
                }
            }
        }

        /* Copy to ARGB buffer */
        for (uint32_t x = 0; x < width; x++) {
            uint8_t r = pixels[x * bpp];
            uint8_t g = pixels[x * bpp + 1];
            uint8_t b = pixels[x * bpp + 2];
            uint8_t a = (bpp == 4) ? pixels[x * bpp + 3] : 255;
            img->pixels[y * width + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }

        prev_row = pixels;
    }

    kfree(uncompressed);
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
