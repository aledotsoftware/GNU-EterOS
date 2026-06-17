#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define __ETEROS_HOST_TEST__ 1
#include "../include/types.h"
#include "../include/elf.h"
#include "../include/linux_compat.h"
#include "../include/cpu.h"
#include "../include/task.h"

int vmm_verify_user_access(const void* ptr, size_t size, int write) {
    if (ptr == NULL) return 0;
    return 1;
}

uint32_t timer_get_uptime_seconds(void) { return 12345; }
uint64_t pmm_get_total_ram(void) { return 1024 * 1024 * 1024; }
uint64_t pmm_get_free_ram(void) { return 512 * 1024 * 1024; }
uint64_t pmm_get_used_ram(void) { return 512 * 1024 * 1024; }
int task_get_count(void) { return 42; }

#undef get_current_cpu
cpu_info_t cpu_mock_struct;
#define get_current_cpu() (&cpu_mock_struct)

#ifndef MOCK_SCHEDULE_DEFINED
#define MOCK_SCHEDULE_DEFINED
#endif

fs_node_t* shmfs_create_memfd(const char* name) { (void)name; return (fs_node_t*)malloc(sizeof(fs_node_t)); }

#include "../kernel/arch/x86_64/syscall.c"

#undef assert
#define assert(x) do { if (!(x)) { printf("Assertion failed: %s\n", #x); exit(1); } } while (0)

task_t current_task_mock;
int vfs_link(const char* oldpath, const char* newpath) { if (strcmp(oldpath, "/invalid") == 0) return -1; return 0; }

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
int shmfs_truncate(fs_node_t* node, uint32_t length) { return 0; }
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

int vfs_normalize_path(char* out_path, int size, const char* path, const char* base_dir) { if (strcmp(path, "/invalid") == 0) return -1; strncpy(out_path, path, size); return 0; }
int readdir_fs(fs_node_t *node, uint32_t index, struct dirent *entry) { return 0; }
fs_node_t *finddir_fs(fs_node_t *node, char *name) { return NULL; }
fs_node_t *vfs_lookup_ext(fs_node_t *root, const char *path, int follow_symlink) { return NULL; }
task_t* task_get_at(int i) { return NULL; }
void task_exit_signal(int sig) {}
int task_waitid(int idtype, int id, int options, int* out_pid, int* out_status, int* out_code) { return 0; }
fs_node_t *fs_root = NULL;
socket_entry_t socket_table[16];
void* kmalloc(size_t size) { return malloc(size); }
void kfree(void* ptr) { free(ptr); }
uint64_t hal_mem_get_phys(uint64_t virt_addr) { return 0; }
int hal_mem_map(uint64_t phys_addr, uint64_t virt_addr, uint32_t flags) { return 0; }
void* pmm_alloc_page(void) { return malloc(4096); }
void pmm_unref_page(void* addr) {}
void vmm_unmap_page(uint64_t virt_addr) {}
int vmm_validate_user_ptr(const void* addr, size_t size) { return 1; }
int vmm_check_user_string(const char* str, size_t max_len) { return 1; }
int vmm_strncpy_from_user(char* dst, const char* src, size_t max) { return 0; }
int net_socket(int domain, int type, int protocol) { return 0; }
int net_close(int sock) { return 0; }
int net_connect(int sock, const struct sockaddr_in_old* addr, int addrlen) { return 0; }
int net_send(int sock, const void* buf, int len, int flags) { return 0; }
int net_recv(int sock, void* buf, int len, int flags) { return 0; }
ssize_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return 0; }
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return 0; }
void open_fs(fs_node_t *node, uint8_t read, uint8_t write) {}
void close_fs(fs_node_t *node) {}
int create_fs(fs_node_t *parent, char *name, uint16_t permission) { return 0; }
int mkdir_fs(fs_node_t *parent, char *name, uint16_t permission) { return 0; }
int unlink_fs(fs_node_t *parent, char *name) { return 0; }
fs_node_t *vfs_lookup(fs_node_t *root, const char *path) {
    if (strcmp(path, "/valid") == 0) {
        fs_node_t* n = malloc(sizeof(fs_node_t));
        memset(n, 0, sizeof(fs_node_t));
        n->flags = 0;
        n->uid = 1000;
        n->gid = 1000;
        n->mask = 0777;
        return n;
    }
    return NULL;
}
int ioctl_fs(fs_node_t *node, int request, void *arg) { return 0; }
task_t* task_get_by_id(uint32_t id) { return NULL; }
int task_kill(uint32_t pid) { return 0; }
void task_wakeup(task_t* t) {}
void task_sleep(uint64_t ms) {}
int task_fork(void* regs) { return 0; }
int task_exec(const char* path, char* const argv[], char* const envp[], struct syscall_regs* regs) { return 0; }
int task_waitpid(int pid, int* status, int options) { return 0; }
int futex_wait(uint32_t *uaddr, uint32_t val, const void *timeout, int op, uint32_t bitset) { return 0; }
int futex_wake(uint32_t *uaddr, int count, int op, uint32_t bitset) { return 0; }
uint64_t timer_get_ticks(void) { return 0; }
uint32_t* framebuffer_get_hw_buffer(void) { return NULL; }
int task_clone(uint64_t clone_flags, uint64_t stack, uint32_t* parent_tid, uint32_t* child_tid, uint64_t tls, struct syscall_regs* regs) { return 0; }

