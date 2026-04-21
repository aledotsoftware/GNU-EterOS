# Virtual File System (VFS) API

El VFS proporciona una interfaz unificada para interactuar con diferentes sistemas de archivos en éterOS.

## Estructuras Principales

- `fs_node_t`: Representa un nodo en el sistema de archivos (archivo, directorio, dispositivo).
- `dirent_t`: Entrada de directorio.

## Funciones de la API

### Operaciones de Archivo
- `int vfs_open(const char *path, int flags)`: Abre un archivo y retorna un descriptor.
- `ssize_t vfs_read(int fd, void *buf, size_t count)`: Lee datos de un archivo.
- `ssize_t vfs_write(int fd, const void *buf, size_t count)`: Escribe datos en un archivo.
- `int vfs_close(int fd)`: Cierra un descriptor de archivo.

### Operaciones de Directorio
- `int vfs_mkdir(const char *path, mode_t mode)`: Crea un nuevo directorio.
- `struct dirent* vfs_readdir(int fd)`: Lee la siguiente entrada de un directorio.

## Implementación de un Driver de Filesystem

Para añadir un nuevo sistema de archivos:

1. Definir las funciones de callback (`read`, `write`, `open`, `close`, `readdir`, etc.).
2. Crear una función `mount` que inicialice el nodo raíz del filesystem.
3. Registrar el filesystem en el VFS.

### Ejemplo de Estructura de Driver

```c
struct fs_node* myfs_mount(struct fs_node* device, const char* path) {
    struct fs_node* root = kmalloc(sizeof(struct fs_node));
    strcpy(root->name, "/");
    root->mask = root->uid = root->gid = 0;
    root->flags = FS_DIRECTORY;
    root->read = &myfs_read;
    // ...
    return root;
}
```

## Referencias
- `kernel/fs/vfs.c`
- `include/fs/vfs.h`
- `docs/filesystem.md`
