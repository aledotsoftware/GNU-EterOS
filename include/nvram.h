#ifndef NVRAM_H
#define NVRAM_H

#include "types.h"

// Register to store the boot partition flag (0x00-0x7F)
// Using 0x50 as it is typically unused by standard BIOS/RTC
#define NVRAM_BOOT_PARTITION_REG 0x50
#define NVRAM_UPDATE_STATE_REG   0x51

#define UPDATE_STATE_NONE    0
#define UPDATE_STATE_PENDING 1
#define UPDATE_STATE_SUCCESS 2
#define UPDATE_STATE_FAILED  3

/**
 * Reads a byte from the NVRAM (CMOS).
 * @param reg The register index to read from.
 * @return The value at the specified register.
 */
uint8_t nvram_read(uint8_t reg);

/**
 * Writes a byte to the NVRAM (CMOS).
 * @param reg The register index to write to.
 * @param value The value to write.
 */
void nvram_write(uint8_t reg, uint8_t value);

/**
 * Sets the boot partition flag in NVRAM.
 * @param partition_id The partition ID to set as active/boot.
 */
void nvram_set_boot_partition(uint8_t partition_id);

/**
 * Gets the current boot partition flag from NVRAM.
 * @return The partition ID currently set as active/boot.
 */
uint8_t nvram_get_boot_partition(void);

/**
 * Sets the OTA update state.
 * @param state The new state (e.g., UPDATE_STATE_PENDING).
 */
void nvram_set_update_state(uint8_t state);

/**
 * Gets the current OTA update state.
 * @return The current state.
 */
uint8_t nvram_get_update_state(void);

#endif // NVRAM_H
