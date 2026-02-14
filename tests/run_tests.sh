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

echo "---------------------------------------------------"
echo "All tests passed!"
