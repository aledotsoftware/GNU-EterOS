#define __ETEROS_HOST_TEST__ 1

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

/* Forward declare task_t so we can use it in mock before including syscall.c */
#include "../include/types.h"
#include "../include/task.h"

/* Global Mock State */
task_t current_task_mock;

/* Mocks */
task_t* task_get_current(void) {
    return &current_task_mock;
}

void task_exit(void) {
    printf("task_exit called\n");
}

void task_yield(void) {
    printf("task_yield called\n");
}

/* keyboard.h is included by syscall.c. We mock the function. */
char keyboard_getchar(void) {
    return 'A'; /* Mock input */
}

/* vga.h is included. */
void terminal_putchar(char c) {
    putchar(c);
}

/* serial.h */
void serial_write_string(const char* s) {
    printf("[SERIAL] %s", s);
}

void serial_putchar(char c) {
    putchar(c);
}

/* string.h is included by syscall.c. */
/* Our string.h declares itoa_s. We implement it. */
void itoa_s(int64_t value, char* str, size_t size, int base) {
    snprintf(str, size, "%lld", (long long)value);
}

void utoa_hex_s(uint64_t value, char* str, size_t size) {
    snprintf(str, size, "%llx", (unsigned long long)value);
}

/* io.h */
void wrmsr(uint32_t msr, uint64_t val) {
    printf("wrmsr(0x%x, 0x%lx)\n", msr, val);
}

uint64_t rdmsr(uint32_t msr) {
    printf("rdmsr(0x%x)\n", msr);
    return 0;
}

/* Mock for syscall_entry (referenced by syscall_init) */
void syscall_entry(void) {
    printf("syscall_entry referenced\n");
}

/* Mock for memset/memcpy because string.h renames them */
void* eteros_memset(void* dest, int c, size_t n) {
    char* d = (char*)dest;
    while(n--) *d++ = (char)c;
    return dest;
}

void* eteros_memcpy(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while(n--) *d++ = *s++;
    return dest;
}

size_t eteros_strlen(const char* s) {
    size_t len = 0;
    while(*s++) len++;
    return len;
}

/* Include source under test */
#include "../kernel/arch/x86_64/syscall.c"

int main() {
    struct syscall_regs regs;

    /* Setup mock task */
    eteros_memset(&current_task_mock, 0, sizeof(task_t));
    current_task_mock.id = 1234;

    printf("Testing Syscall Dispatcher...\n");

    /* Init check */
    syscall_init();

    /* Test SYS_getpid - Not implemented in x86_64 syscall.c yet */
    /*
    regs.rax = SYS_getpid;
    syscall_handler(&regs);
    printf("SYS_getpid returned: %lld\n", (long long)regs.rax);
    assert(regs.rax == 1234);
    */

    /* Test SYS_write */
    char msg[] = "Hello";
    regs.rax = SYS_write;
    regs.rdi = 1; /* stdout */
    regs.rsi = (uint64_t)msg;
    regs.rdx = 5;
    printf("Output: ");
    syscall_handler(&regs);
    printf("\n");
    assert(regs.rax == 5);
    printf("SYS_write passed\n");

    /* Test SYS_read (stdin) - Not implemented yet */
    /*
    char buf[10];
    regs.rax = SYS_read;
    regs.rdi = 0; // stdin
    regs.rsi = (uint64_t)buf;
    regs.rdx = 5;
    syscall_handler(&regs);
    assert(regs.rax == 5);
    assert(buf[0] == 'A');
    printf("SYS_read passed\n");
    */

    /* Test ENOSYS */
    regs.rax = 999;
    syscall_handler(&regs);
    /* In syscall.c we don't have errno.h, so we used -38 cast to uint64_t */
    /* -38 in 64-bit unsigned is large number */
    printf("Unknown syscall returned: %lld (expected %lld)\n", (long long)regs.rax, (long long)-38);
    assert(regs.rax == (uint64_t)-38);
    printf("ENOSYS passed\n");

    printf("All tests passed.\n");
    return 0;
}
