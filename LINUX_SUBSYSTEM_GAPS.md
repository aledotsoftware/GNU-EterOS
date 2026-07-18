# Aether Linux Subsystem - Gap Analysis & Technical Roadmap

## 1. Introducción
El subsistema Aether-Linux busca proveer una capa de compatibilidad ABI de sistema x86_64 sobre EterOS, permitiendo la ejecución de binarios GNU/Linux inalterados (como `musl`, `BusyBox`, `coreutils` y `bash`). Este documento detalla el estado actual, evalúa las capacidades implementadas frente a los estándares de Linux y define los gaps concretos que restan para alcanzar la visión de compatibilidad amplia.

## 2. Estado Actual y Componentes Verificados

- **Detección ELF y Rutas ABI (`kernel/fs/elf.c`)**: Se ha implementado el parseo de encabezados ELF. Los binarios se identifican y cargan en sus direcciones base (`PT_LOAD`). Actualmente asume compilación estática pero logra reconocer la necesidad de memoria `mmap_base` compartida para hijos y pre-carga la tabla fd (`current->fd_table`).
- **Thread Local Storage (TLS) y `arch_prctl` (`kernel/arch/x86_64/syscall.c`)**: Bajo la variante II del System V ABI para x86_64, el TLS se asigna detrás del Thread Control Block (TCB). Lasyscall `arch_prctl` (códigos `ARCH_SET_FS` y `ARCH_SET_GS`) configura correctamente `fs_base` y los MSR subyacentes (`wrmsr(MSR_FS_BASE, addr)`). EterOS inicializa un TCB mínimo incluso cuando `PT_TLS` no existe, previniendo fallos en accesos de librerías POSIX por defecto.
- **Señales (`kernel/arch/x86_64/syscall_entry.asm` & `sys_rt_sigaction`)**: El manejo de interrupciones difiere entre señales nativas y POSIX completas. `setup_sigcontext` fuerza el alineamiento `16n + 8` del stack al inyectar el handler en modo usuario, restaurando transparentemente la `user_stack_scratch` del procesador. El `restorer` gestiona correctamente la liberación con `sys_rt_sigreturn`.
- **Mapeo de Memoria (`sys_mmap`, `sys_mprotect`, `sys_mremap`)**: `mmap` y `munmap` manejan la base del proceso correctamente. `sys_mprotect` obedece el estándar POSIX retornando `-ENOMEM` para zonas no mapeadas. Se ha endurecido el flag `MAP_ANONYMOUS` (permitido implícitamente bajo la máscara ELFOSABI_LINUX). `sys_mremap` preserva el Copy-On-Write (COW).
- **VFS ABI (`sys_readlink`, `sys_openat`)**: `sys_readlinkat` sigue rigurosamente el comportamiento de Linux (retornando la longitud real sin byte nulo implícito). `sys_openat` gestiona correctamente paths en el sistema de archivos VFS (initrd, FAT32) y aplica las restricciones del `mask` subyacente impidiendo que `O_EXCL | O_CREAT` sobreescriba.

## 3. Gaps Críticos (Blockers Actuales)

A pesar de los avances, la ejecución de entornos GNU completos revela brechas fundamentales en la implementación:

### 3.1. Cargador de Librerías Dinámicas (`.so` / `PT_DYNAMIC`)
- **Gap:** El cargador actual en `elf.c` ignora `PT_DYNAMIC` e `PT_INTERP`. Binarios precompilados dinámicamente como `bash` intentan acceder a `/lib/ld-linux-x86-64.so.2`.
- **Acción:** `elf.c` debe parsear el string en `PT_INTERP`, cargar ese intérprete como un segundo binario ELF en una base superior y configurar la pila pasándole el control a éste (con el vector auxiliar `AT_ENTRY` apuntando al binario original).

### 3.2. Bifurcación Compleja (`sys_clone` vs `sys_fork`)
- **Gap:** Si bien la syscall `clone3` delega a la implementación mock `task_fork()`, no soporta el control granular de flags que usa la `glibc` (`CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD`).
- **Acción:** Reestructurar `task_t` para almacenar punteros compartidos a tablas de descriptores de archivos, contextos FS, semáforos, e incorporar semántica Thread-Group Leader (TGID / PID) para los hijos multi-hilos de un mismo proceso.

### 3.3. Terminales Virtuales y Pseudoterminales (PTY)
- **Gap:** Los binarios de utilidades de GNU (`coreutils` o `nano`) realizan ioctls exhaustivas tipo PTY (`TCGETS`, `TIOCGWINSZ`). En EterOS, la salida estándar (`fd 1`) mapea directamente a primitivas TTY o gráficos sin abstraer la terminal adecuadamente.
- **Acción:** Implementar un dispositivo `/dev/ptmx` que genere pares `pty` maestros y esclavos. Añadir manejo de las `ioctl` necesarias para que los binarios GNU crean que interactúan con un entorno estándar POSIX.

### 3.4. Stubs y Falsos Positivos en la Tabla de Syscalls
- **Gap:** Existen docenas de syscalls stub (ej. `sys_msync`, `sys_mincore`, IPC SysV `sys_shmget`) en `syscall_linux_table` que devuelven directamente `0` o `-ENOSYS`.
- **Acción:** A medida que se exija funcionalidad (como las bases de datos en memoria exigiendo `msync`), reemplazar stubs por lógica respaldada por el Virtual Memory Manager o PMM.

### 3.5. Binder y Fusión Android
- **Gap:** Android depende de `/dev/binder` y RPC en tiempo real.
- **Acción:** Expandir el subsistema hacia IPC Android; `sys_mmap` y `sys_ioctl` deberán delegar operaciones complejas de BR_DEAD_REPLY hacia drivers VFS que residan en el Ring 0.

## 4. Próximos Entregables (Roadmap Inmediato)
1. **Fase 1 (Semanas 1-2): Soporte ELF Dinámico (`PT_INTERP`)**. Permitirá ejecutar binarios dinámicos mediante `ld.so`.
2. **Fase 2 (Semanas 3-4): Compatibilidad de IOCTLs TTY/PTY**. Probar binarios ncurses como `htop` o `nano` compilados dinámicamente.
3. **Fase 3 (Semanas 5-8): Full `clone` Semantics**. Soporte multi-hilos real vía `pthreads` para cargas Glibc sin crasheos.
