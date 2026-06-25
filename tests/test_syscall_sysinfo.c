#include <stdio.h>
#include <string.h>

#define __ETEROS_HOST_TEST__ 1
#include "../include/types.h"
#include "../include/sys/sysinfo.h"
#include "../include/cpu.h"
#include "../include/task.h"

int vmm_verify_user_access(const void* ptr, size_t size, int write) {
    if (ptr == NULL) return 0;
    return 1;
}

uint32_t timer_get_uptime_seconds(void) { return 12345; }
uint64_t pmm_get_total_ram(void) { return 1024 * 1024 * 1024; } // 1GB
uint64_t pmm_get_free_ram(void) { return 512 * 1024 * 1024; } // 512MB
uint64_t pmm_get_used_ram(void) { return 512 * 1024 * 1024; } // 512MB
int task_get_count(void) { return 42; }

/* Mock the syscall function directly since we're just testing the implementation */
#undef get_current_cpu
cpu_info_t cpu_mock_struct;
#define get_current_cpu() (&cpu_mock_struct)

#ifndef MOCK_SCHEDULE_DEFINED
#define MOCK_SCHEDULE_DEFINED
/* schedule mocked */
#endif
#include <stdlib.h>
fs_node_t* shmfs_create_memfd(const char* name) { (void)name; return (fs_node_t*)malloc(sizeof(fs_node_t)); }


#include "../kernel/arch/x86_64/syscall.c"

#undef assert
#define assert(x) do { if (!(x)) { printf("Assertion failed: %s\n", #x); return 1; } } while (0)

/* Mock minimal kernel dependencies for syscall.c to compile in the test */
task_t current_task_mock;
int vfs_link(const char* oldpath, const char* newpath) { return -1; }

task_t* task_get_current(void) { return &current_task_mock; }
void task_exit(int status) {}
void task_yield(void) {}
void schedule(void) {}
char keyboard_getchar(void) { return 'A'; }
void terminal_putchar(char c) {}
void serial_write_string(const char* s) {}
void serial_putchar(char c) {}
void wrmsr(uint32_t msr, uint64_t val) {}
uint64_t rdmsr(uint32_t msr) { return 0; }
void syscall_entry(void) {}
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
int vfs_normalize_path(char* out_path, int size, const char* path, const char* base_dir) { return -1; }
int readdir_fs(fs_node_t *node, uint32_t index, struct dirent *entry) { return -1; }
fs_node_t *finddir_fs(fs_node_t *node, char *name) { return NULL; }
fs_node_t *vfs_lookup_ext(fs_node_t *root, const char *path, int follow_symlink) { return NULL; }
task_t* task_get_at(int i) { return NULL; }
void task_exit_signal(int sig) {}
int task_waitid(int idtype, int id, int options, int* out_pid, int* out_status, int* out_code) { return -1; }
fs_node_t *fs_root = NULL;
socket_entry_t socket_table[16];
void* kmalloc(size_t size) { return NULL; }
void kfree(void* ptr) {}
uint64_t hal_mem_get_phys(uint64_t virt_addr) { return 0; }
int hal_mem_map(uint64_t phys_addr, uint64_t virt_addr, uint32_t flags) { return 0; }
void* pmm_alloc_page(void) { return NULL; }
void pmm_unref_page(void* addr) {}
void vmm_unmap_page(uint64_t virt_addr) {}
int vmm_validate_user_ptr(const void* addr, size_t size) { return 1; }
int vmm_check_user_string(const char* str, size_t max_len) { return 1; }
int vmm_strncpy_from_user(char* dst, const char* src, size_t max) { return 0; }
int net_socket(int domain, int type, int protocol) { return -1; }
int net_close(int sock) { return 0; }
int net_connect(int sock, const struct sockaddr_in_old* addr, int addrlen) { return -1; }
int net_send(int sock, const void* buf, int len, int flags) { return -1; }
int net_recv(int sock, void* buf, int len, int flags) { return -1; }
ssize_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return 0; }
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return 0; }
void open_fs(fs_node_t *node, uint8_t read, uint8_t write) {}
void close_fs(fs_node_t *node) {}
int create_fs(fs_node_t *parent, char *name, uint16_t permission) { return -1; }
int mkdir_fs(fs_node_t *parent, char *name, uint16_t permission) { return -1; }
int unlink_fs(fs_node_t *parent, char *name) { return -1; }
fs_node_t *vfs_lookup(fs_node_t *root, const char *path) { return NULL; }
int ioctl_fs(fs_node_t *node, int request, void *arg) { return -1; }
task_t* task_get_by_id(uint32_t id) { return NULL; }
int task_kill(uint32_t pid) { return -1; }
void task_wakeup(task_t* t) {}
void task_sleep(uint64_t ms) {}
int task_fork(void* regs) { return -1; }
int task_exec(const char* path, char* const argv[], char* const envp[], struct syscall_regs* regs) { return -1; }
int task_waitpid(int pid, int* status, int options) { return -1; }
int futex_wait(uint32_t *uaddr, uint32_t val, const void *timeout, int op, uint32_t bitset) { return -1; }
int futex_wake(uint32_t *uaddr, int count, int op, uint32_t bitset) { return -1; }
uint64_t timer_get_ticks(void) { return 0; }
uint32_t* framebuffer_get_hw_buffer(void) { return NULL; }
int task_clone(uint64_t clone_flags, uint64_t stack, uint32_t* parent_tid, uint32_t* child_tid, uint64_t tls, struct syscall_regs* regs) { return -1; }

int main() {
    struct sysinfo info;
    memset(&info, 0xFF, sizeof(struct sysinfo)); // Fill with garbage to ensure memset

    int64_t ret = sys_sysinfo(&info);

    assert(ret == 0);
    assert(info.uptime == 12345);
    assert(info.totalram == 1024 * 1024 * 1024);
    assert(info.freeram == 512 * 1024 * 1024);
    assert(info.procs == 42);
    assert(info.mem_unit == 1);

    printf("sys_sysinfo test passed!\n");
    return 0;
}
void task_stop_signal(int sig) {}
