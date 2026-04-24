with open('kernel/fs/devfs.c', 'r') as f:
    content = f.read()

content = content.replace('if (request == (unsigned int)BINDER_VERSION_IOWR) {', 'if ((unsigned int)request == (unsigned int)BINDER_VERSION_IOWR) {')

with open('kernel/fs/devfs.c', 'w') as f:
    f.write(content)
