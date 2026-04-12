import sys

def main():
    with open('tests/test_devfs.c', 'r') as f:
        lines = f.readlines()

    insert_idx = -1
    for i, line in enumerate(lines):
        if line.startswith('#include "../kernel/fs/devfs.c"'):
            insert_idx = i
            break

    if insert_idx != -1:
        new_lines = lines[:insert_idx] + [
            "task_t* task_get_at(int index) { (void)index; return NULL; }\n",
            "int task_get_max(void) { return 0; }\n",
            "void task_wakeup(task_t* task) { (void)task; }\n",
            "task_t* task_get_current(void) { static task_t t; return &t; }\n",
            "int keyboard_has_input(void) { return 0; }\n",
            "void task_yield(void) {}\n"
        ] + lines[insert_idx:]
        with open('tests/test_devfs.c', 'w') as f:
            f.writelines(new_lines)
        print("Added mocks.")
    else:
        print("Could not find the insertion point.")

if __name__ == '__main__':
    main()
