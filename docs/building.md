# Guía de Compilación de éterOS

Esta guía explica cómo configurar tu entorno y compilar éterOS desde el código fuente.

## Dependencias Requeridas

### Linux (Debian/Ubuntu/WSL)
```bash
sudo apt update
sudo apt install gcc-x86-64-elf binutils-x86-64-elf nasm make qemu-system-x86 python3 xorriso
```

### Windows (Nativo)
Se recomienda el uso de **PowerShell** y el script `build.ps1`. Debes tener instalados:
- `nasm` en el PATH.
- `qemu-system-x86_64` en el PATH.
- `x86_64-elf-gcc` toolchain.

## Proceso de Compilación

### Usando Make (Recomendado para Linux/WSL)
```bash
make all      # Compila boot, kernel y genera la imagen de disco
make run      # Compila y ejecuta en QEMU
make clean    # Borra los artefactos de compilación
```

### Usando build.ps1 (Windows)
```powershell
.\build.ps1 -Target all
.\build.ps1 -Target run
```

## Opciones de Configuración

Puedes pasar variables al `make` para cambiar el comportamiento del build:
- `ARCH=x86_64`: (Por defecto) Compila para PCs modernos.
- `DEBUG=1`: Habilita símbolos de depuración y logs extra.

## Troubleshooting Común

### "x86_64-elf-gcc: command not found"
Asegúrate de tener instalado el cross-compiler. En Ubuntu puedes usar el paquete `gcc-x86-64-elf` o descargar un toolchain precompilado.

### "nasm: command not found"
Instala el ensamblador NASM: `sudo apt install nasm`.

### Error al generar la ISO
Asegúrate de tener instalado `xorriso` para que el script `make iso` funcione correctamente.

## Referencias
- `Makefile`
- `build.ps1`
