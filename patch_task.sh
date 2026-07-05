sed -i '/uint64_t       mmap_base;/a \
    uint64_t       binder_mmap_base;\
    uint64_t       binder_mmap_size;\
    uint64_t       binder_mmap_offset;\
    void*          binder_mmap_kptr;' include/task.h
