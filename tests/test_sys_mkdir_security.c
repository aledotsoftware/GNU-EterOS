#define __ETEROS_HOST_TEST__ 1
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#define timespec eteros_timespec
#include "../include/types.h"
#include "../include/task.h"
#include "../include/fs/vfs.h"
#include "../include/net/socket.h"
#include "../include/pmm.h"
#include "../include/fcntl.h"

#define get_current_cpu real_get_current_cpu
#define get_cpu_id real_get_cpu_id
#include "../include/cpu.h"
#undef get_current_cpu
#undef get_cpu_id
#include "../include/lock.h"

#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef EACCES
#define EACCES 13
#endif
#ifndef EPERM
#define EPERM 1
#endif

task_t current_task_mock;
socket_entry_t socket_table[MAX_SOCKETS];
sem_t net_sem;
fs_node_t* fs_root = NULL;
int total_cpus = 1;
cpu_info_t cpus[MAX_CPUS];

task_t* task_get_current(void) { return &current_task_mock; }
cpu_info_t* get_current_cpu(void) { return &cpus[0]; }
int get_cpu_id(void) { return 0; }
void task_exit(int status) { exit(status); }
void task_yield(void) {}
void schedule(void) {}
void context_switch(uint64_t* old, uint64_t new) {}
void tss_set_rsp0(uint64_t rsp) {}
void serial_write_string(const char* s) {}
void* kmalloc(size_t size) { return malloc(size); }
void kfree(void* ptr) { free(ptr); }
int vmm_verify_user_access(const void* addr, size_t size, int write) { return 1; }
int vmm_validate_user_ptr(const void* addr, size_t size) { return 1; }
int vmm_check_user_string(const char* str, size_t max_len) { return 1; }
uint64_t vmm_virt_to_phys(uint64_t virt) { return virt; }
int vmm_map_page(uint64_t phys, uint64_t virt, uint64_t flags) { return 0; }
void vmm_destroy_pml4(uint64_t pml4) {}
uint64_t vmm_clone_pml4(int cow) { return 0; }
ssize_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return size; }
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return size; }
void close_fs(fs_node_t *node) {}

int mkdir_called = 0;
int mock_mkdir(fs_node_t* parent, char* name, uint16_t permission) {
    mkdir_called = 1;
    return 0;
}

fs_node_t* vfs_lookup(fs_node_t* root, const char* path) {
    if (strcmp(path, "/etc") == 0) {
        fs_node_t* node = (fs_node_t*)malloc(sizeof(fs_node_t));
        memset(node, 0, sizeof(fs_node_t));
        node->flags = FS_DIRECTORY;
        node->inode = 123;
        node->ref_count = 1;
        node->uid = 0; // Root owned
        node->gid = 0;
        node->mask = 0755; // rwxr-xr-x
        node->mkdir = mock_mkdir;
        return node;
    }
    return NULL;
}

int create_fs(fs_node_t *parent, char *name, uint16_t permission) { return 0; }
void open_fs(fs_node_t *node, uint8_t read, uint8_t write) {}
int unlink_fs(fs_node_t *parent, char *name) { return 0; }
int ioctl_fs(fs_node_t *node, int request, void *arg) { return 0; }
uint64_t rdmsr(uint32_t msr) { return 0; }
void wrmsr(uint32_t msr, uint64_t val) {}
uint32_t timer_get_uptime_seconds(void) { return 0; }
uint64_t timer_get_ticks(void) { return 0; }
void task_sleep(uint64_t ms) {}
int net_recv(int sock, void* buf, int len, int flags) { return 0; }
int net_send(int sock, const void* buf, int len, int flags) { return 0; }
int net_close(int sock) { return 0; }
int net_socket(int domain, int type, int protocol) { return -1; }
int net_connect(int sock, const struct sockaddr_in* addr, int addrlen) { return -1; }
int futex_wait(uint32_t *uaddr, uint32_t val, const void *timeout) { return 0; }
int futex_wake(uint32_t *uaddr, int val) { return 0; }
void* pmm_alloc_page(void) { return malloc(4096); }
void pmm_free_page(void* p) { free(p); }
void pmm_unref_page(void* p) {}
uint64_t hal_mem_get_phys(uint64_t virt) { return virt; }
int hal_mem_map(uint64_t phys, uint64_t virt, uint32_t flags) { return 0; }
void lapic_send_ipi(int id, int vector) {}
int vmm_strncpy_from_user(char* dst, const char* src, size_t max) {
    strncpy(dst, src, max); dst[max - 1] = '\0'; return strlen(dst);
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
int eteros_snprintf(char* str, size_t size, const char* format, ...) { return 0; }

int mkdir_fs(fs_node_t *parent, char *name, uint16_t permission) {
    if (parent->mkdir) return parent->mkdir(parent, name, permission);
    return -1;
}

#include "../kernel/arch/x86_64/syscall.c"

int main() {
    printf("Running sys_mkdir security test...\n");
    memset(&current_task_mock, 0, sizeof(task_t));
    current_task_mock.id = 1;
    current_task_mock.uid = 1000; // Normal user
    current_task_mock.gid = 1000;

    int64_t ret = sys_mkdir("/etc/hacked", 0755);

    if (ret == -EACCES || ret == -EPERM) {
        printf("PASSED: sys_mkdir denied write access to /etc for non-root user.\n");
        return 0;
    } else if (mkdir_called) {
        printf("VULNERABILITY CONFIRMED: sys_mkdir allowed non-root to create directory in /etc!\n");
        return 1;
    } else {
        printf("FAILED: sys_mkdir returned unexpected error %ld\n", ret);
        return 1;
    }
}
