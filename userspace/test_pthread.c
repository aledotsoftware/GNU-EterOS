#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void* thread_func(void* arg) {
    printf("Thread running! Arg: %p\n", arg);
    // Return some value
    return (void*)0x5678;
}

int main() {
    printf("=== test_pthread ===\n");
    pthread_t thread;
    if (pthread_create(&thread, NULL, thread_func, (void*)0x1234) == 0) {
        printf("Thread created with TID %ld\n", (long)thread);
        void *ret;
        pthread_join(thread, &ret);
        printf("Thread joined, ret: %p\n", ret);
    } else {
        printf("Failed to create thread\n");
        return 1;
    }
    return 0;
}
