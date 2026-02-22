#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>

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

/* Test Control Enum */
typedef enum {
    TEST_CASE_NONE,
    TEST_CASE_FILE_NOT_FOUND,
    TEST_CASE_HEADER_READ_FAIL,
    TEST_CASE_INVALID_MAGIC,
    TEST_CASE_UNSUPPORTED_TYPE,
    TEST_CASE_WRONG_ARCH,
    TEST_CASE_PH_READ_FAIL,
    TEST_CASE_SEGMENT_OVERFLOW,
    TEST_CASE_FILE_LIMIT,
    /* TEST_CASE_FILE_WRAPAROUND is unreachable due to 32-bit limit checks */
    TEST_CASE_BUFFER_OVERFLOW,
    TEST_CASE_USER_SPACE_VIOLATION,
    TEST_CASE_IDENTITY_MAP_CONFLICT,
    TEST_CASE_SEGMENT_DATA_READ_FAIL,
    TEST_CASE_MAX
} test_case_t;

static test_case_t current_test_case = TEST_CASE_NONE;

/* Globals for testing */
static task_t mock_task;
static Elf64_Ehdr elf_header;
static Elf64_Phdr prog_header;

/* Mock Functions */

void serial_write_string(const char* str) {
    // printf("[SERIAL] %s", str); // Uncomment for debug
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
    if (current_test_case == TEST_CASE_FILE_NOT_FOUND) {
        return NULL;
    }
    static fs_node_t node;
    memset(&node, 0, sizeof(node));
    return &node;
}

uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node;

    /* Simulate Header Read Failure */
    if (current_test_case == TEST_CASE_HEADER_READ_FAIL && offset == 0) {
        return 0;
    }

    /* ELF Header Read */
    if (offset == 0 && size == sizeof(Elf64_Ehdr)) {
        memcpy(buffer, &elf_header, sizeof(Elf64_Ehdr));
        return size;
    }

    /* Simulate Program Header Read Failure */
    if (current_test_case == TEST_CASE_PH_READ_FAIL && offset == elf_header.e_phoff) {
        return 0;
    }

    /* Program Header Read */
    if (offset == elf_header.e_phoff && size == sizeof(Elf64_Phdr)) {
        memcpy(buffer, &prog_header, sizeof(Elf64_Phdr));
        return size;
    }

    /* Simulate Data Read Failure */
    if (current_test_case == TEST_CASE_SEGMENT_DATA_READ_FAIL && offset == prog_header.p_offset) {
        return 0;
    }

    /* Segment Data Read */
    if (offset == prog_header.p_offset && size == prog_header.p_filesz) {
        memset(buffer, 0x90, size); // Fill with NOPs
        return size;
    }

    return 0;
}

void kfree(void* ptr) {
    (void)ptr;
}

/* Now include the code under test */
#include "../kernel/fs/elf.c"

void setup_test_case(test_case_t tc) {
    current_test_case = tc;

    // Reset headers to valid state first
    memset(&elf_header, 0, sizeof(Elf64_Ehdr));
    elf_header.e_ident[0] = 0x7F; elf_header.e_ident[1] = 'E'; elf_header.e_ident[2] = 'L'; elf_header.e_ident[3] = 'F';
    elf_header.e_type = ET_EXEC;
    elf_header.e_machine = EM_X86_64;
    elf_header.e_entry = 0x100000000;
    elf_header.e_phoff = sizeof(Elf64_Ehdr);
    elf_header.e_phentsize = sizeof(Elf64_Phdr);
    elf_header.e_phnum = 1;

    memset(&prog_header, 0, sizeof(Elf64_Phdr));
    prog_header.p_type = PT_LOAD;
    prog_header.p_flags = PF_R | PF_X;
    prog_header.p_offset = 4096;
    prog_header.p_vaddr = 0x100000000;
    prog_header.p_filesz = 4096;
    prog_header.p_memsz = 4096;
    prog_header.p_align = 4096;

    // Apply specific modifications
    switch(tc) {
        case TEST_CASE_INVALID_MAGIC:
            elf_header.e_ident[0] = 'X';
            break;
        case TEST_CASE_UNSUPPORTED_TYPE:
            elf_header.e_type = 0xFFFF;
            break;
        case TEST_CASE_WRONG_ARCH:
            elf_header.e_machine = 0xFFFF;
            break;
        case TEST_CASE_SEGMENT_OVERFLOW:
            prog_header.p_vaddr = UINT64_MAX - 100;
            prog_header.p_memsz = 200;
            break;
        case TEST_CASE_FILE_LIMIT:
            prog_header.p_offset = (uint64_t)UINT32_MAX + 10;
            break;
        /* case TEST_CASE_FILE_WRAPAROUND:
            // Dead code in elf.c: file_offset > UINT32_MAX check prevents 64-bit overflow
            break; */
        case TEST_CASE_BUFFER_OVERFLOW:
            prog_header.p_filesz = 8192;
            prog_header.p_memsz = 4096;
            break;
        case TEST_CASE_USER_SPACE_VIOLATION:
            prog_header.p_vaddr = 0x8000000000000000ULL; // Kernel space start usually
            break;
        case TEST_CASE_IDENTITY_MAP_CONFLICT:
            prog_header.p_vaddr = 0x100000; // Low memory
            break;
        default:
            break;
    }
}

const char* get_test_name(test_case_t tc) {
    switch(tc) {
        case TEST_CASE_FILE_NOT_FOUND: return "File Not Found";
        case TEST_CASE_HEADER_READ_FAIL: return "Header Read Fail";
        case TEST_CASE_INVALID_MAGIC: return "Invalid Magic";
        case TEST_CASE_UNSUPPORTED_TYPE: return "Unsupported Type";
        case TEST_CASE_WRONG_ARCH: return "Wrong Arch";
        case TEST_CASE_PH_READ_FAIL: return "PH Read Fail";
        case TEST_CASE_SEGMENT_OVERFLOW: return "Segment Overflow";
        case TEST_CASE_FILE_LIMIT: return "File Limit";
        /* case TEST_CASE_FILE_WRAPAROUND: return "File Wraparound"; */
        case TEST_CASE_BUFFER_OVERFLOW: return "Buffer Overflow";
        case TEST_CASE_USER_SPACE_VIOLATION: return "User Space Violation";
        case TEST_CASE_IDENTITY_MAP_CONFLICT: return "Identity Map Conflict";
        case TEST_CASE_SEGMENT_DATA_READ_FAIL: return "Segment Data Read Fail";
        default: return "Unknown";
    }
}

int main() {
    printf("Setting up ELF Header Failure Tests...\n");

    /* Map memory at 4GB to simulate the target load address */
    void* mapping = mmap((void*)0x100000000ULL, 4096 * 4, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

    if (mapping == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    int failures = 0;
    for (int i = 1; i < TEST_CASE_MAX; i++) {
        setup_test_case((test_case_t)i);
        const char* name = get_test_name((test_case_t)i);
        printf("[TEST] Running Case: %s... ", name);

        uint64_t entry = elf_load_file("/test_elf", 0);

        if (entry != 0) {
            printf("FAILED: elf_load_file returned success (entry=0x%lx)\n", entry);
            failures++;
        } else {
            printf("PASSED\n");
        }
    }

    if (failures > 0) {
        printf("[TEST] %d tests failed.\n", failures);
        return 1;
    }

    printf("[TEST] All tests passed.\n");
    return 0;
}
