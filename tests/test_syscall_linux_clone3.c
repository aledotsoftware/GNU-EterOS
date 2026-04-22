#define __ETEROS_HOST_TEST__ 1

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

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
#ifndef EFAULT
#define EFAULT 14
#endif

task_t current_task_mock;
socket_entry_t socket_table[MAX_SOCKETS];
sem_t net_sem;
fs_node_t* fs_root = NULL;
task_t* task_get_at(int i) { (void)i; return NULL; }
int task_get_count(void) { return 0; }
void task_exit_signal(int sig) { (void)sig; }
int task_waitid(int idtype, int id, int options, int* out_pid, int* out_status, int* out_code) { (void)idtype; (void)id; (void)options; (void)out_pid; (void)out_status; (void)out_code; return -1; }
void schedule(void) {}
int total_cpus = 1;
cpu_info_t cpus[MAX_CPUS];

task_t* task_get_current(void) { return &current_task_mock; }
cpu_info_t* get_current_cpu(void) { return &cpus[0]; }
int get_cpu_id(void) { return 0; }

int sys_clone(uint64_t clone_flags, uint64_t newsp, uint64_t parent_tid, uint64_t child_tid, uint64_t tls_val, struct syscall_regs *regs) {
    return task_fork(regs);
}

int task_clone(uint64_t clone_flags, uint64_t stack, uint32_t* parent_tid, uint32_t* child_tid, uint64_t tls, struct syscall_regs* regs) {
    return 100; /* Mock PID return */
}

void task_exit(int status) { exit(status); }
void task_yield(void) {}
/* schedule mocked */
void context_switch(uint64_t* old, uint64_t* new, void* fpu1, void* fpu2) {}
void tss_set_rsp0(uint64_t rsp) {}
void serial_write_string(const char* s) {}
void* kmalloc(size_t size) { return malloc(size); }
void kfree(void* ptr) { free(ptr); }
int vmm_verify_user_access(const void* addr, size_t size, int write) { return 1; }
int vmm_validate_user_ptr(const void* addr, size_t size) { return 1; }
int vmm_check_user_string(const char* str, size_t max_len) { return 1; }
uint64_t vmm_virt_to_phys(uint64_t virt) { return virt; }
void vmm_destroy_pml4(uint64_t pml4) {}
uint64_t vmm_clone_pml4(int cow) { return 0; }
ssize_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return size; }
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return size; }
void close_fs(fs_node_t *node) {}
fs_node_t* vfs_lookup(fs_node_t* root, const char* path) { return NULL; }
int create_fs(fs_node_t *parent, char *name, uint16_t permission) { return 0; }
void open_fs(fs_node_t *node, uint8_t read, uint8_t write) {}
int mkdir_fs(fs_node_t *parent, char *name, uint16_t permission) { return 0; }
int unlink_fs(fs_node_t *parent, char *name) { return 0; }
int ioctl_fs(fs_node_t *node, int request, void *arg) { return 0; }
uint64_t rdmsr(uint32_t msr) { return 0; }
void wrmsr(uint32_t msr, uint64_t val) {}
uint32_t timer_get_uptime_seconds(void) { return 0; }
uint64_t timer_get_ticks(void) { return 0; }
void task_sleep(uint64_t ms) {}
int net_recv(socket_t sock, void* buf, int len, int flags) { return 0; }
int net_send(socket_t sock, const void* buf, int len, int flags) { return 0; }
int net_close(socket_t sock) { return 0; }
socket_t net_socket(int domain, int type, int protocol) { return -1; }
int net_connect(socket_t sock, const struct sockaddr_in* addr, int addrlen) { return -1; }
int futex_wait(uint32_t *uaddr, uint32_t val, const void *timeout, int op) { return 0; }
int futex_wake(uint32_t *uaddr, int val, int op) { return 0; }
void* pmm_alloc_page(void) { return malloc(4096); }
void pmm_free_page(void* p) { free(p); }
uint64_t hal_mem_get_phys(uint64_t virt) { return virt; }
int hal_mem_map(uint64_t phys, uint64_t virt, uint32_t flags) { return 0; }
int shmfs_truncate(fs_node_t* node, uint32_t length) { return -1; }
void pmm_ref_page(void* p) {}
uint32_t* framebuffer_get_buffer(void) { return NULL; }
uint32_t framebuffer_get_width(void) { return 0; }
uint32_t framebuffer_get_height(void) { return 0; }
uint32_t framebuffer_get_pitch(void) { return 0; }
uint32_t framebuffer_get_bpp(void) { return 0; }
void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color) {}
void framebuffer_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {}
void gfx_add_dirty_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {}
void gfx_present(void) {}
void lapic_send_ipi(int id, int vector) {}
int vmm_strncpy_from_user(char* dst, const char* src, size_t max) { strncpy(dst, src, max); dst[max - 1] = '\0'; return strlen(dst); }
void pmm_unref_page(void* p) {}
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

int vfs_normalize_path(char* out_path, int size, const char* path, const char* base_dir) {
    if (!out_path || !path || size <= 0) return -1;
    strlcpy(out_path, path, size);
    return 0;
}
int readdir_fs(fs_node_t *node, uint32_t index, struct dirent *entry) { return -1; }
fs_node_t *finddir_fs(fs_node_t *node, char *name) { return NULL; }
fs_node_t *vfs_lookup_ext(fs_node_t *root, const char *path, int follow_symlink) { return NULL; }
uint32_t* framebuffer_get_hw_buffer(void) { return NULL; }

#include "../kernel/arch/x86_64/syscall.c"

int main() {
    printf("Running test_syscall_linux_clone3...\n");

    memset(&current_task_mock, 0, sizeof(task_t));
    current_task_mock.id = 1;

    struct syscall_regs regs;
    memset(&regs, 0, sizeof(regs));

    struct clone_args c_args;
    memset(&c_args, 0, sizeof(c_args));
    c_args.flags = 0x123;
    c_args.stack = 0x4000;

    regs.rax = SYS_clone3;
    regs.rdi = (uint64_t)&c_args;
    regs.rsi = sizeof(c_args);

    syscall_linux_handler(&regs);

    if (regs.rax == 100) {
        printf("PASSED: sys_clone3 correctly called task_clone\n");
    } else {
        printf("FAILED: sys_clone3 returned %ld\n", regs.rax);
        return 1;
    }

    return 0;
}
void* eteros_memset(void* s, int c, size_t n) { char* p = s; while(n--) *p++ = c; return s; }
void* eteros_memcpy(void* dest, const void* src, size_t n) { char* d = dest; const char* s = src; while(n--) *d++ = *s++; return dest; }
size_t eteros_strlen(const char* s) { return strlen(s); }
size_t eteros_strlcpy(char* dst, const char* src, size_t size) {
    size_t srclen = strlen(src);
    if (size > 0) {
        size_t len = (srclen < size - 1) ? srclen : size - 1;
        memcpy(dst, src, len);
        dst[len] = '\0';
    }
    return srclen;
}
size_t eteros_strlcat(char* dst, const char* src, size_t size) { return 0; }
int eteros_strcmp(const char* s1, const char* s2) { return strcmp(s1, s2); }
int vmm_map_page(uint64_t phys_addr, uint64_t virt_addr, uint64_t flags) { return 0; }
char* eteros_strncpy(char* dest, const char* src, size_t n) { return strncpy(dest, src, n); }
