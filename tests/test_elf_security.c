#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>

/* Mock Defines to skip real headers */
#define __ETEROS_HOST_TEST__ 1
#define ELF_H
#define FS_VFS_H
#define ETEROS_HAL_H
#define ETEROS_PMM_H
#define ETEROS_TASK_H
#define ETEROS_SERIAL_H

/* Mock Macros usually in headers */
#define PAGE_SIZE 4096
#define HAL_MEM_READ 1
#define HAL_MEM_WRITE 2
#define HAL_MEM_USER 4
#define HAL_MEM_EXEC 8

#define PAGE_ALIGN_DOWN(x) ((x) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(x)   (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/* Mock Types */
typedef uint64_t uint64_t;
typedef uint32_t uint32_t;
typedef uint16_t uint16_t;
typedef uint8_t  uint8_t;
typedef int32_t  int32_t;
typedef int64_t  int64_t;
typedef size_t   size_t;

/* Types from task.h */
typedef struct {
    char name[32];
    uint64_t os_abi;
    uint64_t brk;
} task_t;

/* Types from fs/vfs.h */
typedef struct fs_node {
    char name[128];
    uint32_t flags;
    uint32_t length;
    uint32_t impl;
} fs_node_t;

fs_node_t* fs_root = NULL;

/* ELF Structures (from elf.h) */
typedef uint64_t Elf64_Addr;
typedef uint16_t Elf64_Half;
typedef uint64_t Elf64_Off;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Xword;

#define EI_NIDENT 16
#define ET_EXEC 2
#define ET_DYN 3
#define EM_X86_64 62
#define PT_LOAD 1
#define PF_R 0x4
#define PF_W 0x2
#define PF_X 0x1
#define EI_OSABI 7
#define ELFOSABI_LINUX 3

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half    e_type;
    Elf64_Half    e_machine;
    Elf64_Word    e_version;
    Elf64_Addr    e_entry;
    Elf64_Off     e_phoff;
    Elf64_Off     e_shoff;
    Elf64_Word    e_flags;
    Elf64_Half    e_ehsize;
    Elf64_Half    e_phentsize;
    Elf64_Half    e_phnum;
    Elf64_Half    e_shentsize;
    Elf64_Half    e_shnum;
    Elf64_Half    e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    Elf64_Word    p_type;
    Elf64_Word    p_flags;
    Elf64_Off     p_offset;
    Elf64_Addr    p_vaddr;
    Elf64_Addr    p_paddr;
    Elf64_Xword   p_filesz;
    Elf64_Xword   p_memsz;
    Elf64_Xword   p_align;
} Elf64_Phdr;

/* Globals for testing */
static task_t mock_task;
static uint32_t last_read_size = 0;
static Elf64_Ehdr elf_header;
static Elf64_Phdr prog_header;

/* Mock Functions */

void serial_write_string(const char* str) {
    printf("[SERIAL] %s", str);
}

task_t* task_get_current(void) {
    return &mock_task;
}

void* pmm_alloc_page(void) {
    return (void*)0xDEADBEEF;
}

uint64_t hal_mem_get_phys(uint64_t virt) {
    (void)virt;
    return 0;
}

void hal_mem_map(uint64_t phys, uint64_t virt, uint32_t flags) {
    (void)phys; (void)virt; (void)flags;
}

fs_node_t* vfs_lookup(fs_node_t* root, const char* path) {
    (void)root;
    if (strcmp(path, "/malicious_elf") == 0) {
        static fs_node_t node;
        memset(&node, 0, sizeof(node));
        return &node;
    }
    return NULL;
}

uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node;
    printf("[MOCK] read_fs: offset=%u, size=%u\n", offset, size);

    if (offset == 0 && size == sizeof(Elf64_Ehdr)) {
        memcpy(buffer, &elf_header, sizeof(Elf64_Ehdr));
        return size;
    }

    if (offset == sizeof(Elf64_Ehdr) && size == sizeof(Elf64_Phdr)) {
        memcpy(buffer, &prog_header, sizeof(Elf64_Phdr));
        return size;
    }

    /* Program Segment */
    if (offset == 4096) {
        last_read_size = size;
        return size;
    }

    return 0;
}

/* Now include the code under test */
#include "../kernel/fs/elf.c"

int main() {
    printf("Setting up ELF Security Test...\n");

    /* Map memory at 4GB */
    void* mapping = mmap((void*)0x100000000ULL, 4096 * 4, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

    if (mapping == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    /* Setup Malicious ELF Header */
    memset(&elf_header, 0, sizeof(Elf64_Ehdr));
    elf_header.e_ident[0] = 0x7F;
    elf_header.e_ident[1] = 'E';
    elf_header.e_ident[2] = 'L';
    elf_header.e_ident[3] = 'F';
    elf_header.e_type = ET_EXEC;
    elf_header.e_machine = EM_X86_64;
    elf_header.e_entry = 0x100000000;
    elf_header.e_phoff = sizeof(Elf64_Ehdr);
    elf_header.e_phentsize = sizeof(Elf64_Phdr);
    elf_header.e_phnum = 1;

    /* Setup Malicious Program Header (p_filesz > p_memsz) */
    memset(&prog_header, 0, sizeof(Elf64_Phdr));
    prog_header.p_type = PT_LOAD;
    prog_header.p_flags = PF_R | PF_W | PF_X;
    prog_header.p_offset = 4096;
    prog_header.p_vaddr = 0x100000000;
    prog_header.p_paddr = 0;
    prog_header.p_memsz = 4096;   /* Allocate 1 page */
    prog_header.p_filesz = 8192;  /* Read 2 pages (OVERFLOW) */
    prog_header.p_align = 4096;

    /* Run ELF Loader */
    printf("Loading malicious ELF...\n");
    elf_load_file("/malicious_elf", 0);

    printf("Last read size: %u\n", last_read_size);

    if (last_read_size == 8192) {
        printf("[TEST] VULNERABILITY DETECTED\n");
        return 1; /* Failed / Vulnerable */
    }

    return 0; /* Passed / Safe */
}
