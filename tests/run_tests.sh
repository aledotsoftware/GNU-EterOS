#!/bin/bash
set -e

echo "Building and running tests..."

# Test Heap
echo "---------------------------------------------------"
echo "Running test_heap..."
gcc -D__ETEROS_HOST_TEST__ tests/test_heap.c -o tests/test_heap
./tests/test_heap
rm tests/test_heap

# Test String
echo "---------------------------------------------------"
echo "Running test_string..."
gcc -D__ETEROS_HOST_TEST__ tests/test_string.c kernel/string.c -o tests/test_string
./tests/test_string
rm tests/test_string

# Test Kcalloc
echo "---------------------------------------------------"
echo "Running test_kcalloc..."
gcc -D__ETEROS_HOST_TEST__ tests/test_kcalloc.c -o tests/test_kcalloc
./tests/test_kcalloc
rm tests/test_kcalloc

# Test Malloc Overflow
echo "---------------------------------------------------"
echo "Running test_malloc_overflow..."
gcc -D__ETEROS_HOST_TEST__ tests/test_malloc_overflow.c -o tests/test_malloc_overflow
./tests/test_malloc_overflow
rm tests/test_malloc_overflow

# Test RTC
echo "---------------------------------------------------"
echo "Running test_rtc..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_rtc.c kernel/string.c -o tests/test_rtc
./tests/test_rtc
rm tests/test_rtc

# Test BCache
echo "---------------------------------------------------"
echo "Running test_bcache..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_bcache.c kernel/string.c -o tests/test_bcache
./tests/test_bcache
rm tests/test_bcache

# Test FAT32
echo "---------------------------------------------------"
echo "Running test_fat32..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_fat32.c kernel/string.c kernel/fs/bcache.c -o tests/test_fat32
./tests/test_fat32
rm tests/test_fat32

# Test JFS
echo "---------------------------------------------------"
echo "Running test_jfs..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_jfs.c kernel/string.c -o tests/test_jfs
./tests/test_jfs
rm tests/test_jfs

# Test Crypto
echo "---------------------------------------------------"
echo "Running test_crypto..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_crypto.c kernel/crypto/sha256.c kernel/crypto/ed25519.c kernel/string.c -o tests/test_crypto
./tests/test_crypto
rm tests/test_crypto

# Test ELF Security (Disabled - File Missing)
# echo "---------------------------------------------------"
# echo "Running test_elf_security..."
# gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_elf_security.c kernel/string.c -o tests/test_elf_security
# ./tests/test_elf_security
# rm tests/test_elf_security

# Test ELF Read Failure
echo "---------------------------------------------------"
echo "Running test_elf_read_failure..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_elf_read_failure.c -o tests/test_elf_read_failure
./tests/test_elf_read_failure
rm tests/test_elf_read_failure

# Test IP Aton
echo "---------------------------------------------------"
echo "Running test_ip_aton..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_ip_aton.c kernel/net/core/ip_utils.c -o tests/test_ip_aton
./tests/test_ip_aton
rm tests/test_ip_aton

# Test Initrd Overflow
echo "---------------------------------------------------"
echo "Running test_initrd_overflow..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_initrd_overflow.c -o tests/test_initrd_overflow
./tests/test_initrd_overflow
rm tests/test_initrd_overflow

# Test Readv Security
echo "---------------------------------------------------"
echo "Running test_readv_security..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_readv_security.c kernel/string.c -o tests/test_readv_security
./tests/test_readv_security
rm tests/test_readv_security

# Test Mmap Fixed Security
echo "---------------------------------------------------"
echo "Running test_mmap_fixed..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_mmap_fixed.c kernel/string.c -o tests/test_mmap_fixed
./tests/test_mmap_fixed
rm tests/test_mmap_fixed

