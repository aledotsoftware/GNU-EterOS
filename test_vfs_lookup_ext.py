import sys

def main():
    with open('tests/test_sys_open.c', 'r') as f:
        lines = f.readlines()

    for i, line in enumerate(lines):
        if line.startswith('fs_node_t *vfs_lookup_ext(fs_node_t *root, const char *path, int follow_symlink) {'):
            lines.insert(i+1, "    if (strcmp(path, \"/some/dir\") == 0) { static fs_node_t dir_node; dir_node.flags = FS_DIRECTORY; return &dir_node; }\n")
            break

    with open('tests/test_sys_open.c', 'w') as f:
        f.writelines(lines)

if __name__ == '__main__':
    main()
