#include <types.h>
#include <elf.h>
#include <fs/vfs.h>
#include <hal.h>
#include <pmm.h>
#include <mm.h>
#include <string.h>
#include <serial.h>
#include <task.h>

uint64_t elf_load_file(const char* path, uint64_t base_vaddr) {
    serial_write_string("[ELF] Loading file: ");
    serial_write_string(path);
    serial_write_string("\n");

    fs_node_t* node = vfs_lookup(fs_root, path);
    if (!node) {
        serial_write_string("[ELF] File not found.\n");
        return 0;
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

            /* Error: Identity Map Conflict (< 4GB) */
            if (vaddr < 0x100000000) {
                serial_write_string("[ELF] Error: Segment address < 4GB conflicts with Kernel Identity Map. Load rejected.\n");
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

    if (current) {
        current->brk = max_vaddr;
        serial_write_string("[ELF] Set process BRK to 0x");
        char brk_buf[32];
        utoa_hex_s(max_vaddr, brk_buf, sizeof(brk_buf));
        serial_write_string(brk_buf);
        serial_write_string("\n");
    }

    kfree(node);
    return header.e_entry + load_offset;
}
