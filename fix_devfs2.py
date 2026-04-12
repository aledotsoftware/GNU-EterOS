import sys

def main():
    with open('tests/test_devfs.c', 'r') as f:
        lines = f.readlines()

    for i, line in enumerate(lines):
        if line.startswith('task_t* task_get_at(int index)'):
            lines[i] = 'struct task* task_get_at(int index) { (void)index; return NULL; }\n'
            lines[i+2] = 'void task_wakeup(struct task* task) { (void)task; }\n'
            lines[i+3] = 'struct task* task_get_current(void) { static struct task t; return &t; }\n'
            lines[i+4] = 'bool keyboard_has_input(void) { return false; }\n'
            break

    # Include task.h before mocks so struct task is known
    insert_idx = -1
    for i, line in enumerate(lines):
        if line.startswith('struct task* task_get_at(int index)'):
            insert_idx = i
            break

    if insert_idx != -1:
        lines.insert(insert_idx, '#include "task.h"\n')
        lines.insert(insert_idx, '#include <stdbool.h>\n')

    with open('tests/test_devfs.c', 'w') as f:
        f.writelines(lines)

if __name__ == '__main__':
    main()
