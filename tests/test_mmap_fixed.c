#define __ETEROS_HOST_TEST__ 1

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

/* Defines to handle conflicts with syscall.c */
#define timespec eteros_timespec

/* Stub eteros_ string funcs to use libc ones */
#define strlen eteros_strlen
#define memset eteros_memset
#define memcpy eteros_memcpy
#define strncpy eteros_strncpy
#define strlcpy eteros_strlcpy
#define strlcat eteros_strlcat


/* Include kernel headers */
#include "../include/types.h"
#include "../include/task.h"
#include "../include/fs/vfs.h"
#include "../include/net/socket.h"
#include "../include/pmm.h"

/* Hack to mock inline functions in cpu.h */
void pmm_ref_page(void* p) { (void)p; }
int shmfs_truncate(fs_node_t* node, uint32_t length) { (void)node; (void)length; return -1; }
#define get_current_cpu real_get_current_cpu
#define get_cpu_id real_get_cpu_id
#include "../include/cpu.h"
#undef get_current_cpu
#undef get_cpu_id

#include "../include/lock.h"

/* Define missing error codes if needed */
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
bool vmm_unmap_called = false;
uint64_t vmm_unmap_addr = 0;
bool pmm_unref_called = false;
uint64_t pmm_unref_addr = 0;

/* Globals for linking */
socket_entry_t socket_table[MAX_SOCKETS];
sem_t net_sem;
fs_node_t* fs_root = NULL;
int total_cpus = 1;
cpu_info_t cpus[MAX_CPUS];

/* Mocks */
task_t* task_get_current(void) {
    return &current_task_mock;
}

cpu_info_t* get_current_cpu(void) { return &cpus[0]; }
int get_cpu_id(void) { return 0; }

void task_exit(int status) {
    printf("task_exit called with status %d\n", status);
    exit(status);
}

void task_yield(void) {}
void schedule(void) {}
void context_switch(uint64_t* old, uint64_t* new, void* fpu1, void* fpu2) {}
void tss_set_rsp0(uint64_t rsp) {}

void serial_write_string(const char* s) {
    // printf("[SERIAL] %s", s);
}

void* kmalloc(size_t size) { return malloc(size); }
void kfree(void* ptr) { free(ptr); }

/* Mock VMM */
int vmm_verify_user_access(const void* addr, size_t size, int write) { return 1; }
int vmm_validate_user_ptr(const void* addr, size_t size) { return 1; }
int vmm_check_user_string(const char* str, size_t max_len) { return 1; }

int vmm_strncpy_from_user(char *dst, const char *src, size_t count) {
    // Mock implementation: just copy
    if (count == 0) return 0;
    strncpy(dst, src, count);
    dst[count - 1] = '\0';
    return strlen(dst);
}

uint64_t vmm_virt_to_phys(uint64_t virt) {
    // Mock mapping for 0x10000000 -> 0x999000
    if (virt >= 0x10000000 && virt < 0x10001000) {
        return 0x999000;
    }
    return 0;
}

int vmm_map_page(uint64_t phys, uint64_t virt, uint64_t flags) { return 0; }

void vmm_unmap_page(uint64_t virt) {
    vmm_unmap_called = true;
    vmm_unmap_addr = virt;
}

void vmm_destroy_pml4(uint64_t pml4) {}
uint64_t vmm_clone_pml4(int cow) { return 0; }

/* Mock PMM */
void* pmm_alloc_page(void) { return malloc(4096); }
void pmm_free_page(void* p) { free(p); }
void pmm_unref_page(void* p) {
    pmm_unref_called = true;
    pmm_unref_addr = (uint64_t)p;
}
uint16_t pmm_get_ref_count(void* addr) { return 1; }

/* Stub Hal */
uint64_t hal_mem_get_phys(uint64_t virt) {
    return vmm_virt_to_phys(virt);
}
int hal_mem_map(uint64_t phys, uint64_t virt, uint32_t flags) { return 0; }

