#define __ETEROS_HOST_TEST__

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "../include/serial.h"
#include "../include/io.h"

/* Mock Hardware State */
static uint8_t registers[8];
static int mock_interrupts_enabled_val = 1;
static uint64_t mock_flags = 0x200; // IF set
static char last_out_char = 0;
static int out_char_count = 0;

/* Mock Implementation */
uint8_t inb(uint16_t port) {
    uint16_t offset = port & 0x7;
    /* Simulate LSR always ready for TX */
    if (offset == 5) { // UART_LINE_STATUS
        return 0x20; // LSR_TX_EMPTY
    }
    return registers[offset];
}

void outb(uint16_t port, uint8_t value) {
    uint16_t offset = port & 0x7;
    registers[offset] = value;
    if (offset == 0) { // UART_DATA
        last_out_char = (char)value;
        out_char_count++;
    }
}

// Unused mocks
void outw(uint16_t port, uint16_t value) { (void)port; (void)value; }
void outl(uint16_t port, uint32_t value) { (void)port; (void)value; }
uint16_t inw(uint16_t port) { (void)port; return 0; }
uint32_t inl(uint16_t port) { (void)port; return 0; }
void io_wait(void) {}
void cpu_halt(void) {}

/* Mock Interrupt Control */
int interrupts_enabled(void) {
    return mock_interrupts_enabled_val;
}

uint64_t irq_save(void) {
    uint64_t flags = mock_flags;
    mock_interrupts_enabled_val = 0;
    mock_flags &= ~0x200;
    return flags;
}

void irq_restore(uint64_t flags) {
    mock_flags = flags;
    if (flags & 0x200) {
        mock_interrupts_enabled_val = 1;
    }
}

/* Include source directly to access internal state (buffers) */
#include "../kernel/drivers/serial/serial.c"

/* Reset Helper */
void reset_test_state() {
    memset(registers, 0, sizeof(registers));
    tx_head = 0;
    tx_tail = 0;
    rx_head = 0;
    rx_tail = 0;
    last_out_char = 0;
    out_char_count = 0;
    mock_interrupts_enabled_val = 1;
    mock_flags = 0x200;
}

int main() {
    printf("Running Serial Hybrid Mode Tests...\n");

    /* Test 1: Buffered Mode */
    {
        printf("Test 1: Buffered Mode (Interrupts Enabled)... ");
        reset_test_state();

        serial_putchar('A');

        /* Expectation: 'A' in buffer, head moved, outb NOT called for data (handled by ISR/start logic) */

        assert(tx_head == 1);
        assert(tx_buffer[0] == 'A');
        assert(out_char_count == 0); /* Data not sent to port 0 yet */

        /* Check that IER was updated to enable TX interrupt */
        assert(registers[1] & 0x02); /* UART_INT_ENABLE bit 1 (IER_TX_EMPTY) */

        printf("PASS\n");
    }

    /* Test 2: Polled Mode */
    {
        printf("Test 2: Polled Mode (Interrupts Disabled)... ");
        reset_test_state();
        mock_interrupts_enabled_val = 0;
        mock_flags = 0;

        serial_putchar('B');

        /* Expectation: 'B' sent immediately via outb, buffer empty */
        assert(tx_head == 0);
        assert(out_char_count == 1);
        assert(last_out_char == 'B');

        printf("PASS\n");
    }

    /* Test 3: Hybrid Flush */
    {
        printf("Test 3: Hybrid Flush (Enabled -> Disabled)... ");
        reset_test_state();

        /* Step 1: Buffer some data */
        serial_putchar('C');
        assert(tx_head == 1);
        assert(out_char_count == 0);

        /* Step 2: Disable interrupts and write another char */
        mock_interrupts_enabled_val = 0;
        mock_flags = 0;

        serial_putchar('D');

        /* Expectation: Buffer flushed ('C' sent), then 'D' sent. Total 2 chars. */

        assert(out_char_count == 2);
        assert(last_out_char == 'D'); /* Last one sent */
        assert(tx_head == tx_tail); /* Buffer flushed */

        printf("PASS\n");
    }

    printf("All tests passed successfully.\n");
    return 0;
}
