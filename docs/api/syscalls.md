# Tabla de Syscalls de éterOS

éterOS implementa una interfaz de llamadas al sistema compatible con el ABI de Linux x86_64 para facilitar la portabilidad de aplicaciones.

## Tabla de Syscalls Implementadas

| Número | Nombre | Descripción |
|--------|--------|-------------|
| 0 | `read` | Lee de un descriptor de archivo |
| 1 | `write` | Escribe en un descriptor de archivo |
| 2 | `open` | Abre o crea un archivo |
| 3 | `close` | Cierra un descriptor de archivo |
| 9 | `mmap` | Mapea archivos o dispositivos en memoria |
| 12 | `brk` | Cambia el tamaño del segmento de datos |
| 39 | `getpid` | Obtiene el ID del proceso actual |
| 57 | `fork` | Crea un proceso hijo |
| 59 | `execve` | Ejecuta un programa |
| 60 | `exit` | Termina el proceso actual |

*(Nota: Esta es una lista parcial. Ver `include/syscall.h` para la lista completa)*

## Uso desde Espacio de Usuario

En C, utilizando nuestra Mini-LibC:

```c
#include <unistd.h>

int main() {
    write(1, "Hola éterOS\n", 13);
    return 0;
}
```

Directamente en ensamblador x86_64:

```nasm
mov rax, 1      ; sys_write
mov rdi, 1      ; stdout
mov rsi, msg    ; buffer
mov rdx, 13     ; longitud
syscall
```

## Referencias
- `include/syscall.h`
- `kernel/arch/x86_64/syscall.c`
- `kernel/arch/x86_64/syscall_entry.asm`
