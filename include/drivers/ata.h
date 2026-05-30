#ifndef DRIVERS_ATA_H
#define DRIVERS_ATA_H

#include <types.h>

void ata_init(void);
int ata_read_sector(uint32_t lba, uint8_t *buffer);
int ata_write_sector(uint32_t lba, uint8_t *buffer);

#endif
