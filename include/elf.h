#ifndef ELF_H
#define ELF_H

#include <types.h>

/* ELF Types */
typedef uint64_t Elf64_Addr;
typedef uint16_t Elf64_Half;
typedef uint64_t Elf64_Off;
typedef int32_t  Elf64_Sword;
typedef int64_t  Elf64_Sxword;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Lword;
typedef uint64_t Elf64_Xword;

/* ELF Header */
#define EI_NIDENT 16
#define EI_OSABI  7

#define ELFOSABI_NONE  0
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

/* e_type */
#define ET_NONE 0
#define ET_REL  1
#define ET_EXEC 2
#define ET_DYN  3
#define ET_CORE 4

/* e_machine */
#define EM_X86_64 62

/* Program Header */
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

/* p_type */
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6

/* p_flags */
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

typedef struct {
    uint64_t entry;
    uint64_t phdr;
    uint64_t phent;
    uint64_t phnum;
    uint64_t base;
} elf_auxv_info_t;

/* Function prototypes */
uint64_t elf_load_file(const char* path, uint64_t base_vaddr);
int elf_get_interp(const char* path, char* out_interp, uint32_t out_interp_size);
int elf_get_auxv_info(const char* path, uint64_t base_vaddr, elf_auxv_info_t* out_info);

#endif
