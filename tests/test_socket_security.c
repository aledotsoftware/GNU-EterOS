#define __ETEROS_HOST_TEST__ 1

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

/* Defines to handle conflicts with syscall.c */
#define timespec eteros_timespec
#define termios eteros_termios

/* Include kernel headers */
#include "../include/types.h"
#include "../include/task.h"
#include "../include/fs/vfs.h"
#include "../include/net/socket.h"
#include "../include/pmm.h"

/* Hack to mock inline functions in cpu.h */
#define get_current_cpu real_get_current_cpu
#define get_cpu_id real_get_cpu_id
#include "../include/cpu.h"
#undef get_current_cpu
#undef get_cpu_id
#include "../include/lock.h"
#include "../include/fcntl.h"

/* Define missing error codes if needed */
#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef EFAULT
#define EFAULT 14
#endif
#ifndef ENOSYS
#define ENOSYS 38
#endif
#ifndef EBADF
#define EBADF 9
#endif

/* Globals */
task_t current_task_mock;
bool vmm_verify_user_access_called = false;
bool vmm_should_fail = false;
int kmalloc_count = 0;
int kfree_count = 0;

/* Globals for linking */
socket_entry_t socket_table[MAX_SOCKETS];
sem_t net_sem;
fs_node_t* fs_root = NULL;
int total_cpus = 1;
cpu_info_t cpus[MAX_CPUS];

/* Mocks */
task_t* task_get_current(void) { return &current_task_mock; }
cpu_info_t* get_current_cpu(void) { return &cpus[0]; }
int get_cpu_id(void) { return 0; }
void task_exit(int status) { exit(status); }
void task_yield(void) {}
void schedule(void) {}
void context_switch(uint64_t* old, uint64_t new) {}
void tss_set_rsp0(uint64_t rsp) {}
void serial_write_string(const char* s) {}

void* kmalloc(size_t size) { kmalloc_count++; return malloc(size); }
void kfree(void* ptr) { if (ptr) kfree_count++; free(ptr); }

/* Mock vmm_verify_user_access */
int vmm_verify_user_access(const void* addr, size_t size, int write) {
    if (vmm_should_fail) return 0;

    /* Reject kernel pointers (simulate) */
    uint64_t start = (uint64_t)addr;
    if (start >= 0x800000000000ULL) return 0;

    return 1;
}

int vmm_validate_user_ptr(const void* addr, size_t size) { return vmm_verify_user_access(addr, size, 0); }
int vmm_check_user_string(const char* str, size_t max_len) { return vmm_verify_user_access(str, 1, 0); }
uint64_t vmm_virt_to_phys(uint64_t virt) { return virt; }
int vmm_map_page(uint64_t phys, uint64_t virt, uint64_t flags) { return 0; }
void vmm_destroy_pml4(uint64_t pml4) {}
uint64_t vmm_clone_pml4(int cow) { return 0; }

/* Mock VFS functions */
ssize_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return size; }
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return size; }
void close_fs(fs_node_t *node) {}
fs_node_t* vfs_lookup(fs_node_t* root, const char* path) { return NULL; }
int create_fs(fs_node_t *parent, char *name, uint16_t permission) { return 0; }
void open_fs(fs_node_t *node, uint8_t read, uint8_t write) {}
int mkdir_fs(fs_node_t *parent, char *name, uint16_t permission) { return 0; }
int unlink_fs(fs_node_t *parent, char *name) { return 0; }
int ioctl_fs(fs_node_t *node, int request, void *arg) { return 0; }
int readdir_fs(fs_node_t *node, uint32_t index, struct dirent *entry) { return -1; }

/* Stub MSR functions */
uint64_t rdmsr(uint32_t msr) { return 0; }
void wrmsr(uint32_t msr, uint64_t val) {}

/* Stub Timer */
uint32_t timer_get_uptime_seconds(void) { return 0; }
uint64_t timer_get_ticks(void) { return 0; }
void task_sleep(uint64_t ms) {}

/* Stub Network */
int net_recv(socket_t sock, void* buf, int len, int flags) { return 0; }
int net_send(socket_t sock, const void* buf, int len, int flags) { return 0; }
int net_close(socket_t sock) { return 0; }
socket_t net_socket(int domain, int type, int protocol) { return -1; }
int net_connect(socket_t sock, const struct sockaddr_in* addr, int addrlen) { return -1; }

