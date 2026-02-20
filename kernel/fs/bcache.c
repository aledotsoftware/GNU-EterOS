#include <fs/bcache.h>
#include <mm.h>
#include <string.h>
#include <lock.h>

#define BCACHE_SIZE 128 /* 128 * 512 = 64KB Cache */

typedef struct {
    uint32_t volume_id;
    uint32_t sector;
    uint8_t  data[512];
    uint8_t  valid;
    uint64_t last_access;
} bcache_entry_t;

static bcache_entry_t *bcache = NULL;
static spinlock_t bcache_lock;
static uint64_t access_counter = 0;

void bcache_init(void) {
    bcache = (bcache_entry_t*)kmalloc(sizeof(bcache_entry_t) * BCACHE_SIZE);
    if (bcache) {
        memset(bcache, 0, sizeof(bcache_entry_t) * BCACHE_SIZE);
    }
    bcache_lock = 0;
    access_counter = 0;
}

int bcache_read(uint32_t volume_id, uint32_t sector, uint8_t *buffer) {
    if (!bcache) return -1;

    spin_lock(&bcache_lock);
    for (int i = 0; i < BCACHE_SIZE; i++) {
        if (bcache[i].valid && bcache[i].volume_id == volume_id && bcache[i].sector == sector) {
            memcpy(buffer, bcache[i].data, 512);
            bcache[i].last_access = ++access_counter;
            spin_unlock(&bcache_lock);
            return 0;
        }
    }
    spin_unlock(&bcache_lock);
    return -1;
}

void bcache_write(uint32_t volume_id, uint32_t sector, const uint8_t *buffer) {
    if (!bcache) return;

    spin_lock(&bcache_lock);

    /* First, check if it's already in cache to update it */
    for (int i = 0; i < BCACHE_SIZE; i++) {
        if (bcache[i].valid && bcache[i].volume_id == volume_id && bcache[i].sector == sector) {
            memcpy(bcache[i].data, buffer, 512);
            bcache[i].last_access = ++access_counter;
            spin_unlock(&bcache_lock);
            return;
        }
    }

    /* Not in cache, find LRU or empty slot */
    int victim_idx = -1;
    uint64_t min_access = 0xFFFFFFFFFFFFFFFF;

    for (int i = 0; i < BCACHE_SIZE; i++) {
        if (!bcache[i].valid) {
            victim_idx = i;
            break;
        }
        if (bcache[i].last_access < min_access) {
            min_access = bcache[i].last_access;
            victim_idx = i;
        }
    }

    if (victim_idx != -1) {
        bcache[victim_idx].volume_id = volume_id;
        bcache[victim_idx].sector = sector;
        memcpy(bcache[victim_idx].data, buffer, 512);
        bcache[victim_idx].valid = 1;
        bcache[victim_idx].last_access = ++access_counter;
    }

    spin_unlock(&bcache_lock);
}

void bcache_invalidate(uint32_t volume_id, uint32_t sector) {
    if (!bcache) return;

    spin_lock(&bcache_lock);
    for (int i = 0; i < BCACHE_SIZE; i++) {
        if (bcache[i].valid && bcache[i].volume_id == volume_id && bcache[i].sector == sector) {
            bcache[i].valid = 0;
            break;
        }
    }
    spin_unlock(&bcache_lock);
}

void bcache_invalidate_all(void) {
    if (!bcache) return;

    if (spin_trylock(&bcache_lock)) {
        for (int i = 0; i < BCACHE_SIZE; i++) {
            bcache[i].valid = 0;
        }
        spin_unlock(&bcache_lock);
    }
}
