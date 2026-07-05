sed -i 's/uint8_t \*temp_payload = NULL;/uint8_t \*temp_payload = NULL;\
                    task_t *target_task = NULL;\
                    if (tr.target.handle == 0 \&\& cmd != BC_REPLY) {\
                        target_task = task_get_by_id((uint32_t)binder_context_mgr_pid);\
                    } else if (cmd == BC_REPLY) {\
                        target_task = task_get_by_id(tr.target.handle);\
                    }/' kernel/fs/devfs.c
