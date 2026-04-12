import sys

def main():
    with open('tests/test_syscall_openat.c', 'r') as f:
        lines = f.readlines()

    for i, line in enumerate(lines):
        if line.startswith('fs_node_t *vfs_lookup_ext(fs_node_t *root, const char *path, int follow_symlink)'):
            lines[i+1] = '    fs_node_t* n = malloc(sizeof(fs_node_t)); memset(n, 0, sizeof(fs_node_t)); n->flags = FS_FILE; return n;\n'
            break

    with open('tests/test_syscall_openat.c', 'w') as f:
        f.writelines(lines)

if __name__ == '__main__':
    main()
