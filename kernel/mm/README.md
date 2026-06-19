# eterOS - Memory Management Subsystem

This directory contains the core memory management components of eterOS, split into Physical Memory Management (PMM) and Virtual Memory Management (VMM).

## Physical Memory Manager (`pmm.c`)

The PMM is responsible for allocating and freeing raw physical frames (typically 4KB each) from the available RAM reported by the BIOS/UEFI/Bootloader.

### Features
- **Bitmap Allocator**: Tracks the usage of every physical page via a bitmap.
- **Reference Counting**: Maintains an array of reference counts for physical frames to support shared memory and Copy-on-Write (COW).
- **Reserved Regions**: Respects memory regions reserved by ACPI, the Bootloader, and the Kernel itself.

### Key API
- `pmm_alloc_page()`: Returns the physical address of a free frame.
- `pmm_free_page(phys)`: Marks a physical frame as free.
- `pmm_ref_page(phys)` / `pmm_unref_page(phys)`: Increments/decrements the reference count of a frame.

## Virtual Memory Manager (`vmm.c`)

The VMM manages page tables (PML4, PDPT, PD, PT on x86_64) to provide isolated virtual address spaces for the kernel and user-space tasks.

### Features
- **Page Mapping**: Maps physical frames to virtual addresses with specific permissions (Present, Read/Write, User/Supervisor, Execute Disable).
- **Address Space Cloning**: Creates copies of address spaces for `fork()` and `clone()`, utilizing COW (Copy-On-Write) for efficiency.
- **Demand Paging**: Handles page faults dynamically.
- **User Memory Access Verification**: Provides secure routines (`vmm_verify_user_access`, `vmm_safe_copy_to_user`, `vmm_safe_copy_from_user`) to safely read/write memory pointers passed from user-space, preventing kernel panics from invalid user pointers.

### Key API
- `vmm_map_page(phys, virt, flags)`: Maps a physical page to a virtual address.
- `vmm_unmap_page(virt)`: Unmaps a virtual page and flushes the TLB.
- `vmm_clone_pml4(cow)`: Clones the current address space.

## Heap Allocator (`heap.c`)

eterOS implements a basic contiguous kernel heap allocator (`kmalloc`, `kfree`) built on top of the VMM to provide dynamic memory allocation for kernel structures.
