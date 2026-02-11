# =============================================================================
# éterOS - Build Script para Windows (PowerShell)
# =============================================================================
# Alternativa al Makefile para compilar nativamente en Windows.
#
# Uso:
#   .\build.ps1              # Compilar todo
#   .\build.ps1 -Target run  # Compilar y ejecutar en QEMU
#   .\build.ps1 -Target clean
#   .\build.ps1 -Target debug
#   .\build.ps1 -Target info
#
# Prerequisitos:
#   - nasm (en PATH)
#   - x86_64-elf-gcc, x86_64-elf-ld, x86_64-elf-objcopy (en PATH)
#   - qemu-system-x86_64 (en PATH)
#
# Instalación recomendada (Scoop):
#   scoop install nasm qemu make
#   Para el cross-compiler: descargar de
#   https://github.com/lordmilko/i686-elf-tools/releases
# =============================================================================

param(
    [ValidateSet("all", "boot", "kernel", "image", "run", "debug", "clean", "info")]
    [string]$Target = "all"
)

$ErrorActionPreference = "Stop"

# ---- Configuración ----
$AS = "nasm"
$CC = "x86_64-elf-gcc"
$LD = "x86_64-elf-ld"
$OBJCOPY = "x86_64-elf-objcopy"
$QEMU = "qemu-system-x86_64"

$BOOT_DIR = "boot\x86_64"
$KERNEL_DIR = "kernel"
$INCLUDE_DIR = "include"
$BUILD_DIR = "build"

$BOOT_SRC = "$BOOT_DIR\boot.asm"
$BOOT_BIN = "$BUILD_DIR\boot.bin"
$KERNEL_ELF = "$BUILD_DIR\kernel.elf"
$KERNEL_BIN = "$BUILD_DIR\kernel.bin"
$OS_IMAGE = "$BUILD_DIR\eteros.img"

$CFLAGS = @(
    "-ffreestanding",
    "-fno-exceptions",
    "-fno-stack-protector",
    "-nostdlib",
    "-nostdinc",
    "-mno-red-zone",
    "-Wall",
    "-Wextra",
    "-O2",
    "-I$INCLUDE_DIR"
)

$KERNEL_SRCS = @(
    "$KERNEL_DIR\main.c",
    "$KERNEL_DIR\string.c",
    "$KERNEL_DIR\drivers\video\vga.c",
    "$KERNEL_DIR\drivers\serial\serial.c"
)

# ---- Funciones auxiliares ----

function Write-Step {
    param([string]$Type, [string]$Message)
    switch ($Type) {
        "ASM" { Write-Host "[$Type]   $Message" -ForegroundColor Cyan }
        "CC" { Write-Host "[$Type]    $Message" -ForegroundColor Green }
        "LD" { Write-Host "[$Type]    $Message" -ForegroundColor Yellow }
        "BIN" { Write-Host "[$Type]   $Message" -ForegroundColor Magenta }
        "IMG" { Write-Host "[$Type]   $Message" -ForegroundColor Blue }
        "QEMU" { Write-Host "[$Type]  $Message" -ForegroundColor White }
        "CLEAN" { Write-Host "[$Type] $Message" -ForegroundColor DarkGray }
        "OK" { Write-Host "[$Type]    $Message" -ForegroundColor Green }
        "ERR" { Write-Host "[$Type]   $Message" -ForegroundColor Red }
        default { Write-Host "[$Type]  $Message" }
    }
}

function Initialize-BuildDirs {
    $dirs = @(
        $BUILD_DIR,
        "$BUILD_DIR\$KERNEL_DIR",
        "$BUILD_DIR\$KERNEL_DIR\drivers\video",
        "$BUILD_DIR\$KERNEL_DIR\drivers\serial"
    )
    foreach ($d in $dirs) {
        if (!(Test-Path $d)) {
            New-Item -ItemType Directory -Path $d -Force | Out-Null
        }
    }
}

function Invoke-BootBuild {
    Write-Step "ASM" $BOOT_SRC
    & $AS -f bin $BOOT_SRC -o $BOOT_BIN
    if ($LASTEXITCODE -ne 0) {
        Write-Step "ERR" "Fallo al ensamblar el bootloader"
        exit 1
    }
}

function Invoke-KernelBuild {
    $objFiles = @()

    foreach ($src in $KERNEL_SRCS) {
        $obj = "$BUILD_DIR\$($src -replace '\.c$', '.o')"
        $objFiles += $obj

        Write-Step "CC" $src
        & $CC @CFLAGS -c $src -o $obj
        if ($LASTEXITCODE -ne 0) {
            Write-Step "ERR" "Fallo al compilar $src"
            exit 1
        }
    }

    Write-Step "LD" "Enlazando kernel..."
    & $LD -T "$BOOT_DIR\linker.ld" -nostdlib -o $KERNEL_ELF @objFiles
    if ($LASTEXITCODE -ne 0) {
        Write-Step "ERR" "Fallo al enlazar el kernel"
        exit 1
    }

    Write-Step "BIN" "Extrayendo binario del kernel..."
    & $OBJCOPY -O binary $KERNEL_ELF $KERNEL_BIN
    if ($LASTEXITCODE -ne 0) {
        Write-Step "ERR" "Fallo al extraer binario"
        exit 1
    }

    $size = (Get-Item $KERNEL_BIN).Length
    Write-Step "OK" "Kernel: $size bytes"
}

