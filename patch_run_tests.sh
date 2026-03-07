sed -i '/echo "Running test_syscall_mmap_fixed..."/i \
echo "---------------------------------------------------"\n\
echo "Running test_syscall_mmap_linux..."\n\
gcc -D__ETEROS_HOST_TEST__ -Iinclude tests/test_syscall_mmap_linux.c kernel/string.c tests/vmm_map_page_mock.c -o tests/test_syscall_mmap_linux\n\
./tests/test_syscall_mmap_linux\n\
rm tests/test_syscall_mmap_linux\n\
' tests/run_tests.sh