/* Stub VFS */
ssize_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return size; }
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return size; }
void close_fs(fs_node_t *node) {}
fs_node_t* vfs_lookup(fs_node_t* root, const char* path) { return NULL; }
int create_fs(fs_node_t *parent, char *name, uint16_t permission) { return 0; }
void open_fs(fs_node_t *node, uint8_t read, uint8_t write) {}
int mkdir_fs(fs_node_t *parent, char *name, uint16_t permission) { return 0; }
int unlink_fs(fs_node_t *parent, char *name) { return 0; }
int ioctl_fs(fs_node_t *node, int request, void *arg) { return 0; }

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
int futex_wait(uint32_t *uaddr, uint32_t val, const void *timeout, int op) { return 0; }
int futex_wake(uint32_t *uaddr, int val, int op) { return 0; }

/* Stub APIC */
void lapic_send_ipi(int id, int vector) {}

/* Stub task functions */
int task_fork(void* regs) { return 0; }

/* Stub framebuffer functions needed by window.c which might be linked or included */
uint32_t* framebuffer_get_buffer(void) { return NULL; }
uint32_t framebuffer_get_width(void) { return 0; }
uint32_t framebuffer_get_height(void) { return 0; }
uint32_t framebuffer_get_pitch(void) { return 0; }
uint32_t framebuffer_get_bpp(void) { return 0; }
void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color) {}
void framebuffer_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {}
void gfx_add_dirty_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {}
void gfx_present(void) {}
int task_exec(const char* path, char* const argv[], char* const envp[], struct syscall_regs* regs) { return 0; }
int task_waitpid(int pid, int* status, int options) { return 0; }
task_t* task_get_by_id(uint32_t id) { return NULL; }
int task_kill(uint32_t pid) { return 0; }
void task_wakeup(task_t* t) {}

/* Stub syscall entry */
void syscall_entry(void) {}

/* Stub eteros_snprintf (called by syscall.c via macro) */
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

task_t* task_get_at(int index) { (void)index; return NULL; }
int task_get_count(void) { return 0; }
void task_exit_signal(int sig) { (void)sig; }
int task_waitid(int idtype, int id, int options, int* out_pid, int* out_status, int* out_code) { (void)idtype; (void)id; (void)options; (void)out_pid; (void)out_status; (void)out_code; return -1; }
#include "../kernel/arch/x86_64/syscall.c"

#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_PRIVATE 0x02
#define PROT_READ 1
#define PROT_WRITE 2

int main() {
    printf("Running test_mmap_fixed...\n");

    // Setup task
    memset(&current_task_mock, 0, sizeof(task_t));
    current_task_mock.mmap_base = 0x700000000000ULL;

    /* TEST: mmap MAP_FIXED over existing mapping */
    // We mocked address 0x10000000 to be mapped to physical 0x999000

    vmm_unmap_called = false;
    pmm_unref_called = false;

    // Call sys_mmap with MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS
    // Addr = 0x10000000, Len = 4096
    int64_t res = sys_mmap((void*)0x10000000, 4096, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

    if (res != 0x10000000) {
        printf("FAILED: sys_mmap failed (res=%lx)\n", res);
        return 1;
    }

    if (!vmm_unmap_called && !pmm_unref_called) {
        printf("Vulnerability reproduced: Existing page was NOT unmapped.\n");
        return 1; // Failed: Vulnerability still exists
    }

    if (vmm_unmap_called && pmm_unref_called) {
        printf("Fix Verified: Existing page WAS unmapped.\n");
        return 0; // Success verification
    }

    printf("Unexpected state: vmm_unmap=%d, pmm_unref=%d\n", vmm_unmap_called, pmm_unref_called);
    return 1;
}
int task_clone(uint64_t clone_flags, uint64_t stack, uint32_t* parent_tid, uint32_t* child_tid, uint64_t tls, struct syscall_regs* regs) { return -1; }
uint32_t* framebuffer_get_hw_buffer(void) { return NULL; }

#ifndef PMM_MOCKS_INJECTED
#define PMM_MOCKS_INJECTED
__attribute__((weak)) uint64_t pmm_get_total_ram(void) { return 1024 * 1024 * 1024; }
__attribute__((weak)) uint64_t pmm_get_free_ram(void) { return 512 * 1024 * 1024; }
__attribute__((weak)) uint64_t pmm_get_used_ram(void) { return 512 * 1024 * 1024; }
#endif
