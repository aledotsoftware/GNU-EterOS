import sys

def main():
    with open('tests/test_sys_rw_perms.c', 'r') as f:
        lines = f.readlines()

    insert_idx = -1
    for i, line in enumerate(lines):
        if line.startswith('#include "../kernel/arch/x86_64/syscall.c"'):
            insert_idx = i
            break

    if insert_idx != -1:
        new_lines = lines[:insert_idx] + [
            "task_t* task_get_at(int index) { (void)index; return NULL; }\n",
            "int task_get_count(void) { return 1; }\n",
            "void task_exit_signal(int sig) { (void)sig; }\n",
            "int task_waitid(int idtype, int id, int options, int* out_pid, int* out_status, int* out_code) {\n",
            "    (void)idtype; (void)id; (void)options; (void)out_pid; (void)out_status; (void)out_code;\n",
            "    return -1;\n",
            "}\n"
        ] + lines[insert_idx:]
        with open('tests/test_sys_rw_perms.c', 'w') as f:
            f.writelines(new_lines)
        print("Added mocks.")
    else:
        print("Could not find the insertion point.")

if __name__ == '__main__':
    main()
