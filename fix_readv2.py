import sys

def main():
    with open('tests/test_readv_security.c', 'r') as f:
        lines = f.readlines()

    for i, line in enumerate(lines):
        if line.startswith('int task_waitid(int idtype, int id, void* infop, int options)'):
            lines[i] = 'int task_waitid(int idtype, int id, int options, int* out_pid, int* out_status, int* out_code) {\n'
            lines[i+1] = '    (void)idtype; (void)id; (void)options; (void)out_pid; (void)out_status; (void)out_code;\n'
            break

    with open('tests/test_readv_security.c', 'w') as f:
        f.writelines(lines)

if __name__ == '__main__':
    main()
