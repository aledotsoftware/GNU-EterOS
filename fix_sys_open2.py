import sys

def main():
    with open('tests/test_sys_open.c', 'r') as f:
        lines = f.readlines()

    for i, line in enumerate(lines):
        if line.startswith('struct task* task_get_at(int index)'):
            lines[i] = 'task_t* task_get_at(int index) { (void)index; return NULL; }\n'
            break

    with open('tests/test_sys_open.c', 'w') as f:
        f.writelines(lines)

if __name__ == '__main__':
    main()
