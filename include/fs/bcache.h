#ifndef FS_BCACHE_H
#define FS_BCACHE_H

#include <types.h>

/* Simple Block Cache */
/* Caches 512-byte sectors */

/* Initialize the block cache */
void bcache_init(void);

/*
 * Read a sector from cache.
 * Returns 0 on success (hit), -1 on miss.
 * If success, data is copied to buffer.
 */
int bcache_read(uint32_t volume_id, uint32_t sector, uint8_t *buffer);

/*
 * Write a sector to cache (and mark as valid).
 * This should be called after a successful disk read, or during a disk write.
 */
void bcache_write(uint32_t volume_id, uint32_t sector, const uint8_t *buffer);

/* Invalidate a specific sector in cache */
void bcache_invalidate(uint32_t volume_id, uint32_t sector);

/* Invalidate all sectors in cache (Panic/OOM cleanup) */
void bcache_invalidate_all(void);

#endif
