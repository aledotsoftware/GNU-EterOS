#include "../../include/vmm.h"
#include <types.h>
#include <elf.h>
#include <fs/vfs.h>
#include <hal.h>
#include <pmm.h>
#include <mm.h>
#include <string.h>
#include <serial.h>
#include <task.h>
#include <errno.h>

int elf_get_interp(const char* path, char* out_interp, uint32_t out_interp_size) {
    fs_node_t* node;
    Elf64_Ehdr header;

    if (!path || !out_interp || out_interp_size < 2) return -EINVAL;
    out_interp[0] = '\0';

    node = vfs_lookup(fs_root, path);
    if (!node) return -ENOENT;

    if (read_fs(node, 0, sizeof(Elf64_Ehdr), (uint8_t*)&header) != sizeof(Elf64_Ehdr)) {
        kfree(node);
        return -ENOEXEC;
    }

    if (header.e_ident[0] != 0x7F ||
        header.e_ident[1] != 'E' ||
        header.e_ident[2] != 'L' ||
        header.e_ident[3] != 'F') {
        kfree(node);
        return -ENOEXEC;
    }

    if (header.e_phentsize < sizeof(Elf64_Phdr) || header.e_phnum == 0) {
        kfree(node);
        return 0;
    }

    for (uint16_t i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr phdr;
        uint64_t phoff = header.e_phoff + ((uint64_t)i * header.e_phentsize);

        if (read_fs(node, (uint32_t)phoff, sizeof(Elf64_Phdr), (uint8_t*)&phdr) != sizeof(Elf64_Phdr)) {
            kfree(node);
            return -ENOEXEC;
        }

        if (phdr.p_type != PT_INTERP) continue;

        if (phdr.p_filesz < 2 || phdr.p_filesz > out_interp_size) {
            kfree(node);
            return -E2BIG;
        }
        if (phdr.p_offset > UINT32_MAX) {
            kfree(node);
            return -ENOEXEC;
        }

        if (read_fs(node, (uint32_t)phdr.p_offset, (uint32_t)phdr.p_filesz, (uint8_t*)out_interp) != (ssize_t)phdr.p_filesz) {
            kfree(node);
            return -ENOEXEC;
        }

        out_interp[out_interp_size - 1] = '\0';
        for (uint32_t j = 0; j < phdr.p_filesz; j++) {
            if (out_interp[j] == '\0') {
                kfree(node);
                return 1;
            }
        }

        /* Ensure NUL termination even if malformed payload omitted it. */
        if (phdr.p_filesz < out_interp_size) {
            out_interp[phdr.p_filesz] = '\0';
        } else {
            out_interp[out_interp_size - 1] = '\0';
        }
        kfree(node);
        return 1;
    }

    kfree(node);
    return 0;
}

int elf_get_auxv_info(const char* path, uint64_t base_vaddr, elf_auxv_info_t* out_info) {
    fs_node_t* node;
    Elf64_Ehdr header;
    uint64_t load_offset = 0;
    uint64_t first_load_vaddr = 0;
    uint64_t first_load_off = 0;
    int have_first_load = 0;

    if (!path || !out_info) return -EINVAL;
    memset(out_info, 0, sizeof(*out_info));

    node = vfs_lookup(fs_root, path);
    if (!node) return -ENOENT;

    if (read_fs(node, 0, sizeof(Elf64_Ehdr), (uint8_t*)&header) != sizeof(Elf64_Ehdr)) {
        kfree(node);
        return -ENOEXEC;
    }

    if (header.e_ident[0] != 0x7F ||
        header.e_ident[1] != 'E' ||
        header.e_ident[2] != 'L' ||
        header.e_ident[3] != 'F') {
        kfree(node);
        return -ENOEXEC;
    }

    if (header.e_type == ET_DYN) {
        load_offset = base_vaddr;
    }

    out_info->entry = header.e_entry + load_offset;
    out_info->phent = header.e_phentsize;
    out_info->phnum = header.e_phnum;
    out_info->base = load_offset;

    if (header.e_phentsize < sizeof(Elf64_Phdr) || header.e_phnum == 0) {
        kfree(node);
        return 0;
    }

    for (uint16_t i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr phdr;
        uint64_t phoff = header.e_phoff + ((uint64_t)i * header.e_phentsize);

        if (read_fs(node, (uint32_t)phoff, sizeof(Elf64_Phdr), (uint8_t*)&phdr) != sizeof(Elf64_Phdr)) {
            kfree(node);
            return -ENOEXEC;
        }

        if (phdr.p_type == PT_PHDR) {
            out_info->phdr = phdr.p_vaddr + load_offset;
        }

        if (!have_first_load && phdr.p_type == PT_LOAD) {
            first_load_vaddr = phdr.p_vaddr + load_offset;
            first_load_off = phdr.p_offset;
            have_first_load = 1;
        }
    }

    if (out_info->phdr == 0 && have_first_load) {
        if (header.e_phoff >= first_load_off) {
            out_info->phdr = first_load_vaddr + (header.e_phoff - first_load_off);
        }
    }

    kfree(node);
    return 0;
}

