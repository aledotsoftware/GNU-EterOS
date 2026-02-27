#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

/* Mock buffer for UART registers (size 1024 bytes is enough) */
uint8_t uart0_mock_memory[1024];

/* Override the base address to point to our mock memory */
/* We need to cast the pointer to uintptr_t so it can be used in arithmetic */
#define ESP32_UART0_BASE ((uintptr_t)uart0_mock_memory)

/* Define ETEROS_HOST_TEST to avoid including incompatible headers */
#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

/* Mock types */
#ifndef ETEROS_TYPES_H
#define ETEROS_TYPES_H
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#endif

/* Mock HAL header */
#ifndef ETEROS_HAL_H
#define ETEROS_HAL_H
typedef void (*irq_handler_t)(void);

/* Declare types needed from hal_impl.c */
void hal_init(void);
void hal_cpu_halt(void);
void hal_cpu_reset(void);
void hal_console_init(void);
void hal_console_putchar(char c);
void hal_console_write(const char* str);
void hal_console_clear(void);
void hal_input_init(void);
char hal_input_getchar(void);
bool hal_input_available(void);
void hal_interrupts_enable(void);
void hal_interrupts_disable(void);
void hal_irq_install(uint8_t vector, irq_handler_t handler);
void hal_timer_init(uint32_t hz);
uint64_t hal_timer_ticks(void);
void hal_debug_putchar(char c);
void hal_debug_write(const char* str);
void wifi_init_stub(void);
void wifi_connect_stub(const char* ssid, const char* pass);
void ble_init_stub(void);
void mqtt_client_init_stub(void);

#endif /* ETEROS_HAL_H */

/*
 * IMPORTANT: We are compiling for the HOST (x86_64), but including code that contains
 * Xtensa assembly instructions (asm volatile). We need to macro them away or
 * conditionally compile them out in hal_impl.c.
 *
 * Since we can't easily modify hal_impl.c to use #ifndef __ETEROS_HOST_TEST__ around every asm block
 * without touching the file extensively, we will use a trick:
 * We will define macros for the inline assembly instructions if possible, OR
 * we will rely on the fact that we are including the file and we can use the preprocessor
 * to "comment out" the asm blocks if they are simple strings.
 *
 * However, the asm blocks are inside functions.
 * Better approach: Modify hal_impl.c to guard asm blocks with #ifndef __ETEROS_HOST_TEST__
 */

/*
 * Wait! I already modified hal_impl.c to make UART base overridable.
 * I should also modify it to guard ASM blocks.
 * But checking the plan, I only mentioned refactoring ESP32_UART0_BASE.
 *
 * Let's modify hal_impl.c to be host-test friendly for ASM as well.
 * But first, let's see if we can trick it with macros.
 * No, C macros cannot replace "asm volatile (...)".
 *
 * So I must modify hal_impl.c.
 */

/* Include the implementation file directly */
#include "../kernel/arch/xtensa/hal_impl.c"


/* Test Functions */

void test_hal_input_available_empty(void) {
    printf("Test: hal_input_available_empty... ");

    /* Clear status register (offset 0x1C) */
    /* RXFIFO_CNT is bits 0-7 */
    *((volatile uint32_t*)(uart0_mock_memory + 0x1C)) = 0x00000000;

    bool available = hal_input_available();
    if (available == false) {
        printf("PASSED\n");
    } else {
        printf("FAILED (expected false)\n");
    }
}

void test_hal_input_available_data(void) {
    printf("Test: hal_input_available_data... ");

    /* Set RXFIFO_CNT to 1 (bit 0) at offset 0x1C */
    *((volatile uint32_t*)(uart0_mock_memory + 0x1C)) = 0x00000001;

    bool available = hal_input_available();
    if (available == true) {
        printf("PASSED\n");
    } else {
        printf("FAILED (expected true)\n");
    }
}

void test_hal_input_getchar(void) {
    printf("Test: hal_input_getchar... ");

    /*
     * To test getchar without blocking forever, we must make sure
     * available returns true immediately.
     */

    /* Set RXFIFO_CNT to 1 */
    *((volatile uint32_t*)(uart0_mock_memory + 0x1C)) = 0x00000001;

    /* Put 'X' (0x58) in the FIFO (offset 0x00) */
    *((volatile uint32_t*)(uart0_mock_memory + 0x00)) = 0x00000058;

    char c = hal_input_getchar();
    if (c == 'X') {
        printf("PASSED\n");
    } else {
        printf("FAILED (got 0x%02x, expected 'X')\n", (unsigned char)c);
    }
}

int main(void) {
    printf("Running Xtensa UART Tests...\n");

    /* Clear mock memory */
    memset(uart0_mock_memory, 0, sizeof(uart0_mock_memory));

    test_hal_input_available_empty();
    test_hal_input_available_data();
    test_hal_input_getchar();

    return 0;
}
