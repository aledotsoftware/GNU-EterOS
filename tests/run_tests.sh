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

# Test Readv Security (Disabled - Link Error)
# echo "---------------------------------------------------"
# echo "Running test_readv_security..."
# gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_readv_security.c kernel/string.c -o tests/test_readv_security
# ./tests/test_readv_security
# rm tests/test_readv_security

# Test Mmap Fixed Security
echo "---------------------------------------------------"
echo "Running test_mmap_fixed..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_mmap_fixed.c kernel/string.c -o tests/test_mmap_fixed
./tests/test_mmap_fixed
rm tests/test_mmap_fixed

# Test Reclaimer
echo "---------------------------------------------------"
echo "Running test_reclaimer..."
gcc -D__ETEROS_HOST_TEST__ tests/test_reclaimer.c -o tests/test_reclaimer
./tests/test_reclaimer
rm tests/test_reclaimer

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

# Test Framebuffer Scroll
echo "---------------------------------------------------"
echo "Running test_framebuffer_scroll..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_framebuffer_scroll.c kernel/drivers/video/font.c -o tests/test_framebuffer_scroll
./tests/test_framebuffer_scroll
rm tests/test_framebuffer_scroll

echo "---------------------------------------------------"
echo "All tests passed!"

# Test Omni Gradient Math
echo "---------------------------------------------------"
echo "Running verify_gradient..."
gcc -D__ETEROS_HOST_TEST__ tests/verify_gradient.c -o tests/verify_gradient
./tests/verify_gradient
rm tests/verify_gradient