/* Stub Futex */
int futex_wait(uint32_t *uaddr, uint32_t val, const void *timeout) { return 0; }
int futex_wake(uint32_t *uaddr, int val) { return 0; }

/* Stub PMM */
void* pmm_alloc_page(void) { return malloc(4096); }
void pmm_free_page(void* p) { free(p); }
void pmm_unref_page(void* p) {}

/* Stub Hal */
uint64_t hal_mem_get_phys(uint64_t virt) { return virt; }
int hal_mem_map(uint64_t phys, uint64_t virt, uint32_t flags) { return 0; }

/* Stub APIC */
void lapic_send_ipi(int id, int vector) {}

/* Stub vmm_strncpy_from_user */
int vmm_strncpy_from_user(char* dst, const char* src, size_t max) {
    if (!dst || !src || max == 0) return -22;
    if (vmm_should_fail) return -14;
    strncpy(dst, src, max);
    dst[max - 1] = '\0';
    return strlen(dst);
}
void vmm_unmap_page(uint64_t virt) {}
void task_wakeup(task_t* task) {}
int task_fork(void* regs) { return 0; }
int task_exec(const char* path, char* const argv[], char* const envp[], struct syscall_regs* regs) { return 0; }
int task_waitpid(int pid, int* status, int options) { return 0; }
task_t* task_get_by_id(uint32_t id) { return NULL; }
int task_kill(uint32_t pid) { return 0; }
void syscall_entry(void) {}

#include <stdarg.h>
#undef snprintf
#undef vsnprintf
int eteros_snprintf(char* str, size_t size, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

/* Include source */
int vfs_normalize_path(char* out_path, int size, const char* path, const char* base_dir) {
    if (!out_path || !path || size <= 0) return -1;
    strlcpy(out_path, path, size);
    return 0;
}

int readdir_fs(fs_node_t *node, uint32_t index, struct dirent *entry) {
    (void)node; (void)index; (void)entry;
    return -1;
}

fs_node_t *finddir_fs(fs_node_t *node, char *name) {
    (void)node; (void)name;
    return NULL;
}

fs_node_t *vfs_lookup_ext(fs_node_t *root, const char *path, int follow_symlink) {
    (void)root; (void)path; (void)follow_symlink;
    return NULL;
}

#include "../kernel/arch/x86_64/syscall.c"

int main() {
    printf("Running test_socket_security...\n");

    // Setup task
    memset(&current_task_mock, 0, sizeof(task_t));
    current_task_mock.id = 1;

    // Fake kernel address
    struct sockaddr* bad_addr = (struct sockaddr*)0xFFFFFFFF80000000ULL;
    int* bad_addrlen = (int*)0xFFFFFFFF80000000ULL;

    // Normal address
    struct sockaddr good_addr;
    int good_addrlen = sizeof(good_addr);
    void* good_buf = malloc(100);

    printf("Test 1: sys_accept with bad addr\n");
    int64_t ret = sys_accept(0, bad_addr, &good_addrlen);
    if (ret != -EFAULT) {
        printf("FAILED: sys_accept did not return -EFAULT for bad addr\n");
        return 1;
    }

    printf("Test 2: sys_accept with bad addrlen\n");
    ret = sys_accept(0, &good_addr, bad_addrlen);
    if (ret != -EFAULT) {
        printf("FAILED: sys_accept did not return -EFAULT for bad addrlen\n");
        return 1;
    }

    printf("Test 3: sys_recvfrom with bad src_addr\n");
    ret = sys_recvfrom(0, good_buf, 100, 0, bad_addr, &good_addrlen);
    if (ret != -EFAULT) {
        printf("FAILED: sys_recvfrom did not return -EFAULT for bad src_addr\n");
        return 1;
    }

    printf("Test 4: sys_recvfrom with bad addrlen\n");
    ret = sys_recvfrom(0, good_buf, 100, 0, &good_addr, bad_addrlen);
    if (ret != -EFAULT) {
        printf("FAILED: sys_recvfrom did not return -EFAULT for bad addrlen\n");
        return 1;
    }

    printf("Test 5: sys_sendto with bad dest_addr\n");
    ret = sys_sendto(0, good_buf, 100, 0, bad_addr, sizeof(struct sockaddr));
    if (ret != -EFAULT) {
        printf("FAILED: sys_sendto did not return -EFAULT for bad dest_addr\n");
        return 1;
    }

    free(good_buf);

    printf("All socket security tests passed!\n");
    return 0;
}