uint64_t elf_load_file(const char* path, uint64_t base_vaddr) {
    serial_write_string("[ELF] Loading file: ");
    serial_write_string(path);
    serial_write_string("\n");

    fs_node_t* node = vfs_lookup(fs_root, path);
    if (!node) {
        serial_write_string("[ELF] File not found.\n");
        return 0;
    }

    /* SECURITY FIX: Enforce execute permissions */
    task_t* _sec_current = task_get_current();
    if (_sec_current && _sec_current->euid != 0) {
        uint32_t granted = 0;
        if (_sec_current->euid == node->uid) granted = (node->mask >> 6) & 7;
        else if (_sec_current->egid == node->gid) granted = (node->mask >> 3) & 7;
        else granted = node->mask & 7;

        if (!(granted & 1)) { /* 1 = Execute */
            serial_write_string("[ELF] Permission denied. Missing execute permission.\n");
            kfree(node);
            return 0;
        }
    }

    Elf64_Ehdr header;
    if (read_fs(node, 0, sizeof(Elf64_Ehdr), (uint8_t*)&header) != sizeof(Elf64_Ehdr)) {
        serial_write_string("[ELF] Failed to read ELF header.\n");
        kfree(node);
        return 0;
    }

    /* Verify Magic: 0x7F 'E' 'L' 'F' */
    if (header.e_ident[0] != 0x7F ||
        header.e_ident[1] != 'E' ||
        header.e_ident[2] != 'L' ||
        header.e_ident[3] != 'F') {
        serial_write_string("[ELF] Invalid ELF Magic.\n");
        kfree(node);
        return 0;
    }

    if (header.e_type != ET_EXEC && header.e_type != ET_DYN) {
        serial_write_string("[ELF] Unsupported ELF Type (Must be EXEC or DYN).\n");
        kfree(node);
        return 0;
    }

    if (header.e_machine != EM_X86_64) {
        serial_write_string("[ELF] Not x86_64 machine type.\n");
        kfree(node);
        return 0;
    }

    task_t* current = task_get_current();
    if (current) {
        current->os_abi = header.e_ident[EI_OSABI];
        if (current->os_abi == ELFOSABI_LINUX || current->os_abi == ELFOSABI_NONE) {
             current->os_abi = ELFOSABI_LINUX;
             current->is_linux = 1;
             serial_write_string("[ELF] Detected Linux ABI.\n");
        } else {
             serial_write_string("[ELF] Detected Native/SysV ABI.\n");
        }
    }

    uint64_t load_offset = 0;
    if (header.e_type == ET_DYN) {
        load_offset = base_vaddr;
        serial_write_string("[ELF] Type: ET_DYN (PIE). Applying base offset.\n");
    } else {
        serial_write_string("[ELF] Type: ET_EXEC. Ignoring base offset.\n");
    }

    uint64_t max_vaddr = 0;

    /* Iterate Program Headers */
    for (int i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr phdr;
        uint64_t offset = header.e_phoff + (i * header.e_phentsize);

        if (read_fs(node, offset, sizeof(Elf64_Phdr), (uint8_t*)&phdr) != sizeof(Elf64_Phdr)) {
            serial_write_string("[ELF] Failed to read Program Header.\n");
            kfree(node);
            return 0;
        }

        if (phdr.p_type == PT_LOAD) {
            uint64_t mem_size = phdr.p_memsz;
            uint64_t file_size = phdr.p_filesz;
            uint64_t vaddr = phdr.p_vaddr + load_offset;
            uint64_t file_offset = phdr.p_offset;

            /* Check file_size versus mem_size */
            if (file_size > mem_size) {
                serial_write_string("[ELF] Error: file_size > mem_size in PT_LOAD.\n");
                kfree(node);
                return 0;
            }

            /* Check for integer overflow in vaddr + mem_size */
            if (vaddr + mem_size < vaddr) {
                serial_write_string("[ELF] Error: Segment address wraparound (overflow).\n");
                kfree(node);
                return 0;
            }

            /* SECURITY FIX: Check for file offset/size truncation (since read_fs uses uint32_t) */
            if (file_offset > UINT32_MAX || file_size > UINT32_MAX) {
                serial_write_string("[ELF] Error: Segment exceeds 32-bit file limits.\n");
                kfree(node);
                return 0;
            }

            /* Check for integer overflow in file_offset + file_size */
            if (file_offset + file_size < file_offset) {
                 serial_write_string("[ELF] Error: File offset wraparound (overflow).\n");
                 kfree(node);
                 return 0;
            }

            if (vaddr + mem_size > max_vaddr) {
                max_vaddr = vaddr + mem_size;
            }

            /* Security Check: Prevent buffer overflow if file size exceeds memory size */
            if (file_size > mem_size) {
                serial_write_string("[ELF] Error: p_filesz > p_memsz (Buffer Overflow Risk). Load rejected.\n");
                kfree(node);
                return 0;
            }

            /* Security Check: Ensure segment is in User Space */
            if (vaddr >= 0x0000800000000000 || (vaddr + mem_size) >= 0x0000800000000000) {
                serial_write_string("[ELF] Error: Segment violates User Space boundaries.\n");
                kfree(node);
                return 0;
            }

            /* Error: Identity Map Conflict (Check against USER_BASE) */
            if (vaddr < USER_BASE) {
                serial_write_string("[ELF] Error: Segment address conflicts with Kernel bounds. Load rejected.\n");
                kfree(node);
                return 0;
            }

            /* Determine final permissions based on ELF Header */
            uint32_t final_flags = HAL_MEM_USER | HAL_MEM_READ;
            if (phdr.p_flags & PF_W) final_flags |= HAL_MEM_WRITE;
            if (phdr.p_flags & PF_X) final_flags |= HAL_MEM_EXEC;

            /* Calculate page range */
            uint64_t start_page = PAGE_ALIGN_DOWN(vaddr);
            uint64_t end_page   = PAGE_ALIGN_UP(vaddr + mem_size);

            /* Pass 1: Map pages as RW (Read/Write) to allow loading content */
            for (uint64_t addr = start_page; addr < end_page; addr += PAGE_SIZE) {
                /* We check if mapped. If mapped by kernel (huge pages), we can't easily overwrite without splitting. */
                /* Assuming vaddr >= 4GB, it should be unmapped initially. */
                if (hal_mem_get_phys(addr) == 0) {
                     void* phys = pmm_alloc_page();
                     if (!phys) {
                         serial_write_string("[ELF] OOM during loading.\n");
                         kfree(node);
                         return 0;
                     }
                     /* Always map as USER | WRITE initially for loading */
                     /* Note: HAL_MEM_READ is implicit with map */
                     hal_mem_map((uint64_t)phys, addr, HAL_MEM_READ | HAL_MEM_WRITE | HAL_MEM_USER);

                     /* Zero the page content initially */
                     memset((void*)addr, 0, PAGE_SIZE);
                }
            }

            /* Now copy data (while pages are RW) */
            if (file_size > 0) {
                if (read_fs(node, file_offset, file_size, (uint8_t*)vaddr) != (ssize_t)file_size) {
                     serial_write_string("[ELF] Failed to read segment data.\n");
                     kfree(node);
                     return 0;
                }
            }

            /* Pass 2: Remap pages with strict permissions (W^X) */
            for (uint64_t addr = start_page; addr < end_page; addr += PAGE_SIZE) {
                 uint32_t page_flags = final_flags;

                 /* Handle shared/misaligned pages: */
                 /* If segment starts mid-page, that page is shared with previous segment. */
                 /* If segment ends mid-page, that page is shared with next segment. */
                 /* We MUST keep Write permission on shared pages to avoid breaking adjacent data segments. */

                 bool is_shared = false;
                 if (addr == start_page && (vaddr & (PAGE_SIZE - 1))) is_shared = true;
                 if (addr == end_page - PAGE_SIZE && ((vaddr + mem_size) & (PAGE_SIZE - 1))) is_shared = true;

                 if (is_shared) {
                     page_flags |= HAL_MEM_WRITE;
                 }

                 /* Apply new flags (e.g. adding EXEC or removing WRITE) */
                 uint64_t phys = hal_mem_get_phys(addr);
                 if (phys) {
                     hal_mem_map(phys, addr, page_flags);
                 }
            }
        }
    }

    /* Pass 3: Parse PT_DYNAMIC to load shared libraries (.so) */
    for (int i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr phdr;
        uint64_t offset = header.e_phoff + (i * header.e_phentsize);
        read_fs(node, offset, sizeof(Elf64_Phdr), (uint8_t*)&phdr);

        if (phdr.p_type == PT_DYNAMIC) {
            uint64_t dyn_vaddr = phdr.p_vaddr + load_offset;

            /* Validate dyn_vaddr against basic bounds to prevent kernel panic */
            if (dyn_vaddr < USER_BASE || dyn_vaddr + phdr.p_memsz < dyn_vaddr || dyn_vaddr + phdr.p_memsz > 0x0000800000000000) {
                serial_write_string("[ELF] Error: Invalid PT_DYNAMIC address.\n");
                continue;
            }

            Elf64_Dyn* dyn = (Elf64_Dyn*)dyn_vaddr;
            uint64_t strtab = 0;

            /* Limit iteration to prevent infinite loops on malformed DYNAMIC section */
            uint64_t max_dyn_entries = phdr.p_memsz / sizeof(Elf64_Dyn);

            /* First pass: find DT_STRTAB */
            for (uint64_t d = 0; d < max_dyn_entries; d++) {
                if (dyn[d].d_tag == DT_NULL) break;
                if (dyn[d].d_tag == DT_STRTAB) {
                    strtab = dyn[d].d_un.d_ptr + load_offset;
                    break;
                }
            }

            /* Second pass: load DT_NEEDED */
            if (strtab && strtab >= USER_BASE && strtab < 0x0000800000000000) {
                for (uint64_t d = 0; d < max_dyn_entries; d++) {
                    if (dyn[d].d_tag == DT_NULL) break;
                    if (dyn[d].d_tag == DT_NEEDED) {
                        /* Validate string pointer to prevent page faults */
                        uint64_t str_addr = strtab + dyn[d].d_un.d_val;
                        if (str_addr < USER_BASE || str_addr >= 0x0000800000000000) {
                            serial_write_string("[ELF] Error: Invalid DT_NEEDED string address.\n");
                            continue;
                        }

                        char* lib_name = (char*)str_addr;
                        char lib_path[256];
                        strlcpy(lib_path, "/lib/", sizeof(lib_path));
                        strlcat(lib_path, lib_name, sizeof(lib_path));

                        serial_write_string("[ELF] Autoloading dynamic library: ");
                        serial_write_string(lib_path);
                        serial_write_string("\n");

                        if (current) {
                            uint64_t lib_base = current->mmap_base;
                            elf_load_file(lib_path, lib_base);
                        }
                    }
                }
            }
        }
    }

    if (current && base_vaddr == 0) {
        current->brk = max_vaddr;
        serial_write_string("[ELF] Set process BRK to 0x");
        char brk_buf[32];
        utoa_hex_s(max_vaddr, brk_buf, sizeof(brk_buf));
        serial_write_string(brk_buf);
        serial_write_string("\n");
    } else if (current) {
        /* Advance mmap_base for shared libraries */
        current->mmap_base = PAGE_ALIGN_UP(max_vaddr + PAGE_SIZE);
    }

    kfree(node);
    return header.e_entry + load_offset;
}
