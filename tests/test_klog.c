#include <stdio.h>
/* Do NOT include system string.h or stdlib.h if shadowed, or handle shadowing */
/* We rely on klog.h -> hal.h -> types.h for size_t etc. */
#include "klog.h"
#include "hal.h"

/* Manual declarations */
void exit(int status);
char *strstr(const char *haystack, const char *needle);
char *strncat(char *dest, const char *src, size_t n);
size_t strlen(const char *s);
int printf(const char *format, ...);

/* ========================================================================= */
/* Mocks                                                                     */
/* ========================================================================= */

char debug_buffer[4096];
char console_buffer[4096];
int panic_called = 0;
/* We can't easily use setjmp/longjmp without system headers.
   We'll use a simple flag and avoid actual longjmp for panic test if possible,
   or just declare them. */
typedef struct { unsigned long __jb[8]; } jmp_buf[1];
int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

jmp_buf panic_jump;

void hal_debug_write(const char* str) {
    strncat(debug_buffer, str, sizeof(debug_buffer) - strlen(debug_buffer) - 1);
}

void hal_console_write(const char* str) {
    strncat(console_buffer, str, sizeof(console_buffer) - strlen(console_buffer) - 1);
}

void hal_cpu_halt(void) {
    panic_called = 1;
    longjmp(panic_jump, 1);
}

void hal_interrupts_disable(void) {
    /* No-op for test */
}

void hal_interrupts_enable(void) {
    /* No-op for test */
}

/* Stubs */
void hal_init(void) {}
void hal_irq_install(uint8_t vector, irq_handler_t handler) {}
void hal_cpu_reset(void) {}
void hal_console_init(void) {}
void hal_console_putchar(char c) {}
void hal_console_clear(void) {}
void hal_input_init(void) {}
char hal_input_getchar(void) { return 0; }
bool hal_input_available(void) { return false; }
void hal_timer_init(uint32_t hz) {}
uint64_t hal_timer_ticks(void) { return 0; }
void hal_debug_putchar(char c) {}

/* ========================================================================= */
/* Tests                                                                     */
/* ========================================================================= */

void reset_buffers() {
    debug_buffer[0] = '\0';
    console_buffer[0] = '\0';
    panic_called = 0;
}

void test_formatting() {
    printf("Running test_formatting...\n");
    reset_buffers();

    klog(KLOG_DEBUG, "Test %s %d 0x%x", "String", 123, 0xABC);

    printf("Debug Buffer: '%s'\n", debug_buffer);

    if (strstr(debug_buffer, "[DEBUG] Test String 123 0xabc") == NULL) {
        printf("FAIL: Expected '[DEBUG] Test String 123 0xabc', got '%s'\n", debug_buffer);
        exit(1);
    }
    printf("PASS\n");
}

void test_routing_debug() {
    printf("Running test_routing_debug...\n");
    reset_buffers();

    klog(KLOG_DEBUG, "Debug Message");

    if (strlen(debug_buffer) == 0) {
        printf("FAIL: Debug buffer empty\n");
        exit(1);
    }
    if (strlen(console_buffer) > 0) {
        printf("FAIL: Console buffer should be empty, got '%s'\n", console_buffer);
        exit(1);
    }
    printf("PASS\n");
}

void test_routing_info() {
    printf("Running test_routing_info...\n");
    reset_buffers();

    klog(KLOG_INFO, "Info Message");

    /* Should appear in CONSOLE buffer */
    if (strlen(console_buffer) == 0) {
        printf("FAIL: Console buffer empty\n");
        exit(1);
    }
    /* Should ALSO appear in DEBUG buffer now (for safety/persistence) */
    if (strlen(debug_buffer) == 0) {
        printf("FAIL: Debug buffer empty (should contain info log)\n");
        exit(1);
    }
    printf("PASS\n");
}

void test_panic() {
    printf("Running test_panic...\n");
    reset_buffers();

    if (setjmp(panic_jump) == 0) {
        klog(KLOG_PANIC, "Panic Message");
        printf("FAIL: klog(PANIC) did not halt/jump\n");
        exit(1);
    } else {
        /* Jumped back */
        if (!panic_called) {
            printf("FAIL: panic_called flag not set\n");
            exit(1);
        }
        if (strstr(console_buffer, "[PANIC] Panic Message") == NULL) {
            printf("FAIL: Panic message not found in console\n");
            exit(1);
        }
        printf("PASS\n");
    }
}

int main() {
    test_formatting();
    test_routing_debug();
    test_routing_info();
    test_panic();

    printf("All tests passed!\n");
    return 0;
}
