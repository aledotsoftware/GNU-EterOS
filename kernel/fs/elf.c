#include <types.h>
#include <elf.h>
#include <fs/vfs.h>
#include <vmm.h>
#include <pmm.h>
#include <string.h>
#include <serial.h>

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
        return 0;
    }

    /* Verify Magic: 0x7F 'E' 'L' 'F' */
    if (header.e_ident[0] != 0x7F ||
        header.e_ident[1] != 'E' ||
        header.e_ident[2] != 'L' ||
        header.e_ident[3] != 'F') {
        serial_write_string("[ELF] Invalid ELF Magic.\n");
        return 0;
    }

    if (header.e_type != ET_EXEC && header.e_type != ET_DYN) {
        serial_write_string("[ELF] Unsupported ELF Type (Must be EXEC or DYN).\n");
        return 0;
    }

    if (header.e_machine != EM_X86_64) {
        serial_write_string("[ELF] Not x86_64 machine type.\n");
        return 0;
    }

    uint64_t load_offset = 0;
    if (header.e_type == ET_DYN) {
        load_offset = base_vaddr;
        serial_write_string("[ELF] Type: ET_DYN (PIE). Applying base offset.\n");
    } else {
        serial_write_string("[ELF] Type: ET_EXEC. Ignoring base offset.\n");
    }

    /* Iterate Program Headers */
    for (int i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr phdr;
        uint64_t offset = header.e_phoff + (i * header.e_phentsize);

        if (read_fs(node, offset, sizeof(Elf64_Phdr), (uint8_t*)&phdr) != sizeof(Elf64_Phdr)) {
            serial_write_string("[ELF] Failed to read Program Header.\n");
            return 0;
        }

        if (phdr.p_type == PT_LOAD) {
            uint64_t mem_size = phdr.p_memsz;
            uint64_t file_size = phdr.p_filesz;
            uint64_t vaddr = phdr.p_vaddr + load_offset;
            uint64_t file_offset = phdr.p_offset;

            /* Security Check: Ensure segment is in User Space */
            if (vaddr >= 0x0000800000000000 || (vaddr + mem_size) >= 0x0000800000000000) {
                serial_write_string("[ELF] Error: Segment violates User Space boundaries.\n");
                return 0;
            }

            /* Warning: Identity Map Conflict (< 4GB) */
            if (vaddr < 0x100000000) {
                serial_write_string("[ELF] Warning: Segment address < 4GB conflicts with Kernel Identity Map.\n");
                /* We continue, but it will likely crash if we don't switch CR3 */
            }

            /* Calculate page range */
            uint64_t start_page = PAGE_ALIGN_DOWN(vaddr);
            uint64_t end_page   = PAGE_ALIGN_UP(vaddr + mem_size);

            /* Map pages */
            for (uint64_t addr = start_page; addr < end_page; addr += PAGE_SIZE) {
                /* We check if mapped. If mapped by kernel (huge pages), we can't easily overwrite without splitting. */
                /* Assuming vaddr >= 4GB, it should be unmapped initially. */
                if (vmm_virt_to_phys(addr) == 0) {
                     void* phys = pmm_alloc_page();
                     if (!phys) {
                         serial_write_string("[ELF] OOM during loading.\n");
                         return 0;
                     }
                     /* Always map as USER | WRITE | PRESENT for loading */
                     vmm_map_page((uint64_t)phys, addr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

                     /* Zero the page content initially */
                     memset((void*)addr, 0, PAGE_SIZE);
                }
            }

            /* Now copy data */
            if (file_size > 0) {
                if (read_fs(node, file_offset, file_size, (uint8_t*)vaddr) != file_size) {
                     serial_write_string("[ELF] Failed to read segment data.\n");
                }
            }
        }
    }

    return header.e_entry + load_offset;
}
