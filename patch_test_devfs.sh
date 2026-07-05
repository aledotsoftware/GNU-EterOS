sed -i '/task_t\* task_get_current/a task_t* task_get_by_id(uint32_t id) { return &mock_task; }' tests/test_devfs.c
