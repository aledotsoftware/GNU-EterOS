import sys

def main():
    with open('tests/test_procfs.c', 'r') as f:
        lines = f.readlines()

    insert_idx = -1
    for i, line in enumerate(lines):
        if line.startswith('#include "../kernel/fs/procfs.c"'):
            insert_idx = i
            break

    if insert_idx != -1:
        new_lines = lines[:insert_idx] + [
            "task_t* task_get_by_id(int id) { (void)id; return NULL; }\n",
            "task_t* task_get_at(int index) { (void)index; return NULL; }\n",
            "int task_get_max(void) { return 0; }\n"
        ] + lines[insert_idx:]
        with open('tests/test_procfs.c', 'w') as f:
            f.writelines(new_lines)
        print("Added mocks.")
    else:
        print("Could not find the insertion point.")

if __name__ == '__main__':
    main()
