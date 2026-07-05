awk '
/    memset\(\(void\*\)v, 0, PAGE_SIZE\);/ {
    print $0;
    if (in_binder) {
        print "#endif";
        print "            if (v == start) {";
        print "                current->binder_mmap_base = start;";
        print "                current->binder_mmap_size = len;";
        print "                current->binder_mmap_offset = 0;";
        print "            }";
        in_binder = 0;
        skip_endif = 1;
        next;
    }
}
/    } else if \(is_binder\) {/ {
    print $0;
    in_binder = 1;
    next;
}
/#endif/ {
    if (skip_endif) {
        skip_endif = 0;
        next;
    }
    print $0;
    next;
}
{ print $0 }
' kernel/arch/x86_64/syscall.c > syscall_new.c
mv syscall_new.c kernel/arch/x86_64/syscall.c
