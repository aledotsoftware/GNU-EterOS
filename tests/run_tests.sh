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

# Test RTC
echo "---------------------------------------------------"
echo "Running test_rtc..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_rtc.c kernel/string.c -o tests/test_rtc
./tests/test_rtc
rm tests/test_rtc

# Test FAT32
echo "---------------------------------------------------"
echo "Running test_fat32..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_fat32.c kernel/string.c -o tests/test_fat32
./tests/test_fat32
rm tests/test_fat32

# Test Crypto
echo "---------------------------------------------------"
echo "Running test_crypto..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_crypto.c kernel/crypto/sha256.c kernel/crypto/ed25519.c kernel/string.c -o tests/test_crypto
./tests/test_crypto
rm tests/test_crypto

# Test ELF Security
echo "---------------------------------------------------"
echo "Running test_elf_security..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_elf_security.c kernel/string.c -o tests/test_elf_security
./tests/test_elf_security
rm tests/test_elf_security

# Test IP Aton
echo "---------------------------------------------------"
echo "Running test_ip_aton..."
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_ip_aton.c kernel/net/ip_utils.c -o tests/test_ip_aton
./tests/test_ip_aton
rm tests/test_ip_aton

echo "---------------------------------------------------"
echo "All tests passed!"

# Test Omni Gradient Math
echo "---------------------------------------------------"
echo "Running verify_gradient..."
gcc -D__ETEROS_HOST_TEST__ tests/verify_gradient.c -o tests/verify_gradient
./tests/verify_gradient
rm tests/verify_gradient
