#include <types.h>
#include <elf.h>
#include <fs/vfs.h>
#include <hal.h>
#include <pmm.h>
#include <mm.h>
#include <string.h>
#include <serial.h>
#include <task.h>
#include <vmm.h>

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
        if (current->os_abi == ELFOSABI_LINUX) {
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

            /* Create VMA for Demand Paging */
            if (current) {
                vm_area_t* vma = (vm_area_t*)kmalloc(sizeof(vm_area_t));
                if (!vma) {
                     serial_write_string("[ELF] OOM Allocating VMA\n");
                     kfree(node);
                     return 0;
                }
                vma->start = start_page;
                vma->end   = end_page;
                vma->node  = node;

                /* Adjust offset to align with start_page */
                uint64_t diff = vaddr - start_page;
                if (file_offset >= diff) {
                     vma->offset = file_offset - diff;
                } else {
                     vma->offset = 0;
                }

                vma->file_size = file_size + diff;
                vma->flags = final_flags;

                /* Increment node refcount */
                if (node->ref_count == 0) node->ref_count = 1;
                node->ref_count++;

                /* Add to list */
                vma->next = current->mmap_list;
                current->mmap_list = vma;

                /* Map as Demand Paging */
                for (uint64_t addr = start_page; addr < end_page; addr += PAGE_SIZE) {
                     uint64_t vmm_flags = PAGE_USER;
                     if (final_flags & HAL_MEM_WRITE) vmm_flags |= PAGE_WRITE;

                     vmm_map_demand_page(addr, vmm_flags);
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

    if (node->ref_count > 0) node->ref_count--;
    if (node->ref_count == 0) {
        kfree(node);
    }
    return header.e_entry + load_offset;
}
