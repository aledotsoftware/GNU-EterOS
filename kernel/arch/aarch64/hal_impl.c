
#include <hal.h>
#include <types.h>

/**
 * ROCKCHIP RK3566 UART2 Base Address
 * Default debug UART for many boards (Quartz64, etc.)
 */
#define UART2_BASE  0xFE660000

/* Register Offsets (32-bit stride) */
#define UART_THR    0x00
#define UART_RBR    0x00
#define UART_LSR    0x14

/* LSR Bits */
#define UART_LSR_THRE 0x20

static volatile uint32_t* uart_base = (volatile uint32_t*)UART2_BASE;

void hal_init(void) {
    /* Initialize UART? Usually done by U-Boot/TF-A */
    /* We just assume it's working for early print */
}

void hal_console_write(const char* str) {
    while (*str) {
        /* Wait for THRE (Transmitter Holding Register Empty) */
        while (!((*(uart_base + (UART_LSR/4))) & UART_LSR_THRE)) {
            /* Spin */
        }
        
        /* Write char */
        *(uart_base + (UART_THR/4)) = *str;
        
        if (*str == '\n') {
             /* Handle \n */
             while (!((*(uart_base + (UART_LSR/4))) & UART_LSR_THRE));
             *(uart_base + (UART_THR/4)) = '\r';
        }
        str++;
    }
}

void hal_console_write_color(const char* str, uint8_t color) {
    (void)color;
    hal_console_write(str);
}

void hal_interrupts_enable(void) {
    __asm__ volatile("msr daifclr, #2"); /* Enable IRQ */
}

void hal_interrupts_disable(void) {
    __asm__ volatile("msr daifset, #2"); /* Disable IRQ */
}

void hal_cpu_halt(void) {
    __asm__ volatile("wfi");
}

void hal_cpu_enable_interrupts_and_halt(void) {
    hal_interrupts_enable();
    hal_cpu_halt();
}

void hal_halt(void) {
    for(;;) { hal_cpu_halt(); }
}

void hal_debug_write(const char* str) {
    hal_console_write(str);
}
