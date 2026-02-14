#include "lwip/sys.h"
#include "timer.h"

u32_t sys_now(void) {
    /* 1 tick = 10ms (100 Hz). Result in ms. */
    return (u32_t)(timer_get_ticks() * 10);
}

void sys_init(void) {
    /* Timer already initialized in HAL */
}
