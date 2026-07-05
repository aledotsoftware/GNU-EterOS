sed -i 's/PAGE_ALIGN_UP(tr.data_size)/(((tr.data_size) + PAGE_SIZE - 1) \& ~(PAGE_SIZE - 1))/g' kernel/fs/devfs.c
