sed -i '/\/\* Binder VMA - Allocate anonymous page for receive buffer \*\//c\
            /* Binder VMA - Allocate mapped page and record in task_t */' kernel/arch/x86_64/syscall.c
awk '
/    } else if \(is_binder\) {/ {
    print $0;
    in_binder = 1;
    next;
}
/#endif/ {
    print $0;
    if (in_binder) {
        print "            if (v == start) {";
        print "                current->binder_mmap_base = start;";
        print "                current->binder_mmap_size = len;";
        print "                current->binder_mmap_offset = 0;";
        print "                current->binder_mmap_kptr = (void*)phys;";
        print "            }";
        in_binder = 0;
    }
    next;
}
{ print $0 }
' kernel/arch/x86_64/syscall.c > syscall_new.c
mv syscall_new.c kernel/arch/x86_64/syscall.c
