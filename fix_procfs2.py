import sys

def main():
    with open('tests/test_procfs.c', 'r') as f:
        lines = f.readlines()

    for i, line in enumerate(lines):
        if line.startswith('task_t* task_get_by_id(int id)'):
            lines[i] = 'task_t* task_get_by_id(uint32_t id) { (void)id; return NULL; }\n'
            break

    with open('tests/test_procfs.c', 'w') as f:
        f.writelines(lines)

if __name__ == '__main__':
    main()
