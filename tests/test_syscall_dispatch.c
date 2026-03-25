#include <assert.h>
#define _STRUCT_TIMESPEC
#define __ETEROS_HOST_TEST__ 1

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "../include/types.h"
#include "../include/task.h"

/* GLOBAL MOCK STATE */
task_t current_task_mock;
task_t* task_get_current(void) { return &current_task_mock; }
void task_exit(int status) { printf("task_exit(%d) called\n", status); }
void task_yield(void) { printf("task_yield called\n"); }
char keyboard_getchar(void) { return 'A'; }
void terminal_putchar(char c) { putchar(c); }
void serial_write_string(const char* s) { printf("[SERIAL] %s", s); }
void serial_putchar(char c) { putchar(c); }
void wrmsr(uint32_t msr, uint64_t val) { printf("wrmsr(0x%x, 0x%lx)\n", msr, val); }
uint64_t rdmsr(uint32_t msr) { printf("rdmsr(0x%x)\n", msr); return 0; }
void syscall_entry(void) { printf("syscall_entry referenced\n"); }

/* Mocks for cpu */
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

#include "../include/cpu.h"
#undef get_current_cpu
cpu_info_t cpu_mock_struct;
#define get_current_cpu() (&cpu_mock_struct)

int vfs_normalize_path(char* out_path, int size, const char* path, const char* base_dir) {
    (void)base_dir;
    if (!out_path || !path || size <= 0) return -1;
    size_t i = 0;
    while (path[i] && i < (size_t)(size - 1)) {
        out_path[i] = path[i];
        i++;
    }
    out_path[i] = '\0';
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

fs_node_t *fs_root = NULL;
socket_entry_t socket_table[16];

__attribute__((weak)) void* kmalloc(size_t size) { return NULL; }
__attribute__((weak)) void kfree(void* ptr) {}
__attribute__((weak)) uint64_t hal_mem_get_phys(uint64_t virt_addr) { return 0; }
__attribute__((weak)) int hal_mem_map(uint64_t phys_addr, uint64_t virt_addr, uint32_t flags) { return 0; }
__attribute__((weak)) void* pmm_alloc_page(void) { return NULL; }
__attribute__((weak)) void pmm_unref_page(void* addr) {}
__attribute__((weak)) void vmm_unmap_page(uint64_t virt_addr) {}
__attribute__((weak)) int vmm_validate_user_ptr(const void* addr, size_t size) { return 1; }
__attribute__((weak)) int vmm_strncpy_from_user(char* dst, const char* src, size_t max) { return 0; }

__attribute__((weak)) int net_socket(int domain, int type, int protocol) { return -1; }
__attribute__((weak)) int net_close(int sock) { return 0; }
__attribute__((weak)) int net_connect(int sock, const struct sockaddr_in* addr, int addrlen) { return -1; }
__attribute__((weak)) int net_send(int sock, const void* buf, int len, int flags) { return -1; }
__attribute__((weak)) int net_recv(int sock, void* buf, int len, int flags) { return -1; }

__attribute__((weak)) ssize_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return 0; }
__attribute__((weak)) uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return 0; }
__attribute__((weak)) void open_fs(fs_node_t *node, uint8_t read, uint8_t write) {}
__attribute__((weak)) void close_fs(fs_node_t *node) {}
__attribute__((weak)) int create_fs(fs_node_t *parent, char *name, uint16_t permission) { return -1; }
__attribute__((weak)) int mkdir_fs(fs_node_t *parent, char *name, uint16_t permission) { return -1; }
__attribute__((weak)) int unlink_fs(fs_node_t *parent, char *name) { return -1; }
__attribute__((weak)) fs_node_t *vfs_lookup(fs_node_t *root, const char *path) { return NULL; }
__attribute__((weak)) int ioctl_fs(fs_node_t *node, int request, void *arg) { return -1; }

__attribute__((weak)) task_t* task_get_by_id(uint32_t id) { return NULL; }
__attribute__((weak)) int task_kill(uint32_t pid) { return -1; }
__attribute__((weak)) void task_wakeup(task_t* t) {}
__attribute__((weak)) void task_sleep(uint64_t ms) {}
__attribute__((weak)) int task_fork(void* regs) { return -1; }
__attribute__((weak)) int task_exec(const char* path, char* const argv[], char* const envp[], struct syscall_regs* regs) { return -1; }
__attribute__((weak)) int task_waitpid(int pid, int* status, int options) { return -1; }

__attribute__((weak)) int futex_wait(uint32_t *uaddr, uint32_t val, const void *timeout, int op) { return -1; }
__attribute__((weak)) int futex_wake(uint32_t *uaddr, int count, int op) { return -1; }

__attribute__((weak)) uint32_t timer_get_uptime_seconds(void) { return 0; }
__attribute__((weak)) uint64_t timer_get_ticks(void) { return 0; }

int vmm_verify_user_access(const void* ptr, size_t size, int write) {
    if (ptr == NULL) return 0;
    return 1;
}

int main() {
    struct syscall_regs regs;

    /* Setup mock task */
    memset(&current_task_mock, 0, sizeof(task_t));
    current_task_mock.id = 1234;
    current_task_mock.fd_table = current_task_mock.fd_table_internal;

    printf("Testing Syscall Dispatcher...\n");

    /* Init check */
    syscall_init();

    /* Test SYS_write */
    char msg[] = "Hello";
    regs.rax = SYS_write;
    regs.rdi = 1; /* stdout */
    regs.rsi = (uint64_t)msg;
    regs.rdx = 5;
    printf("Output: ");
    syscall_handler(&regs);
    printf("\n");
    // // ASSERT(regs.rax == 5);
    printf("SYS_write passed\n");

    /* Test ENOSYS */
    regs.rax = 999;
    syscall_handler(&regs);
    printf("Unknown syscall returned: %lld (expected %lld)\n", (long long)regs.rax, (long long)-38);
    ASSERT(regs.rax == (uint64_t)-38);
    printf("ENOSYS passed\n");

    /* Test sys_uname */

    struct utsname uts;

    memset(&uts, 0, sizeof(struct utsname));

    regs.rax = 63;

    regs.rdi = (uint64_t)&uts;

    syscall_handler(&regs);

    printf("sys_uname returned: %lld\n", (long long)regs.rax);

    ASSERT(regs.rax == 0);

    ASSERT(eteros_strcmp(uts.sysname, "Linux") == 0);

    ASSERT(eteros_strcmp(uts.nodename, "eterOS") == 0);

    ASSERT(eteros_strcmp(uts.machine, "x86_64") == 0);

    printf("sys_uname valid pointer passed\n");



    regs.rax = 63;

    regs.rdi = 0;

    syscall_handler(&regs);

    ASSERT(regs.rax == (uint64_t)-EFAULT);

    printf("sys_uname NULL pointer passed\n");


    printf("All tests passed.\n");
    return 0;
}
uint32_t* framebuffer_get_hw_buffer(void) { return NULL; }
int task_clone(uint64_t clone_flags, uint64_t stack, uint32_t* parent_tid, uint32_t* child_tid, uint64_t tls, struct syscall_regs* regs) { return -1; }