# Test VMM Unmap Page
echo "---------------------------------------------------"
echo "Running test_vmm_unmap..."
# Need to mock the inline assembly for ASAN/Host execution
cp kernel/mm/vmm.c tests/vmm_mock.c
sed -i 's/__asm__ volatile("invlpg (%0)" : : "r" (addr) : "memory");/flush_tlb_local_called = true; flush_tlb_addr = addr;/g' tests/vmm_mock.c
sed -i 's/(void)addr;/flush_tlb_local_called = true; flush_tlb_addr = addr;/g' tests/vmm_mock.c
sed -i 's/__asm__ volatile("pause");//g' tests/vmm_mock.c
sed -i 's/__asm__ volatile("mov %0, %%cr3" : : "r" (pml4_addr) : "memory");//g' tests/vmm_mock.c
sed -i 's/__asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));//g' tests/vmm_mock.c
sed -i 's/__asm__ volatile("mov %0, %%cr3" : : "r"(current_cr3) : "memory");//g' tests/vmm_mock.c
sed -i 's/__asm__ volatile("mov %%cr3, %0" : "=r"(cr3));//g' tests/vmm_mock.c
sed -i 's/static pt_entry_t\* pml4 = (pt_entry_t\*)BOOT_PML4_ADDR;/pt_entry_t* pml4 = NULL;/g' tests/vmm_mock.c
sed -i 's|../../include/|../include/|g' tests/vmm_mock.c
gcc -g -O0 -D__ETEROS_HOST_TEST__ -Iinclude tests/test_vmm_unmap.c -o tests/test_vmm_unmap
./tests/test_vmm_unmap
rm tests/test_vmm_unmap
rm tests/vmm_mock.c

# Test Reclaimer (Disabled - File Missing)
# echo "---------------------------------------------------"
# echo "Running test_reclaimer..."
# gcc -D__ETEROS_HOST_TEST__ tests/test_reclaimer.c -o tests/test_reclaimer
# ./tests/test_reclaimer
# rm tests/test_reclaimer

# Test Stdio
echo "---------------------------------------------------"
echo "Running test_stdio..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_stdio.c kernel/stdio.c kernel/string.c -o tests/test_stdio
./tests/test_stdio
rm tests/test_stdio

# Test ProcFS
echo "---------------------------------------------------"
echo "Running test_procfs..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_procfs.c -o tests/test_procfs
./tests/test_procfs
rm tests/test_procfs

# Test DevFS
echo "---------------------------------------------------"
echo "Running test_devfs..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_devfs.c -o tests/test_devfs
./tests/test_devfs
rm tests/test_devfs

# Test Framebuffer Scroll
echo "---------------------------------------------------"
echo "Running test_framebuffer_scroll..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_framebuffer_scroll.c kernel/drivers/video/font.c -o tests/test_framebuffer_scroll
./tests/test_framebuffer_scroll
rm tests/test_framebuffer_scroll

# Test Sem Logic
echo "---------------------------------------------------"
echo "Running test_sem_logic..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_sem_logic.c -o tests/test_sem_logic
./tests/test_sem_logic
rm tests/test_sem_logic

# Test Sys Open (Security Fix)
echo "---------------------------------------------------"
echo "Running test_sys_open..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_sys_open.c kernel/string.c -o tests/test_sys_open
./tests/test_sys_open
rm tests/test_sys_open

# Test Sys Read/Write Permissions
echo "---------------------------------------------------"
echo "Running test_sys_rw_perms..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_sys_rw_perms.c kernel/string.c -o tests/test_sys_rw_perms
./tests/test_sys_rw_perms
rm tests/test_sys_rw_perms

# Test Socket Security
echo "---------------------------------------------------"
echo "Running test_socket_security..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_socket_security.c kernel/string.c -o tests/test_socket_security
./tests/test_socket_security
rm tests/test_socket_security

# Test VFS Readdir
echo "---------------------------------------------------"
echo "Running test_vfs_readdir..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_vfs_readdir.c -o tests/test_vfs_readdir
./tests/test_vfs_readdir
rm tests/test_vfs_readdir

echo "---------------------------------------------------"
# Test Syscall Dispatch

echo "---------------------------------------------------"

echo "Running test_syscall_dispatch..."

gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_syscall_dispatch.c kernel/string.c -o tests/test_syscall_dispatch

./tests/test_syscall_dispatch

