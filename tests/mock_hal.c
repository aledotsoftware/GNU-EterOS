#include <hal.h>

void hal_cpu_enable_interrupts_and_halt(void) {}
void hal_interrupts_disable(void) {}
void hal_interrupts_enable(void) {}

uint64_t mock_timer_ticks = 0;
uint64_t hal_timer_ticks(void) {
    return mock_timer_ticks++;
}

void hal_cpu_halt(void) {}
void hal_cpu_reset(void) {}
