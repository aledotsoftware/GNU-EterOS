# éterOS - Instrucciones para Jules Agent

## Contexto del Proyecto
éterOS es un sistema operativo bare-metal para x86_64 desarrollado desde cero.
El objetivo es un OS de "nueva era" con kernel híbrido, <10MB de tamaño total,
capaz de ejecutar software POSIX (como Apache) sobre hardware real.

## Entorno de Compilación
Este proyecto se compila en **Linux x86_64**. Herramientas necesarias:

```bash
# Instalar dependencias (Ubuntu/Debian)
sudo apt update
sudo apt install -y build-essential nasm qemu-system-x86 xorriso mtools gdb

# En Ubuntu x86_64, gcc nativo funciona como cross-compiler.
# Crear wrappers para compatibilidad con el Makefile:
sudo ln -sf /usr/bin/gcc /usr/local/bin/x86_64-elf-gcc
sudo ln -sf /usr/bin/ld /usr/local/bin/x86_64-elf-ld
sudo ln -sf /usr/bin/objcopy /usr/local/bin/x86_64-elf-objcopy
```

## Comandos de Build

```bash
# Compilar todo (bootloader + kernel + imagen de disco)
make all

# Ejecutar en QEMU para verificar
make run

# Solo compilar y generar imagen sin ejecutar
make image

# Limpiar artefactos
make clean
```

## Flujo de Trabajo con Git

### Después de cada cambio funcional:
1. Compilar: `make all`
2. Verificar en QEMU: `make run` (verificar que arranca correctamente)
3. Commit del código fuente
4. **Incluir los binarios generados** en el commit:
   - `build/eteros.img` (imagen de disco)
   - `build/eteros.iso` (si se genera)
5. Push a GitHub

### Para crear un Release:
```bash
git tag v0.1.0
git push origin v0.1.0
```
Esto dispara el workflow de GitHub Actions que crea un Release con archivos descargables.

## Estructura del Proyecto

```
boot/x86_64/boot.asm    → Bootloader de 2 etapas (asm, binario plano)
boot/x86_64/linker.ld   → Linker script (kernel en 0x10000)
kernel/main.c            → Punto de entrada kmain()
kernel/string.c          → Utilidades de cadena/memoria
kernel/drivers/video/     → Driver VGA
kernel/drivers/serial/    → Driver UART COM1
include/                  → Headers del sistema
```

## Layout de Memoria

```
0x00000 - 0x07BFF  : Reservado (IVT, BDA)
0x07C00 - 0x07DFF  : Stage 1 (MBR, 512 bytes)
0x07E00 - 0x09DFF  : Stage 2 (bootloader extendido)
0x10000 - 0x1FFFF  : Kernel de éterOS
0x70000 - 0x72FFF  : Tablas de paginación (12 KB)
0x90000            : Tope del Stack
0xB8000 - 0xBFFFF  : Buffer de video VGA
```

## Flags de Compilación Importantes

- `-ffreestanding`: Sin dependencias de la librería estándar de C
- `-nostdlib -nostdinc`: Sin linking con libc del host
- `-mno-red-zone`: Obligatorio para código de kernel x86_64
- `-O2 -Os`: Optimizar por rendimiento y tamaño (<10MB objetivo)
- `-Wall -Wextra`: Máximas advertencias activadas

## Restricción de Tamaño
El sistema operativo completo (kernel + drivers + apps + assets) debe pesar
**menos de 10 MB**. Verificar siempre con:
```bash
du -sh build/
```

## Roadmap de Desarrollo
Ver README.md para la hoja de ruta completa organizada en fases.

## Testing
Después de cada cambio, ejecutar QEMU para verificar:
```bash
# Test rápido (sin GUI, solo serial output)
qemu-system-x86_64 \
  -drive format=raw,file=build/eteros.img,if=floppy \
  -serial stdio \
  -display none \
  -m 128M \
  -no-reboot
```