rm tests/test_syscall_dispatch

echo "---------------------------------------------------"
echo "Running test_syscall_openat..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_syscall_openat.c kernel/string.c -o tests/test_syscall_openat
./tests/test_syscall_openat
rm tests/test_syscall_openat

echo "---------------------------------------------------"
echo "Running test_syscall_getdents64..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_syscall_getdents64.c kernel/string.c -o tests/test_syscall_getdents64
./tests/test_syscall_getdents64
rm tests/test_syscall_getdents64

echo "---------------------------------------------------"
echo "Running test_syscall_epoll..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_syscall_epoll.c kernel/string.c -o tests/test_syscall_epoll
./tests/test_syscall_epoll
rm tests/test_syscall_epoll

echo "---------------------------------------------------"
echo "Running test_syscall_mmap_fixed..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_syscall_mmap_fixed.c kernel/string.c -o tests/test_syscall_mmap_fixed
./tests/test_syscall_mmap_fixed
rm tests/test_syscall_mmap_fixed

echo "---------------------------------------------------"
echo "Running test_syscall_clone..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_syscall_clone.c kernel/string.c -o tests/test_syscall_clone
./tests/test_syscall_clone
rm tests/test_syscall_clone

echo "---------------------------------------------------"
echo "Running test_syscall_pipe2..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_syscall_pipe2.c kernel/string.c -o tests/test_syscall_pipe2
./tests/test_syscall_pipe2
rm tests/test_syscall_pipe2

echo "---------------------------------------------------"
echo "Running test_syscall_rt_sigaction..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_syscall_rt_sigaction.c kernel/string.c -o tests/test_syscall_rt_sigaction
./tests/test_syscall_rt_sigaction
rm tests/test_syscall_rt_sigaction


echo "All tests passed!"

# Test Omni Gradient Math
echo "---------------------------------------------------"
echo "Running verify_gradient..."
gcc -D__ETEROS_HOST_TEST__ tests/verify_gradient.c -o tests/verify_gradient
./tests/verify_gradient
rm tests/verify_gradient

# Test Libc Expansion
# echo "---------------------------------------------------"
# echo "Running test_libc_expansion..."
# gcc -D__ETEROS_HOST_TEST__ -Iuserspace/libc/include tests/test_libc_expansion.c -o tests/test_libc_expansion
# ./tests/test_libc_expansion
# rm tests/test_libc_expansion

# Test Xtensa UART
echo "---------------------------------------------------"
echo "Running test_xtensa_uart..."
gcc -D__ETEROS_HOST_TEST__ tests/test_xtensa_uart.c -o tests/test_xtensa_uart
./tests/test_xtensa_uart
rm tests/test_xtensa_uart

# Test Sem
echo "---------------------------------------------------"
echo "Running test_sem..."
gcc -D__ETEROS_HOST_TEST__ -Itests/mocks tests/test_sem.c -o tests/test_sem
./tests/test_sem
rm tests/test_sem

# Test Bench Draw Window Fastpath
echo "---------------------------------------------------"
echo "Running bench_draw_window_fastpath..."
gcc -D__ETEROS_HOST_TEST__ tests/bench_draw_window_fastpath.c -o tests/bench_draw_window_fastpath
./tests/bench_draw_window_fastpath
rm tests/bench_draw_window_fastpath

# Test Bench GFX Draw Rect Fastpath
echo "---------------------------------------------------"
echo "Running bench_gfx_draw_rect..."
gcc -D__ETEROS_HOST_TEST__ tests/bench_gfx_draw_rect.c -o tests/bench_gfx_draw_rect
./tests/bench_gfx_draw_rect
rm tests/bench_gfx_draw_rect
# Test Bench Gfx Rect Fastpath
echo "---------------------------------------------------"
echo "Running bench_gfx_rect..."
gcc -O3 -D__ETEROS_HOST_TEST__ -Iinclude tests/bench_gfx_rect.c kernel/string.c -o tests/bench_gfx_rect
./tests/bench_gfx_rect
rm tests/bench_gfx_rect
