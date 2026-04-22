#include <stdint.h>
uint64_t pmm_get_total_ram(void) { return 1024 * 1024 * 1024; }
uint64_t pmm_get_free_ram(void) { return 512 * 1024 * 1024; }
uint64_t pmm_get_used_ram(void) { return 512 * 1024 * 1024; }