function Invoke-ImageBuild {
    Write-Step "IMG" "Generando imagen de disco..."

    $imagePath = Join-Path (Get-Location) $OS_IMAGE

    # Crear imagen vacía de 1.44 MB (2880 sectores de 512 bytes)
    $imageSize = 2880 * 512  # 1,474,560 bytes
    $imageData = New-Object byte[] $imageSize

    # Leer el bootloader y copiarlo al inicio
    $bootPath = Join-Path (Get-Location) $BOOT_BIN
    $bootData = [System.IO.File]::ReadAllBytes($bootPath)
    [System.Array]::Copy($bootData, 0, $imageData, 0, $bootData.Length)

    # Leer el kernel y copiarlo a partir del sector 9 (offset 4608)
    $kernelPath = Join-Path (Get-Location) $KERNEL_BIN
    $kernelData = [System.IO.File]::ReadAllBytes($kernelPath)
    $kernelOffset = 9 * 512
    [System.Array]::Copy($kernelData, 0, $imageData, $kernelOffset, $kernelData.Length)

    # Escribir imagen final
    [System.IO.File]::WriteAllBytes($imagePath, $imageData)

    $totalSize = (Get-Item $imagePath).Length
    Write-Step "OK" "Imagen: $imagePath ($totalSize bytes)"
}

function Invoke-QemuRun {
    param([bool]$DebugMode = $false)

    Write-Step "QEMU" "Iniciando eterOS..."

    $qemuArgs = @(
        "-drive", "format=raw,file=$OS_IMAGE,if=floppy",
        "-serial", "stdio",
        "-m", "128M",
        "-no-reboot",
        "-no-shutdown"
    )

    if ($DebugMode) {
        $qemuArgs += @("-s", "-S")
        Write-Host ""
        Write-Host "  Modo depuracion activado." -ForegroundColor Yellow
        Write-Host "  Conectar con: gdb -ex 'target remote localhost:1234'" -ForegroundColor Gray
        Write-Host ""
    }

    & $QEMU @qemuArgs
}

function Invoke-BuildClean {
    Write-Step "CLEAN" "Limpiando archivos generados..."
    if (Test-Path $BUILD_DIR) {
        Remove-Item -Recurse -Force $BUILD_DIR
    }
    Write-Step "CLEAN" "Limpieza completada."
}

function Show-BuildInfo {
    Write-Host ""
    Write-Host "=== eterOS Build System (Windows) ===" -ForegroundColor Cyan
    Write-Host ""

    $tools = @(
        @{Name = "Assembler"; Cmd = $AS },
        @{Name = "Compiler"; Cmd = $CC },
        @{Name = "Linker"; Cmd = $LD },
        @{Name = "ObjCopy"; Cmd = $OBJCOPY },
        @{Name = "Emulator"; Cmd = $QEMU }
    )

    foreach ($tool in $tools) {
        $found = Get-Command $tool.Cmd -ErrorAction SilentlyContinue
        if ($found) {
            Write-Host "  $($tool.Name): $($found.Source)" -ForegroundColor Green
        }
        else {
            Write-Host "  $($tool.Name): $($tool.Cmd) [NO ENCONTRADO]" -ForegroundColor Red
        }
    }

    Write-Host ""
    Write-Host "  Boot source:  $BOOT_SRC"
    Write-Host "  Kernel srcs:  $($KERNEL_SRCS -join ', ')"
    Write-Host "  Output:       $OS_IMAGE"
    Write-Host ""
}

# ---- Banner ----
Write-Host ""
Write-Host "  eterOS Build System" -ForegroundColor Cyan
Write-Host "  Target: $Target" -ForegroundColor DarkGray
Write-Host ""

# ---- Ejecutar target ----
switch ($Target) {
    "all" {
        Initialize-BuildDirs
        Invoke-BootBuild
        Invoke-KernelBuild
        Invoke-ImageBuild
        Write-Host ""
        Write-Host "  ================================================" -ForegroundColor Green
        Write-Host "    eterOS construido exitosamente!" -ForegroundColor Green
        Write-Host "    Imagen: $OS_IMAGE" -ForegroundColor White
        Write-Host "    Ejecutar: .\build.ps1 -Target run" -ForegroundColor White
        Write-Host "  ================================================" -ForegroundColor Green
        Write-Host ""
    }
    "boot" {
        Initialize-BuildDirs
        Invoke-BootBuild
    }
    "kernel" {
        Initialize-BuildDirs
        Invoke-KernelBuild
    }
    "image" {
        Initialize-BuildDirs
        Invoke-BootBuild
        Invoke-KernelBuild
        Invoke-ImageBuild
    }
    "run" {
        Initialize-BuildDirs
        Invoke-BootBuild
        Invoke-KernelBuild
        Invoke-ImageBuild
        Invoke-QemuRun
    }
    "debug" {
        Initialize-BuildDirs
        Invoke-BootBuild
        Invoke-KernelBuild
        Invoke-ImageBuild
        Invoke-QemuRun -DebugMode $true
    }
    "clean" {
        Invoke-BuildClean
    }
    "info" {
        Show-BuildInfo
    }
}
