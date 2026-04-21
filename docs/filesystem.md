# Documentación del Sistema de Archivos

éterOS utiliza un Virtual File System (VFS) para abstraer el almacenamiento y proporcionar una interfaz POSIX a las aplicaciones.

## Arquitectura

El VFS mantiene un árbol de nodos (`fs_node_t`) que representan todos los elementos accesibles en el sistema.

### Drivers Soportados

1.  **InitRD**: RAM disk de solo lectura que contiene los archivos necesarios para el arranque. Se monta en `/`.
2.  **DevFS**: Filesystem virtual para dispositivos. Se monta en `/dev/`. Proporciona acceso a `null`, `zero`, `tty`, `input`, etc.
3.  **ProcFS**: Proporciona información sobre el estado del kernel y procesos. Se monta en `/proc/`.
4.  **FAT32**: Soporte para lectura y escritura en particiones de disco duro físicas. Se suele montar en `/mnt/`.
5.  **JFS (Journaling FS)**: Implementación experimental con soporte para transacciones.

## Uso de la API VFS

### Apertura de archivos
```c
int fd = open("/dev/tty", O_RDWR);
```

### Lectura y Escritura
```c
char buf[256];
read(fd, buf, sizeof(buf));
write(1, "Log\n", 4);
```

## Limitaciones Conocidas

- **FAT32**: Actualmente no soporta nombres largos (LFN), limitado a formato 8.3.
- **JFS**: La persistencia a disco está en fase de desarrollo; actualmente opera principalmente en RAM con journal persistente.
- **VFS**: El número máximo de archivos abiertos por proceso está limitado a 32 por defecto.

## Referencias
- `kernel/fs/README.md`
- `docs/api/vfs.md`
- `include/fs/vfs.h`
