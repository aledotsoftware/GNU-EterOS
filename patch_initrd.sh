sed -i 's/    return 1; \/\* EOF \*\//    return -ENOENT; \/\* EOF \*\//g' kernel/fs/initrd.c
