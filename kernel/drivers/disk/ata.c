#include <drivers/ata.h>
#include <hal.h>
#include <io.h>
#include <klog.h>

#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERR          0x1F1
#define ATA_PRIMARY_SECCOUNT     0x1F2
#define ATA_PRIMARY_LBA_LO       0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HI       0x1F5
#define ATA_PRIMARY_DRV_HEAD     0x1F6
#define ATA_PRIMARY_STATUS       0x1F7
#define ATA_PRIMARY_CMD          0x1F7

#define ATA_CMD_READ_PIO         0x20
#define ATA_CMD_WRITE_PIO        0x30
#define ATA_CMD_CACHE_FLUSH      0xE7

#define ATA_SR_BSY               0x80    // Busy
#define ATA_SR_DRDY              0x40    // Drive ready
#define ATA_SR_DF                0x20    // Drive write fault
#define ATA_SR_DSC               0x10    // Drive seek complete
#define ATA_SR_DRQ               0x08    // Data request ready
#define ATA_SR_CORR              0x04    // Corrected data
#define ATA_SR_IDX               0x02    // Index
#define ATA_SR_ERR               0x01    // Error

static void ata_wait_bsy(void) {
    while (inb(ATA_PRIMARY_STATUS) & ATA_SR_BSY);
}

static void ata_wait_drq(void) {
    while (!(inb(ATA_PRIMARY_STATUS) & ATA_SR_DRQ));
}

void ata_init(void) {
    klog(KLOG_INFO, "[ATA] Primary IDE ATA driver initialized\n");
}

int ata_read_sector(uint32_t lba, uint8_t *buffer) {
    ata_wait_bsy();
    
    // Select drive (Master) and LBA high 4 bits
    outb(ATA_PRIMARY_DRV_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    
    // Send Sector Count and LBA
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    
    // Send Read Command
    outb(ATA_PRIMARY_CMD, ATA_CMD_READ_PIO);
    
    // Wait for drive to be ready with data
    ata_wait_bsy();
    ata_wait_drq();
    
    // Read 256 words (512 bytes)
    for (int i = 0; i < 256; i++) {
        uint16_t word = inw(ATA_PRIMARY_DATA);
        buffer[i * 2] = (uint8_t)word;
        buffer[i * 2 + 1] = (uint8_t)(word >> 8);
    }
    
    return 0;
}

int ata_write_sector(uint32_t lba, uint8_t *buffer) {
    ata_wait_bsy();
    
    // Select drive (Master) and LBA high 4 bits
    outb(ATA_PRIMARY_DRV_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    
    // Send Sector Count and LBA
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    
    // Send Write Command
    outb(ATA_PRIMARY_CMD, ATA_CMD_WRITE_PIO);
    
    // Wait for drive to be ready to accept data
    ata_wait_bsy();
    ata_wait_drq();
    
    // Write 256 words (512 bytes)
    for (int i = 0; i < 256; i++) {
        uint16_t word = buffer[i * 2] | ((uint16_t)buffer[i * 2 + 1] << 8);
        outw(ATA_PRIMARY_DATA, word);
    }
    
    // Cache flush
    outb(ATA_PRIMARY_CMD, ATA_CMD_CACHE_FLUSH);
    ata_wait_bsy();
    
    return 0;
}
