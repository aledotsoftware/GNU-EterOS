#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>

/* Undefine macros from project headers so we can use standard libc functions */
#undef memcpy
#undef memset
#undef strcmp
#undef snprintf
#undef vsnprintf

/* Declare standard functions since project headers didn't include them */
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
int strcmp(const char *s1, const char *s2);
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

/* Mock Defines to skip real headers in elf.c */
#define __ETEROS_HOST_TEST__ 1
#define ELF_H
#define FS_VFS_H
#define ETEROS_HAL_H
#define ETEROS_PMM_H
#define ETEROS_TASK_H
#define ETEROS_SERIAL_H
/* ETEROS_STRING_H and ETEROS_STDIO_H are already defined by includes above */

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
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;
typedef uint64_t Elf64_Addr;
typedef struct {
    Elf64_Sxword d_tag;
    union {
        Elf64_Xword d_val;
        Elf64_Addr d_ptr;
    } d_un;
} Elf64_Dyn;

#define DT_NULL    0
#define DT_NEEDED  1
#define DT_STRTAB  5

typedef struct {
    char name[32];
    uint64_t os_abi;
    uint8_t is_linux;
    uint64_t brk;
    uint32_t euid;
    uint32_t egid;
    uint64_t mmap_base;
} task_t;
/* Types from fs/vfs.h */
typedef struct fs_node {
    char name[128];
    uint32_t flags;
    uint32_t length;
    uint32_t impl;
    uint32_t uid;
    uint32_t gid;
    uint32_t mask;
} fs_node_t;

fs_node_t* fs_root = NULL;

/* ELF Structures (from elf.h) */
typedef uint64_t Elf64_Addr;
typedef uint16_t Elf64_Half;
typedef uint64_t Elf64_Off;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;
typedef uint64_t Elf64_Addr;

#define EI_NIDENT 16
#define ET_EXEC 2
#define ET_DYN 3
#define EM_X86_64 62
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_PHDR 6
#define PF_R 0x4
#define PF_W 0x2
#define PF_X 0x1
#define EI_OSABI 7
#define ELFOSABI_NONE 0
#define ELFOSABI_LINUX 3

typedef struct {
    uint64_t entry;
    uint64_t phdr;
    uint64_t phent;
    uint64_t phnum;
    uint64_t base;
} elf_auxv_info_t;

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
static Elf64_Ehdr elf_header;
static Elf64_Phdr prog_header;

/* Mock Functions */

void serial_write_string(const char* str) {
    printf("[SERIAL] %s", str);
}

size_t eteros_strlen(const char* s) { size_t l=0; while(*s++) l++; return l; }
size_t eteros_strlcpy(char *dst, const char *src, size_t dsize) {
    const char *osrc = src;
    size_t nleft = dsize;
    if (nleft != 0) {
        while (--nleft != 0) {
            if ((*dst++ = *src++) == '\0') break;
        }
    }
    if (nleft == 0) {
        if (dsize != 0) *dst = '\0';
        while (*src++) ;
    }
    return (src - osrc - 1);
}

size_t eteros_strlcat(char *dst, const char *src, size_t dsize) {
    const char *odst = dst;
    const char *osrc = src;
    size_t n = dsize;
    size_t dlen;
    while (n-- != 0 && *dst != '\0') dst++;
    dlen = dst - odst;
    n = dsize - dlen;
    if (n == 0) return (dlen + strlen(src));
    while (*src != '\0') {
        if (n != 1) {
            *dst++ = *src;
            n--;
        }
        src++;
    }
    *dst = '\0';
    return (dlen + (src - osrc));
}
void utoa_hex_s(uint64_t val, char* buf, size_t size) {
    snprintf(buf, size, "%lx", val);
}

task_t* task_get_current(void) {
    return &mock_task;
}

void* pmm_alloc_page(void) {
    /* Return dummy non-zero address */
    static uint8_t page[4096];
    return page;
}

uint64_t hal_mem_get_phys(uint64_t virt) {
    (void)virt;
    return 0; /* Always unmapped initially */
}

void hal_mem_map(uint64_t phys, uint64_t virt, uint32_t flags) {
    (void)phys; (void)virt; (void)flags;
}

fs_node_t* vfs_lookup(fs_node_t* root, const char* path) {
    (void)root;
    if (strcmp(path, "/broken_elf") == 0) {
        static fs_node_t node;
        memset(&node, 0, sizeof(node));
        return &node;
    }
    return NULL;
}

uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node;

    if (offset == 0 && size == sizeof(Elf64_Ehdr)) {
        memcpy(buffer, &elf_header, sizeof(Elf64_Ehdr));
        return size;
    }

    if (offset == sizeof(Elf64_Ehdr) && size == sizeof(Elf64_Phdr)) {
        memcpy(buffer, &prog_header, sizeof(Elf64_Phdr));
        return size;
    }

    /* Simulate Read Failure for Segment Data */
    if (offset == 4096) {
        printf("[MOCK] Simulating read failure (returning 0)\n");
        return 0;
    }

    return 0;
}

void kfree(void* ptr) {
    (void)ptr;
}

/* Now include the code under test */
#include "../kernel/fs/elf.c"

int main() {
    printf("Setting up ELF Read Failure Test...\n");

    /* Map memory at 4GB to simulate the target load address */
    void* mapping = mmap((void*)0x100000000ULL, 4096 * 4, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

    if (mapping == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    /* Setup ELF Header */
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

    /* Setup Program Header */
    memset(&prog_header, 0, sizeof(Elf64_Phdr));
    prog_header.p_type = PT_LOAD;
    prog_header.p_flags = PF_R | PF_X;
    prog_header.p_offset = 4096;
    prog_header.p_vaddr = 0x100000000;
    prog_header.p_paddr = 0;
    prog_header.p_memsz = 4096;
    prog_header.p_filesz = 4096;
    prog_header.p_align = 4096;

    /* Run ELF Loader */
    printf("Loading broken ELF...\n");
    uint64_t entry = elf_load_file("/broken_elf", 0);

    if (entry != 0) {
        printf("[TEST] FAILURE: elf_load_file returned success (entry=0x%lx) despite read failure.\n", entry);
        return 1;
    }

    printf("[TEST] SUCCESS: elf_load_file returned 0 as expected.\n");
    return 0;
}
