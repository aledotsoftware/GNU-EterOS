#define __ETEROS_HOST_TEST__ 1

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

/* Defines to handle conflicts */
#define timespec eteros_timespec

/* Include kernel headers */
#include "../include/types.h"
#include "../include/task.h"
#include "../include/fs/vfs.h"
#include "../include/net/socket.h"
#include "../include/pmm.h"
#include "../include/hal.h"

/* Hack to mock inline functions in cpu.h */
#define get_current_cpu real_get_current_cpu
#define get_cpu_id real_get_cpu_id
#include "../include/cpu.h"
#undef get_current_cpu
#undef get_cpu_id
#include "../include/lock.h"

/* Define missing error codes */
#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef EFAULT
#define EFAULT 14
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif
#ifndef ENODEV
#define ENODEV 19
#endif

/* Globals */
task_t current_task_mock;
bool hal_mem_map_called = false;
uint64_t hal_mem_map_last_virt = 0;
bool pmm_unref_page_called = false;
uint64_t pmm_unref_page_last_addr = 0;

/* Mocks */
task_t* task_get_current(void) { return &current_task_mock; }
cpu_info_t* get_current_cpu(void) { return NULL; }
int get_cpu_id(void) { return 0; }

void task_exit(int status) { exit(status); }
void task_yield(void) {}
void schedule(void) {}
void task_sleep(uint64_t ms) {}
task_t* task_get_by_id(uint32_t id) { return NULL; }
int task_kill(uint32_t pid) { return 0; }
int task_exec(const char* path, char* const argv[], char* const envp[], struct syscall_regs* regs) { return 0; }
int task_fork(void* regs) { return 0; }
int task_waitpid(int pid, int* status, int options) { return 0; }
void task_wakeup(task_t* t) {}

void serial_write_string(const char* s) {}
void* kmalloc(size_t size) { return malloc(size); }
void kfree(void* ptr) { free(ptr); }

/* Mock VMM */
int vmm_validate_user_ptr(const void* addr, size_t size) { return 1; }
int vmm_verify_user_access(const void* addr, size_t size, int write) { return 1; }
int vmm_check_user_string(const char* str, size_t max_len) { return 1; }
uint64_t vmm_virt_to_phys(uint64_t virt) { return virt; } /* Identity for test */

/* Mock PMM */
void* pmm_alloc_page(void) {
    void* p = malloc(4096);
    return p;
}
void pmm_free_page(void* p) { free(p); }
void pmm_unref_page(void* p) {
    pmm_unref_page_called = true;
    pmm_unref_page_last_addr = (uint64_t)p;
    /* In real pmm, this might free if ref count drops to 0. Here we assume we own it. */
    // free(p); // Dangerous if we mock hal_mem_get_phys returning static addr
}

/* Mock HAL */
bool page_mapped = true;

/* We simulate that address 0x1000 IS mapped to physical 0x5000 initially */
uint64_t hal_mem_get_phys(uint64_t virt) {
    if (virt == 0x1000 && page_mapped) return 0x5000;
    return 0;
}

int hal_mem_map(uint64_t phys, uint64_t virt, uint32_t flags) {
    hal_mem_map_called = true;
    hal_mem_map_last_virt = virt;
    page_mapped = true;
    return 0;
}

int hal_mem_unmap(uint64_t virt) {
    if (virt == 0x1000) page_mapped = false;
    return 0;
}

/* Mock VFS */
ssize_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return 0; }
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return 0; }
void close_fs(fs_node_t *node) {}
fs_node_t* vfs_lookup(fs_node_t* root, const char* path) { return NULL; }
int create_fs(fs_node_t *parent, char *name, uint16_t permission) { return 0; }
void open_fs(fs_node_t *node, uint8_t read, uint8_t write) {}
int mkdir_fs(fs_node_t *parent, char *name, uint16_t permission) { return 0; }
int unlink_fs(fs_node_t *parent, char *name) { return 0; }
int ioctl_fs(fs_node_t *node, int request, void *arg) { return 0; }

