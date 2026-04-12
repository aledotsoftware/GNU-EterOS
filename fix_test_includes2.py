import sys
import os
import glob

def fix_file(filepath):
    with open(filepath, 'r') as f:
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
        with open(filepath, 'w') as f:
            f.writelines(new_lines)
        return True
    return False

def main():
    files = glob.glob('tests/*.c')
    for f in files:
        # Check if already added
        with open(f, 'r') as file:
            content = file.read()
            if 'task_get_at' in content and 'task_waitid' in content:
                continue

            if '#include "../kernel/arch/x86_64/syscall.c"' in content:
                if fix_file(f):
                    print(f"Fixed {f}")

if __name__ == '__main__':
    main()
