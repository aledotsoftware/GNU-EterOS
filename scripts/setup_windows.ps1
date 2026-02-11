# =============================================================================
# éterOS - Configuración del entorno en Windows
# =============================================================================
# Ejecutar como Administrador:
#   Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
#   .\scripts\setup_windows.ps1
#
# Opciones de entorno:
#   1. WSL2 (Recomendado) - Usa Linux dentro de Windows
#   2. MSYS2 - Entorno Unix-like nativo en Windows
# =============================================================================

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  eterOS - Setup para Windows" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# --- Verificar si Chocolatey está instalado ---
function Install-Choco {
    if (!(Get-Command choco -ErrorAction SilentlyContinue)) {
        Write-Host "[INSTALL] Instalando Chocolatey..." -ForegroundColor Yellow
        Set-ExecutionPolicy Bypass -Scope Process -Force
        [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
        Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
    } else {
        Write-Host "[OK] Chocolatey ya instalado." -ForegroundColor Green
    }
}

# --- Verificar si Scoop está instalado ---
function Install-Scoop {
    if (!(Get-Command scoop -ErrorAction SilentlyContinue)) {
        Write-Host "[INSTALL] Instalando Scoop..." -ForegroundColor Yellow
        Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser -Force
        Invoke-RestMethod -Uri https://get.scoop.sh | Invoke-Expression
    } else {
        Write-Host "[OK] Scoop ya instalado." -ForegroundColor Green
    }
}

Write-Host "Selecciona el metodo de instalacion:" -ForegroundColor White
Write-Host ""
Write-Host "  [1] WSL2 + Ubuntu (RECOMENDADO)" -ForegroundColor Green
Write-Host "      Instala Linux dentro de Windows."
Write-Host "      Todo funciona nativamente con el Makefile."
Write-Host ""
Write-Host "  [2] Herramientas nativas Windows" -ForegroundColor Yellow
Write-Host "      NASM + QEMU + Cross-compiler via Scoop/Choco."
Write-Host "      Usar build.ps1 para compilar."
Write-Host ""

$choice = Read-Host "Opcion (1 o 2)"

switch ($choice) {
    "1" {
        Write-Host ""
        Write-Host "[WSL2] Configurando WSL2 con Ubuntu..." -ForegroundColor Cyan
        Write-Host ""

        # Habilitar WSL
        Write-Host "[1/4] Habilitando WSL..." -ForegroundColor Yellow
        wsl --install -d Ubuntu --no-launch 2>$null

        Write-Host "[2/4] WSL instalado." -ForegroundColor Green
        Write-Host ""
        Write-Host "IMPORTANTE:" -ForegroundColor Red
        Write-Host "  1. Reinicia tu PC si es la primera vez que instalas WSL." -ForegroundColor White
        Write-Host "  2. Despues del reinicio, abre 'Ubuntu' desde el menu inicio." -ForegroundColor White
        Write-Host "  3. Crea un usuario y contrasena." -ForegroundColor White
        Write-Host "  4. Ejecuta los siguientes comandos dentro de WSL:" -ForegroundColor White
        Write-Host ""
        Write-Host "     sudo apt update" -ForegroundColor Gray
        Write-Host "     sudo apt install -y build-essential nasm qemu-system-x86 gdb" -ForegroundColor Gray
        Write-Host ""
        Write-Host "  5. Para el cross-compiler, hay dos opciones:" -ForegroundColor White
        Write-Host ""
        Write-Host "     Opcion A - Usar el compilador del sistema (mas facil):" -ForegroundColor Yellow
        Write-Host "       No se necesita cross-compiler si compilas dentro de WSL" -ForegroundColor Gray
        Write-Host "       con un Linux x86_64. Usa gcc directamente:" -ForegroundColor Gray
        Write-Host "         gcc -ffreestanding -nostdlib ..." -ForegroundColor Gray
        Write-Host ""
        Write-Host "     Opcion B - Compilar x86_64-elf-gcc desde codigo fuente:" -ForegroundColor Yellow
        Write-Host "       Visita: https://wiki.osdev.org/GCC_Cross-Compiler" -ForegroundColor Gray
        Write-Host ""
        Write-Host "  6. Accede a tus archivos de Windows desde WSL:" -ForegroundColor White
        Write-Host "     cd /mnt/p/EterOS" -ForegroundColor Gray
        Write-Host "     make run" -ForegroundColor Gray
        Write-Host ""
    }

    "2" {
        Write-Host ""
        Write-Host "[NATIVE] Instalando herramientas nativas..." -ForegroundColor Cyan
        Write-Host ""

        # Instalar Scoop si no está disponible
        Install-Scoop

        # Agregar buckets necesarios
        Write-Host "[1/5] Configurando repositorios de Scoop..." -ForegroundColor Yellow
        scoop bucket add extras 2>$null
        scoop bucket add main 2>$null

        # Instalar NASM
        Write-Host "[2/5] Instalando NASM..." -ForegroundColor Yellow
        scoop install nasm 2>$null

        # Instalar QEMU
        Write-Host "[3/5] Instalando QEMU..." -ForegroundColor Yellow
        scoop install qemu 2>$null

        # Instalar Make (GNU Make para Windows)
        Write-Host "[4/5] Instalando GNU Make..." -ForegroundColor Yellow
        scoop install make 2>$null

        # Cross-compiler
        Write-Host "[5/5] Cross-compiler..." -ForegroundColor Yellow
        Write-Host ""
        Write-Host "  El cross-compiler x86_64-elf-gcc no esta disponible" -ForegroundColor White
        Write-Host "  directamente en Scoop. Opciones:" -ForegroundColor White
        Write-Host ""
        Write-Host "  Opcion A - Descargar binario pre-compilado:" -ForegroundColor Yellow
        Write-Host "    https://github.com/lordmilko/i686-elf-tools/releases" -ForegroundColor Gray
        Write-Host "    Descarga x86_64-elf-tools-windows.zip" -ForegroundColor Gray
        Write-Host "    Extrae en C:\x86_64-elf-tools\ y agrega al PATH" -ForegroundColor Gray
        Write-Host ""
        Write-Host "  Opcion B - Compilar desde fuente en MSYS2:" -ForegroundColor Yellow
        Write-Host "    https://wiki.osdev.org/GCC_Cross-Compiler" -ForegroundColor Gray
        Write-Host ""
    }

    default {
        Write-Host "Opcion no valida." -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Verificando herramientas instaladas..." -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

$tools = @("nasm", "qemu-system-x86_64", "make", "x86_64-elf-gcc", "gcc", "wsl")
foreach ($tool in $tools) {
    $cmd = Get-Command $tool -ErrorAction SilentlyContinue
    if ($cmd) {
        Write-Host "  [OK] $tool -> $($cmd.Source)" -ForegroundColor Green
    } else {
        Write-Host "  [--] $tool no encontrado" -ForegroundColor DarkGray
    }
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Setup completado!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
