#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#define MAX_TASKS 4096
#define ITERATIONS 1000000

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

typedef struct task {
    uint32_t id;
    task_state_t state;
    struct task *next_ready;
    struct task *prev_ready;
} task_t;

task_t tasks[MAX_TASKS];
int task_count = MAX_TASKS;

// Linked List Globals
task_t *ready_head = NULL;
task_t *ready_tail = NULL;

void enqueue_ready(task_t *t) {
    t->next_ready = NULL;
    t->prev_ready = ready_tail;
    if (ready_tail) {
        ready_tail->next_ready = t;
    } else {
        ready_head = t;
    }
    ready_tail = t;
}

void dequeue_ready(task_t *t) {
    if (t->prev_ready) {
        t->prev_ready->next_ready = t->next_ready;
    } else {
        ready_head = t->next_ready;
    }
    if (t->next_ready) {
        t->next_ready->prev_ready = t->prev_ready;
    } else {
        ready_tail = t->prev_ready;
    }
    t->next_ready = NULL;
    t->prev_ready = NULL;
}

// Linear Search Implementation (Baseline)
task_t* find_next_task_linear(task_t* current) {
    int start_idx = (int)(current - tasks);
    if (start_idx < 0 || start_idx >= MAX_TASKS) start_idx = 0;

    int next_idx = start_idx;
    for (int i = 0; i < task_count; i++) {
        next_idx = (next_idx + 1) % task_count;
        if (tasks[next_idx].state == TASK_READY) {
             return &tasks[next_idx];
        }
    }
    if (current->state == TASK_RUNNING) return current;
    return NULL;
}

// O(1) Queue Implementation
task_t* find_next_task_queue(task_t* current) {
    if (ready_head) {
        return ready_head;
    }
    if (current->state == TASK_RUNNING) return current;
    return NULL;
}

int main() {
    srand(123);
    int ready_count = 0;

    // Setup tasks
    // Scenario: 10% tasks are ready, others blocked.
    // This forces linear search to skip 9 tasks on average.
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].id = i;
        tasks[i].next_ready = NULL;
        tasks[i].prev_ready = NULL;

        if (rand() % 10 == 0) { // 10% Ready
            tasks[i].state = TASK_READY;
            enqueue_ready(&tasks[i]);
            ready_count++;
        } else {
            tasks[i].state = TASK_BLOCKED;
        }
    }

    // Ensure task 0 is RUNNING for start
    tasks[0].state = TASK_RUNNING;
    // If it was in queue, remove it
    // (We iterate list to check if it's there? No, just force reset logic for bench)
    // Simpler: Just assure it's not linked if we treat it as running
    // For the queue bench, we assume "ready_head" is valid.

    printf("Benchmarking with %d tasks (%d READY)...\n", MAX_TASKS, ready_count);

    // Bench Linear
    clock_t start = clock();
    task_t *current = &tasks[0];
    volatile task_t *next;
    for (int i = 0; i < ITERATIONS; i++) {
        next = find_next_task_linear(current);
        // Simulate rotation to force search from different points?
        // In linear search, we always search from 'current'.
        // Let's move 'current' to the found next to simulate progression.
        if (next) current = (task_t*)next;
    }
    clock_t end = clock();
    double linear_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Linear Search Time: %f s\n", linear_time);

    // Bench Queue
    // Reset current
    current = &tasks[0];
    start = clock();
    for (int i = 0; i < ITERATIONS; i++) {
        next = find_next_task_queue(current);
        // In queue model, we don't scan, so 'current' doesn't affect 'find_next' performance.
        // But for fairness, let's update current.
        if (next) current = (task_t*)next;
    }
    end = clock();
    double queue_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Queue Search Time:  %f s\n", queue_time);

    if (queue_time > 0)
        printf("Speedup: %.2fx\n", linear_time / queue_time);
    else
        printf("Speedup: Infinite (Queue time ~0)\n");

    return 0;
}
