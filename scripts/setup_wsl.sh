#!/bin/bash
# =============================================================================
# éterOS - Setup rápido para WSL2 (Ubuntu)
# =============================================================================
# Ejecutar DENTRO de WSL:
#   chmod +x scripts/setup_wsl.sh
#   ./scripts/setup_wsl.sh
# =============================================================================

set -e

echo ""
echo "============================================"
echo "  éterOS - Setup para WSL2 (Ubuntu)"
echo "============================================"
echo ""

# --- Actualizar paquetes ---
echo "[1/4] Actualizando repositorios..."
sudo apt update -y

# --- Instalar herramientas base ---
echo "[2/4] Instalando herramientas de compilación..."
sudo apt install -y \
    build-essential \
    nasm \
    qemu-system-x86 \
    gdb \
    xorriso \
    mtools

# --- Verificar si necesitamos cross-compiler o podemos usar gcc nativo ---
echo "[3/4] Configurando compilador..."
ARCH=$(uname -m)
if [ "$ARCH" = "x86_64" ]; then
    echo "  [INFO] Arquitectura x86_64 detectada."
    echo "  [INFO] Puedes usar 'gcc' directamente (no necesitas cross-compiler)."
    echo "  [INFO] El Makefile usará gcc en modo freestanding."
    
    # Crear enlaces simbólicos para compatibilidad con el Makefile
    if ! command -v x86_64-elf-gcc &> /dev/null; then
        echo "  [INFO] Creando aliases para compatibilidad..."
        
        # Script wrapper para x86_64-elf-gcc
        sudo tee /usr/local/bin/x86_64-elf-gcc > /dev/null << 'WRAPPER'
#!/bin/bash
exec gcc "$@"
WRAPPER
        sudo chmod +x /usr/local/bin/x86_64-elf-gcc

        sudo tee /usr/local/bin/x86_64-elf-ld > /dev/null << 'WRAPPER'
#!/bin/bash
exec ld "$@"
WRAPPER
        sudo chmod +x /usr/local/bin/x86_64-elf-ld

        sudo tee /usr/local/bin/x86_64-elf-objcopy > /dev/null << 'WRAPPER'
#!/bin/bash
exec objcopy "$@"
WRAPPER
        sudo chmod +x /usr/local/bin/x86_64-elf-objcopy

        echo "  [OK] Wrappers creados en /usr/local/bin/"
    fi
else
    echo "  [WARN] Arquitectura $ARCH detectada. Necesitas un cross-compiler."
    echo "         Visita: https://wiki.osdev.org/GCC_Cross-Compiler"
fi

# --- Verificar instalación ---
echo ""
echo "[4/4] Verificando herramientas..."
echo ""

check_tool() {
    if command -v "$1" &> /dev/null; then
        echo "  [OK] $1 -> $(which $1)"
    else
        echo "  [!!] $1 NO encontrado"
    fi
}

check_tool nasm
check_tool gcc
check_tool ld
check_tool objcopy
check_tool x86_64-elf-gcc
check_tool x86_64-elf-ld
check_tool x86_64-elf-objcopy
check_tool qemu-system-x86_64
check_tool gdb
check_tool make

echo ""
echo "============================================"
echo "  Setup completado!"
echo "============================================"
echo ""
echo "  Para compilar y ejecutar éterOS:"
echo ""
echo "    cd /mnt/p/EterOS"
echo "    make all"
echo "    make run"
echo ""
echo "  NOTA: Si QEMU no puede abrir ventana gráfica,"
echo "  instala un servidor X11 en Windows:"
echo "    - VcXsrv: https://vcxsrv.com/"
echo "    - O usa: make run  (con -nographic en QEMU)"
echo ""
