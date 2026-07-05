awk '
/                            temp_payload = \(uint8_t\*\)kmalloc\(\(size_t\)tr.data_size\);/ {
    print "                            task_t *target_task = NULL;";
    print "                            if (tr.target.handle == 0 && cmd != BC_REPLY) {";
    print "                                target_task = task_get_by_id((uint32_t)binder_context_mgr_pid);";
    print "                            } else if (cmd == BC_REPLY) {";
    print "                                target_task = task_get_by_id(tr.target.handle);";
    print "                            }";
    print "                            if (target_task && target_task->binder_mmap_kptr && (target_task->binder_mmap_offset + tr.data_size <= target_task->binder_mmap_size)) {";
    print "                                /* Copy directly into receivers mapped memory area */";
    print "                                uint64_t target_vaddr = target_task->binder_mmap_base + target_task->binder_mmap_offset;";
    print "                                uint8_t *kptr = (uint8_t*)target_task->binder_mmap_kptr + target_task->binder_mmap_offset;";
    print "                                ";
    print "                                if (safe_copy_from_user(kptr, (void*)(uintptr_t)tr.data.ptr.buffer, (size_t)tr.data_size) == 0) {";
    print "                                    temp_payload = (uint8_t*)(uintptr_t)target_vaddr; /* Store the receivers VADDR so they can access it directly */";
    print "                                    target_task->binder_mmap_offset += PAGE_ALIGN_UP(tr.data_size);";
    print "                                } else {";
    print "                                    tr.data_size = 0;";
    print "                                }";
    print "                            } else {";
    print "                                /* Fallback to legacy kmalloc if no mmap or full */";
    print $0;
    skip_else = 1;
    next;
}
/                            } else {/ {
    if (skip_else) {
        print $0;
        skip_else = 0;
        print "                            }";
        next;
    }
    print $0;
    next;
}
/                             kfree\(\(void\*\)buffer_to_free\);/ {
    print "                             if (buffer_to_free < current->binder_mmap_base || buffer_to_free >= current->binder_mmap_base + current->binder_mmap_size) {";
    print $0;
    print "                             }";
    next;
}
/                        if \(safe_copy_to_user\(\(void\*\)\(uintptr_t\)tr_copy.data.ptr.buffer, payload_ptr, tr_copy.data_size\) != 0\) {/ {
    print "                        if ((uintptr_t)payload_ptr >= current->binder_mmap_base && (uintptr_t)payload_ptr < current->binder_mmap_base + current->binder_mmap_size) {";
    print "                            /* It is already in their mapped memory, just give them the pointer */";
    print "                            tr_copy.data.ptr.buffer = (uint64_t)(uintptr_t)payload_ptr;";
    print "                        } else {";
    print "                            /* Legacy copy */";
    print $0;
    skip_legacy_endif = 1;
    next;
}
/                        \/\* We don.t free payload_ptr here/ {
    if (skip_legacy_endif) {
        print "                        }";
        skip_legacy_endif = 0;
    }
    print $0;
    next;
}
{ print $0 }
' kernel/fs/devfs.c > devfs_new.c
mv devfs_new.c kernel/fs/devfs.c
