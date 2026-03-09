#include <stdlib.h>
#include <assert.h>
#ifndef assert
#define assert(x) do { if (!(x)) { printf("ASSERT FAILED: %s\n", #x); exit(1); } } while(0)
#endif
#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <setjmp.h>

/* Defines and Mocks from include/vmm.h */
#define PAGE_SIZE       4096
#define PAGE_PRESENT    0x1
#define PAGE_WRITE      0x2
#define PAGE_USER       0x4
#define PAGE_ACCESSED   0x20
#define PAGE_SWAPPED    0x400
#define PAGE_ADDR_MASK  0x000FFFFFFFFFF000

/* Defines and Mocks from include/task.h */
#define TASK_DEAD 0
#define TASK_RUNNING 1
#define TASK_SLEEPING 3

typedef struct {
    uint64_t rsp;
    uint8_t* stack_base;
    uint64_t kernel_stack;
    uint64_t cr3;
    uint32_t id;
    uint32_t parent_id;
    int state;
    /* Other fields omitted as not used by reclaimer.c */
} task_t;

/* Global state for mocks */
static jmp_buf loop_break;
static int loop_break_enabled = 0;
static int sleep_calls = 0;
static uint64_t total_ram = 1000;
static uint64_t free_ram = 1000;
static int unref_calls = 0;
static uint64_t last_unref_phys = 0;

/* Mock task system */
#define MAX_TASKS 10
static task_t mock_tasks[MAX_TASKS];
static int task_count = MAX_TASKS;

void reset_mocks() {
    memset(mock_tasks, 0, sizeof(mock_tasks));
    task_count = MAX_TASKS;
    unref_calls = 0;
    last_unref_phys = 0;
    loop_break_enabled = 0;
    sleep_calls = 0;
    total_ram = 1000;
    free_ram = 1000;
}

/* Mock Functions */
void serial_write_string(const char* str) {
    /* printf("[SERIAL] %s", str); */
    (void)str;
}

void task_sleep(int ms) {
    (void)ms;
    sleep_calls++;
    if (loop_break_enabled && sleep_calls > 1) {
        longjmp(loop_break, 1);
    }
}

int task_get_max() {
    return task_count;
}

task_t* task_get_at(int i) {
    if (i < 0 || i >= task_count) return NULL;
    return &mock_tasks[i];
}

static void (*reclaimer_thread_entry)(void) = NULL;
void task_create(const char* name, void (*func)(void)) {
    (void)name;
    reclaimer_thread_entry = func;
}

uint64_t pmm_get_total_ram() {
    return total_ram;
}

uint64_t pmm_get_free_ram() {
    return free_ram;
}

void pmm_unref_page(void* phys) {
    unref_calls++;
    last_unref_phys = (uint64_t)phys;
}

/* Include reclaimer.c, skipping headers */
#define ETEROS_MM_H
#define ETEROS_VMM_H
#define ETEROS_PMM_H
#define ETEROS_TASK_H
#define ETEROS_SERIAL_H

#include "../kernel/mm/reclaimer.c"

/* Tests */

void test_reclaim_recursive_eviction() {
    printf("Running test_reclaim_recursive_eviction... ");
    reset_mocks();

    /* Setup page tables */
    /* We need arrays aligned to 4096 */
    uint64_t* pml4 = (uint64_t*)aligned_alloc(4096, 4096);
    uint64_t* pdpt = (uint64_t*)aligned_alloc(4096, 4096);
    uint64_t* pd   = (uint64_t*)aligned_alloc(4096, 4096);
    uint64_t* pt   = (uint64_t*)aligned_alloc(4096, 4096);

    memset(pml4, 0, 4096);
    memset(pdpt, 0, 4096);
    memset(pd, 0, 4096);
    memset(pt, 0, 4096);

    /* Link tables */
    /* Check pointers fit in mask */
    assert(((uint64_t)pdpt & ~PAGE_ADDR_MASK) == 0);
    assert(((uint64_t)pd & ~PAGE_ADDR_MASK) == 0);
    assert(((uint64_t)pt & ~PAGE_ADDR_MASK) == 0);

    /* Construct hierarchy: PML4[0] -> PDPT[8] -> PD[0] -> PT[0] -> Page */
    /* Note: reclaimer skips kernel mappings at level 4 (i!=0) and level 3 (i<8) */

    pml4[0] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
    pdpt[8] = (uint64_t)pd   | PAGE_PRESENT | PAGE_USER | PAGE_WRITE; /* Index 8 passes the skip check */
    pd[0]   = (uint64_t)pt   | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;

    /* Setup pages in PT */
    /* Page 0: Accessed recently (Second chance) */
    uint64_t page0_phys = 0x100000;
    pt[0] = page0_phys | PAGE_PRESENT | PAGE_USER | PAGE_ACCESSED;

    /* Page 1: Not accessed recently (Eviction candidate) */
    uint64_t page1_phys = 0x200000;
    pt[1] = page1_phys | PAGE_PRESENT | PAGE_USER; /* No ACCESSED bit */

    /* Page 2: Already swapped (Should be ignored) */
    uint64_t page2_phys = 0x300000;
    pt[2] = page2_phys | PAGE_SWAPPED; /* No PRESENT bit */

    /* Page 3: Kernel page (Should be skipped by PAGE_USER check) */
    uint64_t page3_phys = 0x400000;
    pt[3] = page3_phys | PAGE_PRESENT; /* No PAGE_USER bit */

    /* Run reclaimer on PML4 */
    reclaim_pt_recursive(pml4, 4);

    /* Verify Page 0: ACCESSED bit cleared, still PRESENT */
    assert((pt[0] & PAGE_ACCESSED) == 0);
    assert((pt[0] & PAGE_PRESENT) == PAGE_PRESENT);
    assert((pt[0] & PAGE_SWAPPED) == 0);

    /* Verify Page 1: Evicted (PRESENT cleared, SWAPPED set) */
    assert((pt[1] & PAGE_PRESENT) == 0);
    assert((pt[1] & PAGE_SWAPPED) == PAGE_SWAPPED);
    assert(unref_calls == 1);
    assert(last_unref_phys == page1_phys);

    /* Verify Page 2: Unchanged */
    assert((pt[2] & PAGE_PRESENT) == 0);
    assert((pt[2] & PAGE_SWAPPED) == PAGE_SWAPPED);
    /* unref_calls should still be 1 */
    assert(unref_calls == 1);

    /* Verify Page 3: Unchanged */
    assert((pt[3] & PAGE_PRESENT) == PAGE_PRESENT);
    assert((pt[3] & PAGE_SWAPPED) == 0);

    /* Cleanup */
    free(pml4);
    free(pdpt);
    free(pd);
    free(pt);

    printf("PASSED\n");
}

