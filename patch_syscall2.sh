sed -i '/\/\* Binder VMA - Allocate anonymous page for receive buffer \*\//c\
            /* Binder VMA - Allocate mapped page and record in task_t */' kernel/arch/x86_64/syscall.c
