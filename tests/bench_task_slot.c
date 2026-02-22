#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define MAX_TASKS 64

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

typedef struct {
    int id;
    task_state_t state;
} task_t;

static task_t tasks[MAX_TASKS];
static int task_count = 0;
static uint64_t task_bitmap = 0;

// Reset state
void reset_state() {
    memset(tasks, 0, sizeof(tasks));
    task_count = 0;
    task_bitmap = 0;
    // Simulate init
    tasks[0].state = TASK_RUNNING;
    task_count = 1;
    task_bitmap |= 1;
}

int find_free_slot_linear() {
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD || (i >= task_count && tasks[i].state == 0)) {
            slot = i;
            break;
        }
    }
    if (slot >= 0) {
        // Simulate allocation side-effect (updating task_count)
        if (slot >= task_count) task_count = slot + 1;
        tasks[slot].state = TASK_READY;
    }
    return slot;
}

int find_free_slot_bitmap() {
    if (~task_bitmap == 0) return -1;

    int slot = __builtin_ctzll(~task_bitmap);
    if (slot >= MAX_TASKS) return -1;

    // Simulate allocation side-effect
    task_bitmap |= (1ULL << slot);
    if (slot >= task_count) task_count = slot + 1;
    tasks[slot].state = TASK_READY;

    return slot;
}

void benchmark(const char* name, int (*func)(void)) {
    reset_state();

    // Fill up to 60 tasks
    for (int i=1; i<60; i++) {
        func();
    }

    // Now benchmark finding the last few slots and handling "full"
    // But to see real diff, we need many iterations.
    // Let's do a pattern: Free one, Alloc one.

    clock_t start = clock();
    long iterations = 10000000;

    for (long i = 0; i < iterations; i++) {
        // Free a random slot between 1 and 59
        int victim = (i % 59) + 1;
        tasks[victim].state = TASK_DEAD;
        task_bitmap &= ~(1ULL << victim); // Sync bitmap for fairness

        // Alloc
        func();
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("%s: %f seconds\n", name, time_taken);
}

int main() {
    printf("Benchmarking Task Slot Allocation (MAX_TASKS=%d)\n", MAX_TASKS);
    benchmark("Linear Search", find_free_slot_linear);
    benchmark("Bitmap Search", find_free_slot_bitmap);
    return 0;
}
