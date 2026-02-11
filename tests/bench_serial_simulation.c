#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

/* ========================================================================= */
/* Mock Hardware                                                             */
/* ========================================================================= */

#define UART_DATA           0
#define UART_INT_ENABLE     1
#define UART_FIFO_CTRL      2
#define UART_LINE_CTRL      3
#define UART_MODEM_CTRL     4
#define UART_LINE_STATUS    5
#define UART_MODEM_STATUS   6

#define LSR_TX_EMPTY        0x20

// Simulate UART state
typedef struct {
    uint64_t last_tx_time;
    uint64_t tx_delay_cycles; // Cycles per char
    bool     tx_empty;
} MockUART;

// 10000 cycles delay per char (arbitrary, represents slow I/O)
MockUART uart = {0, 10000, true};

static uint64_t current_cycles(void) {
    return __rdtsc();
}

static uint8_t inb_mock(uint16_t port) {
    // Only care about offset from base
    uint16_t offset = port & 0x7;

    if (offset == UART_LINE_STATUS) {
        uint64_t now = current_cycles();
        if (now - uart.last_tx_time > uart.tx_delay_cycles) {
            return LSR_TX_EMPTY;
        } else {
            return 0;
        }
    }
    return 0;
}

static void outb_mock(uint16_t port, uint8_t value) {
    uint16_t offset = port & 0x7;
    if (offset == UART_DATA) {
        uart.last_tx_time = current_cycles();
        uart.tx_empty = false;
    }
    (void)value;
}

/* ========================================================================= */
/* Old Implementation (Blocking)                                             */
/* ========================================================================= */

void serial_putchar_blocking(char c) {
    while (!(inb_mock(UART_LINE_STATUS) & LSR_TX_EMPTY));
    outb_mock(UART_DATA, c);
}

/* ========================================================================= */
/* New Implementation (Buffered + Interrupt Simulation)                      */
/* ========================================================================= */

#define SERIAL_BUFFER_SIZE 1024
static uint8_t tx_buffer[SERIAL_BUFFER_SIZE];
static uint16_t tx_head = 0;
static uint16_t tx_tail = 0;
static bool tx_busy = false;

// Simulate ISR behavior: call this periodically or when appropriate
void serial_isr_mock(void) {
    if (inb_mock(UART_LINE_STATUS) & LSR_TX_EMPTY) {
        if (tx_head != tx_tail) {
            outb_mock(UART_DATA, tx_buffer[tx_tail]);
            tx_tail = (tx_tail + 1) % SERIAL_BUFFER_SIZE;
            tx_busy = true;
        } else {
            tx_busy = false;
        }
    }
}

void serial_putchar_buffered(char c) {
    uint16_t next_head = (tx_head + 1) % SERIAL_BUFFER_SIZE;

    // Wait if buffer full
    while (next_head == tx_tail) {
        serial_isr_mock();
    }

    tx_buffer[tx_head] = c;
    tx_head = next_head;

    // If transmitter idle, kickstart
    if (!tx_busy) {
        serial_isr_mock();
    }
}

/* ========================================================================= */
/* Benchmark                                                                 */
/* ========================================================================= */

int main() {
    uint64_t start, end;
    const char* test_str = "This is a test string to measure serial performance. "
                           "It needs to be long enough to fill the hardware buffer and cause blocking.\n";
    int len = strlen(test_str);
    int iterations = 10;

    printf("Benchmarking Serial Output (String len: %d, Iterations: %d)\n", len, iterations);
    printf("Simulated UART delay: %lu cycles per char\n", uart.tx_delay_cycles);

    // ----------------------------------------------------------------
    // Measure Blocking
    // ----------------------------------------------------------------
    uart.last_tx_time = 0;
    start = current_cycles();
    for (int i = 0; i < iterations; i++) {
        for (int j = 0; j < len; j++) {
            serial_putchar_blocking(test_str[j]);
        }
    }
    end = current_cycles();
    uint64_t blocking_cycles = (end - start) / iterations;
    printf("Blocking Implementation: %lu cycles (avg)\n", blocking_cycles);

    // ----------------------------------------------------------------
    // Measure Buffered (Latency to return)
    // ----------------------------------------------------------------
    // Reset state
    uart.last_tx_time = 0;
    tx_head = 0;
    tx_tail = 0;
    tx_busy = false;

    uint64_t total_cycles = 0;

    for (int i = 0; i < iterations; i++) {
        uint64_t iter_start = current_cycles();
        for (int j = 0; j < len; j++) {
            serial_putchar_buffered(test_str[j]);
        }
        uint64_t iter_end = current_cycles();
        total_cycles += (iter_end - iter_start);

        // Simulate background drainage (so buffer is empty for next iteration)
        while (tx_head != tx_tail) serial_isr_mock();
    }

    uint64_t buffered_cycles = total_cycles / iterations;
    printf("Buffered Implementation: %lu cycles (avg)\n", buffered_cycles);

    if (buffered_cycles > 0)
        printf("Speedup (Application Latency): %.2fx\n", (double)blocking_cycles / buffered_cycles);

    return 0;
}
