awk '
/                            temp_payload = \(uint8_t\*\)kmalloc\(\(size_t\)tr.data_size\);/ {
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
{ print $0 }
' kernel/fs/devfs.c > devfs_new2.c
mv devfs_new2.c kernel/fs/devfs.c
