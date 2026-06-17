/**
 * éterOS - NVRAM / CMOS Driver
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 */

#include "nvram.h"
#include "io.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

uint8_t nvram_read(uint8_t reg) {
    // Write the register index to port 0x70
    // Note: This implementation does not preserve the NMI disable bit (bit 7)
    // If NMI handling becomes critical, we should read the current state or shadow it.
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

void nvram_write(uint8_t reg, uint8_t value) {
    outb(CMOS_ADDRESS, reg);
    outb(CMOS_DATA, value);
}

void nvram_set_boot_partition(uint8_t partition_id) {
    nvram_write(NVRAM_BOOT_PARTITION_REG, partition_id);
}

uint8_t nvram_get_boot_partition(void) {
    uint8_t val = nvram_read(NVRAM_BOOT_PARTITION_REG);
    if (val != 0 && val != 1) {
        return 0xFF;
    }
    return val;
}

void nvram_set_update_state(uint8_t state) {
    nvram_write(NVRAM_UPDATE_STATE_REG, state);
}

uint8_t nvram_get_update_state(void) {
    uint8_t val = nvram_read(NVRAM_UPDATE_STATE_REG);
    if (val > UPDATE_STATE_FAILED) {
        return UPDATE_STATE_NONE;
    }
    return val;
}