void test_reclaimer_thread_low_memory() {
    printf("Running test_reclaimer_thread_low_memory... ");
    reset_mocks();

    /* Setup low memory condition: free < total / 5 */
    total_ram = 1000;
    free_ram = 100; /* 10% free */

    /* Setup a task with a CR3 */
    uint64_t* pml4 = (uint64_t*)aligned_alloc(4096, 4096);
    memset(pml4, 0, 4096);

    mock_tasks[1].id = 100;
    mock_tasks[1].state = TASK_RUNNING;
    mock_tasks[1].cr3 = (uint64_t)pml4;

    /* Setup a page to reclaim */
    uint64_t* pdpt = (uint64_t*)aligned_alloc(4096, 4096);
    uint64_t* pd   = (uint64_t*)aligned_alloc(4096, 4096);
    uint64_t* pt   = (uint64_t*)aligned_alloc(4096, 4096);
    memset(pdpt, 0, 4096);
    memset(pd, 0, 4096);
    memset(pt, 0, 4096);

    pml4[0] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
    pdpt[8] = (uint64_t)pd   | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
    pd[0]   = (uint64_t)pt   | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
    pt[0]   = 0x123000 | PAGE_PRESENT | PAGE_USER; /* Candidate for eviction */

    /* Enable loop break */
    loop_break_enabled = 1;

    if (setjmp(loop_break) == 0) {
        page_reclaimer_thread();
    }

    /* Verify eviction happened */
    assert(unref_calls == 1);
    assert((pt[0] & PAGE_SWAPPED) == PAGE_SWAPPED);

    free(pml4);
    free(pdpt);
    free(pd);
    free(pt);

    printf("PASSED\n");
}

void test_reclaimer_thread_high_memory() {
    printf("Running test_reclaimer_thread_high_memory... ");
    reset_mocks();

    /* Setup high memory condition: free > total / 5 */
    total_ram = 1000;
    free_ram = 500; /* 50% free */

    /* Setup a task with a CR3 */
    uint64_t* pml4 = (uint64_t*)aligned_alloc(4096, 4096);
    memset(pml4, 0, 4096);

    mock_tasks[1].id = 100;
    mock_tasks[1].state = TASK_RUNNING;
    mock_tasks[1].cr3 = (uint64_t)pml4;

    /* Setup a page to reclaim (but shouldn't run) */
    uint64_t* pdpt = (uint64_t*)aligned_alloc(4096, 4096);
    uint64_t* pd   = (uint64_t*)aligned_alloc(4096, 4096);
    uint64_t* pt   = (uint64_t*)aligned_alloc(4096, 4096);
    memset(pdpt, 0, 4096);
    memset(pd, 0, 4096);
    memset(pt, 0, 4096);

    pml4[0] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
    pdpt[8] = (uint64_t)pd   | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
    pd[0]   = (uint64_t)pt   | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
    pt[0]   = 0x123000 | PAGE_PRESENT | PAGE_USER;

    /* Enable loop break */
    loop_break_enabled = 1;

    if (setjmp(loop_break) == 0) {
        page_reclaimer_thread();
    }

    /* Verify eviction did NOT happen */
    assert(unref_calls == 0);
    assert((pt[0] & PAGE_PRESENT) == PAGE_PRESENT);

    free(pml4);
    free(pdpt);
    free(pd);
    free(pt);

    printf("PASSED\n");
}

void test_reclaimer_init() {
    printf("Running test_reclaimer_init... ");
    reset_mocks();

    reclaimer_init();

    assert(reclaimer_thread_entry == page_reclaimer_thread);

    printf("PASSED\n");
}

int main() {
    test_reclaim_recursive_eviction();
    test_reclaimer_thread_low_memory();
    test_reclaimer_thread_high_memory();
    test_reclaimer_init();

    printf("\nAll reclaimer tests passed!\n");
    return 0;
}