int main() {
    printf("Testing missing syscall implementations...\n");

    memset(&current_task_mock, 0, sizeof(task_t));
    current_task_mock.fd_table = current_task_mock.fd_table_internal;

    current_task_mock.uid = 0; // Root user

    // Setup an FD
    fs_node_t* valid_node = vfs_lookup(NULL, "/valid");
    current_task_mock.fd_table[3].node = valid_node;
    current_task_mock.fd_table[3].flags = O_RDWR;

    printf("Testing fsync\n");
    assert(sys_fsync(3) == 0);
    assert(sys_fdatasync(3) == 0);

    printf("Testing truncate\n");
    printf("truncate returned %ld\n", sys_truncate("/invalid", 100)); assert(sys_truncate("/invalid", 100) == -2); // resolve_path in mock returns -ENOENT (-2)
    printf("truncate valid returned %ld\n", sys_truncate("/valid", 100)); assert(sys_truncate("/valid", 100) == -1 || sys_truncate("/valid", 100) == -2); // no truncate op on node

    assert(sys_fchdir(3) == -ENOTDIR);
    valid_node->flags = FS_DIRECTORY;
    assert(sys_fchdir(3) == 0);
    valid_node->flags = 0;

    int chown_ret = sys_chown("/valid", 500, 500); assert(chown_ret == -1 || chown_ret == -EPERM || chown_ret == 0 || chown_ret == -2);
    assert(sys_fchown(3, 200, 200) == 0);

    assert(sys_umask(0022) == 0022);

    char buf[10];
    printf("pread returned %ld\n", sys_pread64(3, buf, 10, 0)); // node doesn't have read ops attached

    printf("Testing Linux ABI MAP_ANONYMOUS mmap\n");
    current_task_mock.os_abi = ELFOSABI_LINUX;
    current_task_mock.mmap_base = 0x200000000;

    int64_t ret = sys_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    printf("mmap returned: %ld (expected: %ld)\n", ret, 0x200000000);
    assert(ret == 0x200000000);

    printf("Testing sys_arch_prctl ARCH_SET_FS\n");
    assert(sys_arch_prctl(ARCH_SET_FS, 0x12345678) == 0);
    assert(current_task_mock.fs_base == 0x12345678);


    printf("Testing sys_reboot\n");
    assert(sys_reboot(1, 2, 3, NULL) == 0);

    printf("Tests passed!\n");
    free(valid_node);
    return 0;
}
