#define __ETEROS_HOST_TEST__ 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <sys/mman.h>

/* Mock types */
#include "../include/types.h"
#include "../include/fs/vfs.h"
#include "../include/mm.h"
#include "../include/hal.h"
#include "../include/task.h"
#include "../include/elf.h"
#include "../include/stdio.h"

/* Global Mock State */
task_t current_task_mock;
uint8_t* fake_elf_file_data = NULL;
size_t fake_elf_file_size = 0;
fs_node_t* fs_root = NULL;

/* Mocks */
task_t* task_get_current(void) {
    return NULL;
}

void kfree(void* ptr) {
}

void* kmalloc(size_t size) {
    return malloc(size);
}

void serial_write_string(const char* s) {
    /* Muted for clean output, or enable for debug */
    /* fprintf(stderr, "[SERIAL] %s", s); */
}

void serial_putchar(char c) {
}

/* Mock Memory Management */
void* pmm_alloc_page(void) {
    static uint64_t next_page = 0x1000;
    next_page += 0x1000;
    return (void*)next_page;
}

uint64_t hal_mem_get_phys(uint64_t vaddr) {
    return 0;
}

int hal_mem_map(uint64_t phys, uint64_t virt, uint32_t flags) {
    return 0;
}

/* Mock VFS */
fs_node_t* vfs_lookup(fs_node_t *root, const char *path) {
    fs_node_t* node = (fs_node_t*)malloc(sizeof(fs_node_t));
    __builtin_memset(node, 0, sizeof(fs_node_t));
    return node;
}

uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (offset >= fake_elf_file_size) return 0;
    if (offset + size > fake_elf_file_size) size = fake_elf_file_size - offset;

    __builtin_memcpy(buffer, fake_elf_file_data + offset, size);
    return size;
}

/* Mock String Functions */
void* eteros_memset(void* s, int c, size_t n) {
    return __builtin_memset(s, c, n);
}

void* eteros_memcpy(void* dest, const void* src, size_t n) {
    return __builtin_memcpy(dest, src, n);
}

size_t eteros_strlen(const char* s) {
    return __builtin_strlen(s);
}

size_t eteros_strlcpy(char* dest, const char* src, size_t size) {
    size_t len = __builtin_strlen(src);
    if (size > 0) {
        size_t n = (len >= size) ? size - 1 : len;
        __builtin_memcpy(dest, src, n);
        dest[n] = '\0';
    }
    return len;
}

char* eteros_strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for ( ; i < n; i++)
        dest[i] = '\0';
    return dest;
}

int eteros_snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    #undef vsnprintf
    int ret = vsnprintf(str, size, format, args);
    #define vsnprintf eteros_vsnprintf
    va_end(args);
    return ret;
}

int eteros_vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    #undef vsnprintf
    int ret = vsnprintf(str, size, format, ap);
    #define vsnprintf eteros_vsnprintf
    return ret;
}

void utoa_hex_s(uint64_t value, char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "0x%llx", (unsigned long long)value);
}

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#ifndef PAGE_ALIGN_DOWN
#define PAGE_ALIGN_DOWN(x) ((x) & ~(PAGE_SIZE - 1))
#endif
#ifndef PAGE_ALIGN_UP
#define PAGE_ALIGN_UP(x) (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#endif

#ifndef EI_CLASS
#define EI_CLASS 4
#endif
#ifndef ELFCLASS64
#define ELFCLASS64 2
#endif
#ifndef EI_DATA
#define EI_DATA 5
#endif
#ifndef ELFDATA2LSB
#define ELFDATA2LSB 1
#endif
#ifndef EI_VERSION
#define EI_VERSION 6
#endif
#ifndef EV_CURRENT
#define EV_CURRENT 1
#endif
#ifndef ELFOSABI_SYSV
#define ELFOSABI_SYSV 0
#endif
#ifndef SHN_UNDEF
#define SHN_UNDEF 0
#endif

#include "../kernel/fs/elf.c"

void test_elf_truncation() {
    printf("Running test_elf_truncation...\n");

    /* Map memory at 0x100000000 (4GB) so we pass identity map check */
    void* mem = mmap((void*)0x100000000ULL, 0x4000, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    fake_elf_file_size = 4096 * 2;
    fake_elf_file_data = (uint8_t*)malloc(fake_elf_file_size);
    if (!fake_elf_file_data) { fprintf(stderr, "malloc failed\n"); exit(1); }

    __builtin_memset(fake_elf_file_data, 0, fake_elf_file_size);

    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)fake_elf_file_data;
    ehdr->e_ident[0] = 0x7F;
    ehdr->e_ident[1] = 'E';
    ehdr->e_ident[2] = 'L';
    ehdr->e_ident[3] = 'F';
    ehdr->e_ident[EI_CLASS] = ELFCLASS64;
    ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr->e_ident[EI_VERSION] = EV_CURRENT;
    ehdr->e_ident[EI_OSABI] = ELFOSABI_SYSV;
    ehdr->e_type = ET_EXEC;
    ehdr->e_machine = EM_X86_64;
    ehdr->e_version = EV_CURRENT;
    ehdr->e_entry = 0x100000000ULL;
    ehdr->e_phoff = sizeof(Elf64_Ehdr);
    ehdr->e_shoff = 0;
    ehdr->e_flags = 0;
    ehdr->e_ehsize = sizeof(Elf64_Ehdr);
    ehdr->e_phentsize = sizeof(Elf64_Phdr);
    ehdr->e_phnum = 1;
    ehdr->e_shentsize = 0;
    ehdr->e_shnum = 0;
    ehdr->e_shstrndx = SHN_UNDEF;

    Elf64_Phdr* phdr = (Elf64_Phdr*)(fake_elf_file_data + ehdr->e_phoff);
    phdr->p_type = PT_LOAD;
    phdr->p_flags = PF_R | PF_X;

    phdr->p_offset = 0x100001000ULL;
    phdr->p_vaddr = 0x100000000ULL;
    phdr->p_paddr = 0x100000000ULL;
    phdr->p_filesz = 0x100;
    phdr->p_memsz = 0x100;
    phdr->p_align = 0x1000;

    // Put NOPs
    __builtin_memset(fake_elf_file_data + 0x1000, 0x90, 0x100);

    uint64_t entry = elf_load_file("/bin/test", 0);

    if (entry != 0) {
        printf("VULNERABILITY CONFIRMED: elf_load_file succeeded with truncated offset!\n");
        printf("Entry point returned: 0x%llx\n", (unsigned long long)entry);
        exit(1);
    } else {
        printf("PASSED: elf_load_file correctly rejected the truncated offset.\n");
    }

    free(fake_elf_file_data);
}

int main() {
    test_elf_truncation();
    return 0;
}
