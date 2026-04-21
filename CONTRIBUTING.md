# Guía de Contribución a éterOS

¡Gracias por tu interés en contribuir a éterOS! Queremos que colaborar sea lo más sencillo posible.

## Estándares de Código

### Estilo de C
- Usar 4 espacios para identación (no tabs).
- Llaves en la misma línea para `if`, `while`, `for`.
- Nombres de funciones en `snake_case`.
- Prefijar funciones de módulos (ej: `vfs_open`, `pmm_alloc`).

### Comentarios
- Documentar funciones complejas en el header correspondiente.
- Usar comentarios estilo Doxygen para APIs públicas.

## Proceso de Pull Requests

1.  **Fork** del repositorio.
2.  Crear una **rama** para tu funcionalidad o corrección (`git checkout -b feature/nueva-funcionalidad`).
3.  Realizar tus **commits** con mensajes claros y descriptivos.
4.  Asegurarte de que el sistema **compila** y los tests pasan (`make all`, `tests/run_tests.sh`).
5.  Enviar el **Pull Request**.

## Arquitectura del Proyecto

- `boot/`: Bootloaders de primera y segunda etapa.
- `kernel/`: El núcleo del sistema operativo.
  - `arch/`: Código específico de arquitectura (x86_64, etc).
  - `drivers/`: Controladores de hardware.
  - `fs/`: Sistema de archivos y VFS.
  - `mm/`: Gestión de memoria.
  - `net/`: Stack de red.
  - `gfx/`: Servidor gráfico Eterland.
- `userspace/`: Librerías de usuario (libc) y aplicaciones ELF.
- `docs/`: Documentación detallada.
- `tests/`: Suite de pruebas unitarias e integración.

## Setup del Entorno de Desarrollo

### Requisitos
- `gcc` (cross-compiler `x86_64-elf-gcc` recomendado)
- `nasm`
- `make`
- `qemu-system-x86_64`

### Compilación rápida
```bash
make all
make run
```
