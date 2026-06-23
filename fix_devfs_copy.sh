sed -i 's/temp_payload = (uint8_t\*)(uintptr_t)(target_task->binder_mmap_offset);/temp_payload = (uint8_t*)(uintptr_t)target_vaddr;/' kernel/fs/devfs.c
