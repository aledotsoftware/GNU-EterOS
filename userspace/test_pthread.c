#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <errno.h>

#define CLONE_VM        0x00000100
#define CLONE_FS        0x00000200
#define CLONE_FILES     0x00000400
#define CLONE_SIGHAND   0x00000800
#define CLONE_THREAD    0x00010000

int thread_func(void* arg) {
    printf("Thread running! Arg: %p\n", arg);
    while (1) {
        // yield loop simulation
    }
    return 0;
}

int pthread_create_mock(void** thread, void* attr, void* (*start_routine)(void*), void* arg) {
    (void)attr;
    // Allocate stack
    void* stack = malloc(8192);
    if (!stack) return -1;
    void* stack_top = (void*)((char*)stack + 8192);

    // Provide inline assembly for syscall since we don't have a wrapper in this test environment
    long tid;
    long clone_flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD;
    __asm__ volatile (
        "mov $56, %%rax\n"
        "mov %1, %%rdi\n"
        "mov %2, %%rsi\n"
        "xor %%rdx, %%rdx\n"
        "xor %%r10, %%r10\n"
        "xor %%r8, %%r8\n"
        "syscall\n"
        "mov %%rax, %0\n"
        : "=r" (tid)
        : "r" (clone_flags), "r" (stack_top)
        : "rax", "rdi", "rsi", "rdx", "r10", "r8", "rcx", "r11", "memory"
    );

    if (tid == 0) {
        // Child
        start_routine(arg);
        __asm__ volatile (
            "mov $60, %%rax\n"
            "xor %%rdi, %%rdi\n"
            "syscall\n"
            :
            :
            : "rax", "rdi", "memory"
        );
    } else if (tid > 0) {
        if (thread) *thread = (void*)tid;
        return 0;
    }
    return -1;
}

int main() {
    printf("=== test_pthread ===\n");
    void* thread;
    if (pthread_create_mock(&thread, NULL, (void* (*)(void*))(uintptr_t)thread_func, (void*)0x1234) == 0) {
        printf("Thread created with TID %ld\n", (long)thread);
    } else {
        printf("Failed to create thread\n");
        return 1;
    }
    return 0;
}