/* Stub Networking */
int net_recv(int sock, void* buf, int len, int flags) { return 0; }
int net_send(int sock, const void* buf, int len, int flags) { return 0; }
int net_close(int sock) { return 0; }
int net_socket(int domain, int type, int protocol) { return -1; }
int net_connect(int sock, const struct sockaddr_in* addr, int addrlen) { return -1; }

socket_entry_t socket_table[MAX_SOCKETS];
sem_t net_sem;
fs_node_t *fs_root = NULL;

/* Stub MSR */
uint64_t rdmsr(uint32_t msr) { return 0; }
void wrmsr(uint32_t msr, uint64_t val) {}

/* Stub Timer */
uint32_t timer_get_uptime_seconds(void) { return 0; }
uint64_t timer_get_ticks(void) { return 0; }

/* Stub Futex */
int futex_wait(uint32_t *uaddr, uint32_t val, const void *timeout) { return 0; }
int futex_wake(uint32_t *uaddr, int val) { return 0; }

/* Stub other */
void syscall_entry(void) {}
int eteros_snprintf(char* str, size_t size, const char* format, ...) { return 0; }

/* String mocks */
#undef strlen
#undef strncpy
#undef memcpy
#undef memset

size_t eteros_strlen(const char* str) { return strlen(str); }
size_t eteros_strlcpy(char* dest, const char* src, size_t size) {
    strncpy(dest, src, size);
    if (size > 0) dest[size-1] = 0;
    return strlen(src);
}
void* eteros_memcpy(void* dest, const void* src, size_t n) { return memcpy(dest, src, n); }
void* eteros_memset(void* dest, int c, size_t n) {
    if ((uint64_t)dest == 0x1000) return dest; /* Skip write to mock address */
    return memset(dest, c, n);
}
void utoa_hex_s(uint64_t value, char* buffer, size_t buffer_size) { sprintf(buffer, "%lx", value); }

/* Re-define macros for syscall.c */
#define strlen eteros_strlen
#define strncpy eteros_strncpy
#define memcpy eteros_memcpy
#define memset eteros_memset
#define strlcpy eteros_strlcpy

/* Include source under test */
#include "../kernel/arch/x86_64/syscall.c"

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Running test_mmap_security_mock...\n");

    /* Setup Task */
    memset(&current_task_mock, 0, sizeof(task_t));
    current_task_mock.mmap_base = 0x100000;

    /* Test Case: MAP_FIXED over existing mapping */
    /* 0x1000 is simulated as mapped by hal_mem_get_phys */
    void* addr = (void*)0x1000;
    size_t len = 4096;
    int prot = 3; /* RW */
    int flags = 0x22; /* MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS (0x20) */
    /* Wait, MAP_FIXED is 0x10. MAP_ANONYMOUS is usually 0x20. */
    /* In syscall.c: check flags & 0x20 for anon? */
    /* Let's check syscall.c: "if (!(flags & 0x20)) return -ENODEV;" */
    /* So bit 0x20 IS required (MAP_ANONYMOUS). */
    /* MAP_FIXED is bit 0x10. */

    flags = 0x32; /* MAP_FIXED (0x10) | MAP_ANONYMOUS (0x20) | MAP_PRIVATE (0x02) */

    hal_mem_map_called = false;

    int64_t res = sys_mmap(addr, len, prot, flags, -1, 0);

    if (res != 0x1000) {
        printf("sys_mmap failed: returned %lx\n", res);
        return 1;
    }

    if (hal_mem_map_called) {
        printf("PASSED: hal_mem_map was called. Existing mapping was replaced.\n");
    } else {
        printf("FAILED: hal_mem_map was NOT called. Existing mapping was PRESERVED (Security Risk).\n");
        return 1;
    }

    return 0;
}
