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
    [ValidateSet("all", "boot", "kernel", "image", "vdi", "vbox", "run", "run-nographic", "debug", "clean", "info", "pxe", "usb", "iso")]
    [string]$Target = "all",

    [ValidateSet("x86_64", "i386", "aarch64")]
    [string]$Arch = "x86_64",

    [string]$Changelog = "Auto-compilacion sin detalles"
)

$ErrorActionPreference = "Stop"

# ---- Auto-detección de herramientas ----
# Busca herramientas en PATH y en rutas de instalación comunes
function Find-Tool {
    param([string]$Name, [string[]]$SearchPaths)

    # Primero intentar en PATH
    $found = Get-Command $Name -ErrorAction SilentlyContinue
    if ($found) { return $found.Source }

    # Buscar en rutas comunes
    foreach ($p in $SearchPaths) {
        $full = Join-Path $p $Name
        if (Test-Path $full) { return $full }
        if (Test-Path "$full.exe") { return "$full.exe" }
    }

    return $null
}

$nasmSearchPaths = @(
    "$env:LOCALAPPDATA\bin\NASM",
    "C:\Program Files\NASM",
    "C:\Program Files (x86)\NASM",
    "C:\nasm"
)

$crossSearchPaths = @(
    "C:\x86_64-elf-tools\bin",
    "$env:LOCALAPPDATA\x86_64-elf-tools\bin",
    "C:\cross\bin"
)

$cross32SearchPaths = @(
    "C:\i686-elf-tools\bin",
    "$env:LOCALAPPDATA\i686-elf-tools\bin",
    "C:\cross32\bin"
)

$qemuSearchPaths = @(
    "C:\Program Files\qemu",
    "C:\Program Files (x86)\qemu",
    "$env:LOCALAPPDATA\Programs\qemu"
)

$aarch64SearchPaths = @(
    "C:\aarch64-elf-tools\bin",
    "$env:LOCALAPPDATA\aarch64-elf-tools\bin",
    "C:\cross-aarch64\bin",
    "C:\msys64\opt\aarch64-elf\bin",
    "C:\Program Files (x86)\Arm GNU Toolchain\15.2 aarch64-none-elf\bin",
    "C:\Program Files\Arm GNU Toolchain\15.2 aarch64-none-elf\bin"
)

$AS = Find-Tool "nasm"                $nasmSearchPaths
$QEMU = Find-Tool "qemu-system-$Arch"  $qemuSearchPaths

# VBoxManage (opcional — para generar VDI)
$vboxSearchPaths = @(
    "C:\Program Files\Oracle\VirtualBox",
    "C:\Program Files (x86)\Oracle\VirtualBox",
    "$env:LOCALAPPDATA\Programs\Oracle\VirtualBox"
)
$VBOXMANAGE = Find-Tool "VBoxManage" $vboxSearchPaths

$mkisofsSearchPaths = @(
    "C:\Program Files\cdrtools",
    "C:\Program Files (x86)\cdrtools",
    "C:\Program Files\Git\usr\bin",
    "C:\Program Files (x86)\Git\usr\bin",
    "C:\msys64\usr\bin"
)
$MKISOFS = Find-Tool "mkisofs" $mkisofsSearchPaths

if ($Arch -eq "x86_64") {
    $CC = Find-Tool "x86_64-elf-gcc" $crossSearchPaths
    $LD = Find-Tool "x86_64-elf-ld" $crossSearchPaths
    $OBJCOPY = Find-Tool "x86_64-elf-objcopy" $crossSearchPaths
    $BOOT_DIR = "boot\x86_64"
    $CARCHFLAGS = @("-m64", "-mcmodel=large", "-mno-red-zone", "-mno-sse", "-mno-sse2", "-mno-mmx", "-D__x86_64__", "-DARCH_X86_64")
    $LDFLAGS_ARCH = @("-m", "elf_x86_64")
    $VBOX_OSTYPE = "Other_64"
}
elseif ($Arch -eq "aarch64") {
    $CC = Find-Tool "aarch64-elf-gcc" $aarch64SearchPaths
    if (!$CC) { $CC = Find-Tool "aarch64-none-elf-gcc" $aarch64SearchPaths }

    $LD = Find-Tool "aarch64-elf-ld" $aarch64SearchPaths
    if (!$LD) { $LD = Find-Tool "aarch64-none-elf-ld" $aarch64SearchPaths }

    $OBJCOPY = Find-Tool "aarch64-elf-objcopy" $aarch64SearchPaths
    if (!$OBJCOPY) { $OBJCOPY = Find-Tool "aarch64-none-elf-objcopy" $aarch64SearchPaths }

    # For ARM, we use the GCC preprocessor/assembler instead of NASM
    $BOOT_DIR = "boot\aarch64"
    $CARCHFLAGS = @("-march=armv8-a", "-ffreestanding", "-nostdlib", "-D__aarch64__", "-DARCH_AARCH64")
    $LDFLAGS_ARCH = @("-m", "aarch64elf")
    $VBOX_OSTYPE = "Oracle_Arm64" 
}
else {
    # Para i386 usamos el compilador de 32 bits si existe, o el de 64 bits con flag -m32
    $CC = Find-Tool "i686-elf-gcc" $cross32SearchPaths
    if (!$CC) { 
        $CC = Find-Tool "x86_64-elf-gcc" $crossSearchPaths 
        $CARCHFLAGS = @("-m32", "-mno-sse", "-mno-sse2", "-mno-mmx")
    }
    else {
        $CARCHFLAGS = @("-mno-sse", "-mno-sse2", "-mno-mmx")
    }

    $LD = Find-Tool "i686-elf-ld" $cross32SearchPaths
    if (!$LD) {
        $LD = Find-Tool "x86_64-elf-ld" $crossSearchPaths
        $LDFLAGS_ARCH = @("-m", "elf_i386")
    }
    else {
        $LDFLAGS_ARCH = @()
    }
    
    $OBJCOPY = Find-Tool "i686-elf-objcopy" $cross32SearchPaths
    if (!$OBJCOPY) { $OBJCOPY = Find-Tool "x86_64-elf-objcopy" $crossSearchPaths }

    $BOOT_DIR = "boot\i386"
    $VBOX_OSTYPE = "Other"
}

# Verificar herramientas críticas
$missing = @()
if (!$AS) { $missing += "nasm" }
if (!$CC) { $missing += "$Arch-elf-gcc" }
if (!$LD) { $missing += "$Arch-elf-ld" }
if (!$OBJCOPY) { $missing += "$Arch-elf-objcopy" }
if (!$QEMU) { $missing += "qemu-system-$Arch" }

if ($missing.Count -gt 0 -and $Target -notin @("clean", "info")) {
    Write-Host "  [ERROR] Herramientas no encontradas:" -ForegroundColor Red
    foreach ($m in $missing) {
        Write-Host "    - $m" -ForegroundColor Red
    }
    Write-Host ""
    Write-Host "  Instalar con:" -ForegroundColor Yellow
    Write-Host "    winget install NASM.NASM" -ForegroundColor Gray
    Write-Host "    winget install SoftwareFreedomConservancy.QEMU" -ForegroundColor Gray
    Write-Host "    Cross-compiler: https://github.com/lordmilko/i686-elf-tools/releases" -ForegroundColor Gray
    Write-Host ""
    exit 1
}

$KERNEL_DIR = "kernel"
$INCLUDE_DIR = "include"
$BUILD_DIR = "build\$Arch"

if (!(Test-Path $BUILD_DIR)) { New-Item -ItemType Directory -Force -Path $BUILD_DIR | Out-Null }

if ($Arch -eq "aarch64") {
    $BOOT_SRC = "$BOOT_DIR\boot.S"
}
else {
    $BOOT_SRC = "$BOOT_DIR\boot.asm"
}
$BOOT_BIN = "$BUILD_DIR\boot.bin"
$KERNEL_ELF = "$BUILD_DIR\kernel.elf"
$KERNEL_BIN = "$BUILD_DIR\kernel.bin"
$OS_IMAGE = "$BUILD_DIR\eteros.img"
$OS_VDI = "$BUILD_DIR\eteros.vdi"

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
) + $CARCHFLAGS

# Archivos fuente comunes
$KERNEL_SRCS = @(
    "$KERNEL_DIR\main.c",
    "$KERNEL_DIR\string.c",
    "$KERNEL_DIR\shell\shell.c",
    "$KERNEL_DIR\shell\commands.c",
    "$KERNEL_DIR\shell\history.c",
    "$KERNEL_DIR\shell\cmd_system.c",
    "$KERNEL_DIR\shell\cmd_task.c",
    "$KERNEL_DIR\shell\cmd_net.c",
    "$KERNEL_DIR\shell\cmd_misc.c",
    "$KERNEL_DIR\shell\cmd_ota.c",
    "$KERNEL_DIR\shell\cmd_devices.c",
    "$KERNEL_DIR\shell\cmd_time.c",
    "$KERNEL_DIR\shell\cmd_user.c",
    "$KERNEL_DIR\shell\cmd_panel.c",
    "$KERNEL_DIR\drivers\video\vga.c",
    "$KERNEL_DIR\drivers\video\framebuffer.c",
    "$KERNEL_DIR\drivers\video\font.c",
    "$KERNEL_DIR\drivers\serial\serial.c",
    "$KERNEL_DIR\drivers\input\keyboard.c",
    "$KERNEL_DIR\drivers\input\mouse.c",
    "$KERNEL_DIR\drivers\timer\pit.c",
    "$KERNEL_DIR\drivers\rtc\rtc.c",
    "$KERNEL_DIR\drivers\pci\pci.c",
    "$KERNEL_DIR\drivers\net\e1000.c",
    "$KERNEL_DIR\net\core\nic.c",
    "$KERNEL_DIR\net\core\dhcp.c",
    "$KERNEL_DIR\net\core\dhcp_parser.c",
    "$KERNEL_DIR\net\core\stack.c",
    "$KERNEL_DIR\net\core\raw_tcp.c",
    "$KERNEL_DIR\net\core\tcp.c",
    "$KERNEL_DIR\net\core\ip_utils.c",
    "$KERNEL_DIR\net\core\lwip_stubs.c",
    "$KERNEL_DIR\mm\heap.c",
    "$KERNEL_DIR\mm\pmm.c",
    "$KERNEL_DIR\mm\vmm.c",
    "$KERNEL_DIR\libgcc.c",
    "$KERNEL_DIR\apps\santitravel.c",
    "$KERNEL_DIR\apps\sysmon.c",
    "$KERNEL_DIR\apps\gui_demo.c",
    "$KERNEL_DIR\apps\user_loader.c",
    "$KERNEL_DIR\apps\wget.c",
    "$KERNEL_DIR\task.c",
    "$KERNEL_DIR\sem.c",
    "$KERNEL_DIR\futex.c",
    "$KERNEL_DIR\klog.c",
    "$KERNEL_DIR\stdio.c",
    "$KERNEL_DIR\fs\initrd.c",
    "$KERNEL_DIR\fs\vfs.c",
    "$KERNEL_DIR\fs\devfs.c",
    "$KERNEL_DIR\fs\procfs.c",
    "$KERNEL_DIR\fs\bcache.c",
    "$KERNEL_DIR\fs\jfs.c",
    "$KERNEL_DIR\fs\shmfs.c",
    "$KERNEL_DIR\drivers\disk\partition.c",
    "$KERNEL_DIR\drivers\disk\ata.c",
    "$KERNEL_DIR\drivers\input\input.c",
    "$KERNEL_DIR\drivers\tty.c",
    "$KERNEL_DIR\gfx\gfx.c",
    "$KERNEL_DIR\gfx\window.c",
    "$KERNEL_DIR\fs\elf.c",
    "$KERNEL_DIR\crypto\sha256.c",
    "$KERNEL_DIR\crypto\sha512.c",
    "$KERNEL_DIR\crypto\ed25519.c"
)

# Archivos específicos de arquitectura
if ($Arch -eq "x86_64") {
    $KERNEL_SRCS += "$KERNEL_DIR\arch\x86_64\idt.c"
    $KERNEL_SRCS += "$KERNEL_DIR\arch\x86_64\pic.c"
    $KERNEL_SRCS += "$KERNEL_DIR\arch\x86_64\gdt.c"
    $KERNEL_SRCS += "$KERNEL_DIR\arch\x86_64\hal_impl.c"
    $KERNEL_SRCS += "$KERNEL_DIR\arch\x86_64\syscall.c"
    $KERNEL_SRCS += "$KERNEL_DIR\arch\x86_64\acpi.c"
    $KERNEL_SRCS += "$KERNEL_DIR\arch\x86_64\smp.c"
    $KERNEL_SRCS += "$KERNEL_DIR\arch\x86_64\apic.c"
    $KERNEL_SRCS += "$KERNEL_DIR\arch\x86_64\pat.c"
    $KERNEL_SRCS += "$KERNEL_DIR\arch\x86_64\boot\nvram.c"
}
elseif ($Arch -eq "aarch64") {
    $KERNEL_SRCS += "$KERNEL_DIR\arch\aarch64\hal_impl.c"
}
else {
    $KERNEL_SRCS += "$KERNEL_DIR\arch\i386\idt.c"
    $KERNEL_SRCS += "$KERNEL_DIR\arch\i386\pic.c"
}

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
        "$BUILD_DIR\$KERNEL_DIR\drivers\serial",
        "$BUILD_DIR\$KERNEL_DIR\drivers\input",
        "$BUILD_DIR\$KERNEL_DIR\arch\$Arch",
        "$BUILD_DIR\$KERNEL_DIR\drivers\timer",
        "$BUILD_DIR\$KERNEL_DIR\drivers\rtc",
        "$BUILD_DIR\$KERNEL_DIR\drivers\pci",
        "$BUILD_DIR\$KERNEL_DIR\drivers\net",
        "$BUILD_DIR\$KERNEL_DIR\drivers\disk",
        "$BUILD_DIR\$KERNEL_DIR\net",
        "$BUILD_DIR\$KERNEL_DIR\net\core",
        "$BUILD_DIR\$KERNEL_DIR\mm",
        "$BUILD_DIR\$KERNEL_DIR\apps",
        "$BUILD_DIR\$KERNEL_DIR\fs",
        "$BUILD_DIR\$KERNEL_DIR\shell",
        "$BUILD_DIR\$KERNEL_DIR\ui",
        "$BUILD_DIR\$KERNEL_DIR\gfx",
        "$BUILD_DIR\$KERNEL_DIR\drivers",
        "$BUILD_DIR\$KERNEL_DIR\arch\$Arch\boot",
        "$BUILD_DIR\$KERNEL_DIR\crypto"
    )
    foreach ($d in $dirs) {
        if (!(Test-Path $d)) {
            New-Item -ItemType Directory -Path $d -Force | Out-Null
        }
    }
}

function Get-SectorCount {
    param([long]$SizeBytes)

    if ($SizeBytes -le 0) { return 0 }
    return [int][Math]::Ceiling($SizeBytes / 512.0)
}

function Get-KernelSectorCount {
    $kernelPath = Join-Path (Get-Location) $KERNEL_BIN
    if (!(Test-Path $kernelPath)) { return 0 }
    return Get-SectorCount ((Get-Item $kernelPath).Length)
}

function Get-InitrdSectorCount {
    $initrdPath = Join-Path (Get-Location) "$BUILD_DIR\initrd.bin"
    if (!(Test-Path $initrdPath)) { return 0 }
    return Get-SectorCount ((Get-Item $initrdPath).Length)
}

function Get-BootAsmDefines {
    if ($Arch -ne "x86_64") {
        return @()
    }

    $defines = @()
    $kernelPath = Join-Path (Get-Location) $KERNEL_BIN
    if (Test-Path $kernelPath) {
        $kernelSize = (Get-Item $kernelPath).Length
        $kernelSectors = Get-SectorCount $kernelSize
        $defines += "-dKERNEL_SECTORS=$kernelSectors"
    }

    $initrdPath = Join-Path (Get-Location) "$BUILD_DIR\initrd.bin"
    if (Test-Path $initrdPath) {
        $initrdSize = (Get-Item $initrdPath).Length
        $initrdSectors = Get-SectorCount $initrdSize
        $defines += "-dINITRD_SECTORS=$initrdSectors"
        $defines += "-dINITRD_SIZE_BYTES=$initrdSize"
    }

    return $defines
}

function Invoke-BootBuild {
    Write-Step "ASM" $BOOT_SRC
    if ($Arch -eq "aarch64") {
        # Para ARM usamos GCC/AS para generar un binario crudo o similar
        # Por ahora generamos un objeto para enlazarlo con el kernel
        & $CC $CFLAGS -c $BOOT_SRC -o "$BUILD_DIR\boot.o"
    }
    else {
        $bootDefines = Get-BootAsmDefines
        if ($Target -eq "vbox") {
            $bootDefines += "-dDISABLE_VBE"
        }
        & $AS -f bin @bootDefines $BOOT_SRC -o $BOOT_BIN
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Step "ERR" "Fallo al ensamblar el bootloader"
        exit 1
    }
}

function Invoke-BumpVersion {
    $mainCPath = "$KERNEL_DIR\main.c"
    $content = Get-Content $mainCPath -Raw
    if ($content -match '#define\s+ETEROS_VERSION_PATCH\s+(\d+)') {
        $oldPatch = [int]$matches[1]
        $newPatch = $oldPatch + 1
        $content = $content -replace "(#define\s+ETEROS_VERSION_PATCH\s+)\d+", "`${1}$newPatch"
        Set-Content -Path $mainCPath -Value $content -NoNewline
        Write-Step "VER" "Versión incrementada a Patch $newPatch"
    }
}

function Invoke-ExportUpdate {
    $mainCPath = "$KERNEL_DIR\main.c"
    $content = Get-Content $mainCPath -Raw
    $major = "0"; $minor = "0"; $patch = "0"
    if ($content -match '#define\s+ETEROS_VERSION_MAJOR\s+(\d+)') { $major = $matches[1] }
    if ($content -match '#define\s+ETEROS_VERSION_MINOR\s+(\d+)') { $minor = $matches[1] }
    if ($content -match '#define\s+ETEROS_VERSION_PATCH\s+(\d+)') { $patch = $matches[1] }
    $version = "$major.$minor.$patch"
    
    $updateRoot = "p:\update.tudexnetworks.com"
    $versionDir = "$updateRoot\$version"
    
    if (!(Test-Path $versionDir)) {
        New-Item -ItemType Directory -Force -Path $versionDir | Out-Null
    }
    
    Write-Step "OTA" "Exportando actualización OTA (v$version)..."
    Copy-Item $OS_IMAGE "$versionDir\eteros.img" -Force
    
    $metadataPath = "$updateRoot\metadata.json"
    $history = @()
    if (Test-Path $metadataPath) {
        try {
            $oldMeta = Get-Content $metadataPath -Raw | ConvertFrom-Json
            if ($oldMeta.history) {
                $history = @($oldMeta.history)
            }
            if ($oldMeta.latest_version -and $oldMeta.latest_version -ne $version) {
                $history = @( @{ version = $oldMeta.latest_version; changelog = $oldMeta.changelog } ) + $history
            }
        } catch {}
    }

    $metadata = [ordered]@{
        path = "/$version/eteros.img"
        latest_version = $version
        changelog = $Changelog
        history = $history
    }
    $metadata | ConvertTo-Json -Depth 5 | Set-Content $metadataPath
    Write-Step "OK" "Actualización $version exportada a $updateRoot con historial"
}

function Invoke-KernelBuild {
    Invoke-BumpVersion
    $objFiles = @()

    # Compilar ASM stubs si existen
    $asmFiles = Get-ChildItem "$KERNEL_DIR\arch\$Arch" -Include @("*.asm", "*.S") -Recurse
    
    foreach ($asmFile in $asmFiles) {
        $asmSrc = $asmFile.FullName
        $asmObj = "$BUILD_DIR\$KERNEL_DIR\arch\$Arch\$($asmFile.Name -replace '\.asm$', '.o' -replace '\.S$', '.o')"
        $objFiles += $asmObj

        Write-Step "ASM" $asmFile.Name
        if ($Arch -eq "aarch64") {
            # Use GCC as assembler for ARM (.S files)
            & $CC $CFLAGS -c $asmSrc -o $asmObj
        }
        else {
            $asmFormat = if ($Arch -eq "x86_64") { "elf64" } else { "elf32" }
            & $AS -f $asmFormat $asmSrc -o $asmObj
        }
        
        if ($LASTEXITCODE -ne 0) {
            Write-Step "ERR" "Fallo al ensamblar $($asmFile.Name)"
            exit 1
        }
    }

    foreach ($src in $KERNEL_SRCS) {
        $obj = "$BUILD_DIR\$($src -replace '\.c$', '.o')"
        $objFiles += $obj

        Write-Step "CC" $src
        $ErrorActionPreference = "Continue"
        & $CC @CFLAGS -c $src -o $obj 2>&1
        $ccExit = $LASTEXITCODE
        $ErrorActionPreference = "Stop"
        if ($ccExit -ne 0) {
            Write-Step "ERR" "Fallo al compilar $src"
            exit 1
        }
    }

    Write-Step "LD" "Enlazando kernel..."
    $ErrorActionPreference = "Continue"
    & $LD @LDFLAGS_ARCH -T "$BOOT_DIR\linker.ld" -nostdlib -o $KERNEL_ELF $objFiles 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) {
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


function Invoke-UserspaceBuild {
    Write-Step "CC" "Compilando Userspace (libc + test.elf)..."

    $userDir = "userspace"
    $libcSrc = "$userDir\libc\src"
    $libcObjDir = "$BUILD_DIR\userspace\libc"
    $initrdRoot = "initrd_root"
    $gnuBinDir = "$initrdRoot\gnu\bin"
    if (!(Test-Path $libcObjDir)) { New-Item -ItemType Directory -Force -Path $libcObjDir | Out-Null }
    if (!(Test-Path $initrdRoot)) { New-Item -ItemType Directory -Force -Path $initrdRoot | Out-Null }
    if (!(Test-Path $gnuBinDir)) { New-Item -ItemType Directory -Force -Path $gnuBinDir | Out-Null }
    if (Test-Path "$gnuBinDir\busybox") { Remove-Item -Force "$gnuBinDir\busybox" }


    # libc objects
    $libcObjs = @()
    
    # CRT0
    $crt0Src = "$libcSrc\crt0.asm"
    $crt0Obj = "$libcObjDir\crt0.o"
    if ($Arch -eq "x86_64") {
        & $AS -f elf64 $crt0Src -o $crt0Obj
    } else {
        & $AS -f elf32 $crt0Src -o $crt0Obj
    }
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar crt0.asm"; exit 1 }
    $libcObjs += $crt0Obj
    $libcSources = Get-ChildItem "$libcSrc\*.c"
    foreach ($src in $libcSources) {
        $obj = "$libcObjDir\$($src.Name -replace '\.c$', '.o')"
        $libcObjs += $obj
        # Write-Step "CC" "libc/$($src.Name)"
        & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $src.FullName -o $obj
        if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar libc ($($src.Name))"; exit 1 }
    }

    # test.elf
    $testSrc = "$userDir\test.c"
    $testObj = "$BUILD_DIR\userspace\test.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $testSrc -o $testObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar test.c"; exit 1 }

    $testElf = "$initrdRoot\test.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $testElf $testObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar test.elf"; exit 1 }

    # test_syscalls_s1.elf
    $testSyscallsSrc = "$userDir\test_syscalls_s1.c"
    $testSyscallsObj = "$BUILD_DIR\userspace\test_syscalls_s1.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $testSyscallsSrc -o $testSyscallsObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar test_syscalls_s1.c"; exit 1 }

    $testSyscallsElf = "$initrdRoot\test_syscalls_s1.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $testSyscallsElf $testSyscallsObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar test_syscalls_s1.elf"; exit 1 }

    # test_epoll.elf
    $testEpollSrc = "$userDir\test_epoll.c"
    $testEpollObj = "$BUILD_DIR\userspace\test_epoll.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $testEpollSrc -o $testEpollObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar test_epoll.c"; exit 1 }

    $testEpollElf = "$initrdRoot\test_epoll.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $testEpollElf $testEpollObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar test_epoll.elf"; exit 1 }

    # test_sigaltstack.elf
    $testSigAltSrc = "$userDir\test_sigaltstack.c"
    $testSigAltObj = "$BUILD_DIR\userspace\test_sigaltstack.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $testSigAltSrc -o $testSigAltObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar test_sigaltstack.c"; exit 1 }

    $testSigAltElf = "$initrdRoot\test_sigaltstack.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $testSigAltElf $testSigAltObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar test_sigaltstack.elf"; exit 1 }

    # test_signal_posix.elf
    $testSignalPosixSrc = "$userDir\test_signal_posix.c"
    $testSignalPosixObj = "$BUILD_DIR\userspace\test_signal_posix.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $testSignalPosixSrc -o $testSignalPosixObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar test_signal_posix.c"; exit 1 }

    $testSignalPosixElf = "$initrdRoot\test_signal_posix.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $testSignalPosixElf $testSignalPosixObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar test_signal_posix.elf"; exit 1 }

    # test_waitid.elf
    $testWaitidSrc = "$userDir\test_waitid.c"
    $testWaitidObj = "$BUILD_DIR\userspace\test_waitid.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $testWaitidSrc -o $testWaitidObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar test_waitid.c"; exit 1 }

    $testWaitidElf = "$initrdRoot\test_waitid.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $testWaitidElf $testWaitidObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar test_waitid.elf"; exit 1 }

    # test_procfs.elf
    $testProcfsSrc = "$userDir\test_procfs.c"
    $testProcfsObj = "$BUILD_DIR\userspace\test_procfs.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $testProcfsSrc -o $testProcfsObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar test_procfs.c"; exit 1 }

    $testProcfsElf = "$initrdRoot\test_procfs.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $testProcfsElf $testProcfsObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar test_procfs.elf"; exit 1 }

    # test_pty_jobcontrol.elf
    $testPtyJobSrc = "$userDir\test_pty_jobcontrol.c"
    $testPtyJobObj = "$BUILD_DIR\userspace\test_pty_jobcontrol.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $testPtyJobSrc -o $testPtyJobObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar test_pty_jobcontrol.c"; exit 1 }

    $testPtyJobElf = "$initrdRoot\test_pty_jobcontrol.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $testPtyJobElf $testPtyJobObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar test_pty_jobcontrol.elf"; exit 1 }

    # test_shebang_exec.elf
    $testShebangExecSrc = "$userDir\test_shebang_exec.c"
    $testShebangExecObj = "$BUILD_DIR\userspace\test_shebang_exec.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $testShebangExecSrc -o $testShebangExecObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar test_shebang_exec.c"; exit 1 }

    $testShebangExecElf = "$initrdRoot\test_shebang_exec.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $testShebangExecElf $testShebangExecObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar test_shebang_exec.elf"; exit 1 }

    # test_pt_interp_route.elf
    $testPtInterpRouteSrc = "$userDir\test_pt_interp_route.c"
    $testPtInterpRouteObj = "$BUILD_DIR\userspace\test_pt_interp_route.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $testPtInterpRouteSrc -o $testPtInterpRouteObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar test_pt_interp_route.c"; exit 1 }

    $testPtInterpRouteElf = "$initrdRoot\test_pt_interp_route.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $testPtInterpRouteElf $testPtInterpRouteObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar test_pt_interp_route.elf"; exit 1 }

    # ld-eter.elf (dynamic loader bootstrap stub)
    $ldEterSrc = "$userDir\ld_eter.c"
    $ldEterObj = "$BUILD_DIR\userspace\ld_eter.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $ldEterSrc -o $ldEterObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar ld_eter.c"; exit 1 }

    $ldEterElf = "$initrdRoot\ld-eter.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $ldEterElf $ldEterObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar ld-eter.elf"; exit 1 }

    # ptinterp_demo.elf (has PT_INTERP=/ld-eter.elf)
    $ptInterpDemoSrc = "$userDir\ptinterp_demo.c"
    $ptInterpDemoObj = "$BUILD_DIR\userspace\ptinterp_demo.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $ptInterpDemoSrc -o $ptInterpDemoObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar ptinterp_demo.c"; exit 1 }

    $ptInterpDemoElf = "$initrdRoot\ptinterp_demo.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $ptInterpDemoElf $ptInterpDemoObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar ptinterp_demo.elf"; exit 1 }

    # test_auxv.elf
    $testAuxvSrc = "$userDir\test_auxv.c"
    $testAuxvObj = "$BUILD_DIR\userspace\test_auxv.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $testAuxvSrc -o $testAuxvObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar test_auxv.c"; exit 1 }

    $testAuxvElf = "$initrdRoot\test_auxv.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $testAuxvElf $testAuxvObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar test_auxv.elf"; exit 1 }

    # test_dlopen.elf
    $testDlopenSrc = "$userDir\test_dlopen.c"
    $testDlopenObj = "$BUILD_DIR\userspace\test_dlopen.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $testDlopenSrc -o $testDlopenObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar test_dlopen.c"; exit 1 }

    $testDlopenElf = "$initrdRoot\test_dlopen.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $testDlopenElf $testDlopenObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar test_dlopen.elf"; exit 1 }

    # eter_posix_validate.elf
    $eterPosixValidateSrc = "$userDir\eter_posix_validate.c"
    $eterPosixValidateObj = "$BUILD_DIR\userspace\eter_posix_validate.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $eterPosixValidateSrc -o $eterPosixValidateObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar eter_posix_validate.c"; exit 1 }

    $eterPosixValidateElf = "$initrdRoot\eter_posix_validate.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $eterPosixValidateElf $eterPosixValidateObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar eter_posix_validate.elf"; exit 1 }

    # sh.elf
    $shSrc = "$userDir\sh.c"
    $shObj = "$BUILD_DIR\userspace\sh.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $shSrc -o $shObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar sh.c"; exit 1 }

    $shElf = "$initrdRoot\sh.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $shElf $shObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar sh.elf"; exit 1 }

    # eterland.elf
    $eterlandSrc = "$userDir\eterland.c"
    $eterlandObj = "$BUILD_DIR\userspace\eterland.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $eterlandSrc -o $eterlandObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar eterland.c"; exit 1 }

    $eterlandElf = "$initrdRoot\eterland.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $eterlandElf $eterlandObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar eterland.elf"; exit 1 }

    # marea_shell.elf
    $mareaShellSrc = "$userDir\marea_shell.c"
    $mareaShellObj = "$BUILD_DIR\userspace\marea_shell.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $mareaShellSrc -o $mareaShellObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar marea_shell.c"; exit 1 }

    $mareaShellElf = "$initrdRoot\marea_shell.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $mareaShellElf $mareaShellObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar marea_shell.elf"; exit 1 }

    # apt-get.elf
    $aptGetSrc = "$userDir\apt_get.c"
    $aptGetObj = "$BUILD_DIR\userspace\apt_get.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $aptGetSrc -o $aptGetObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar apt_get.c"; exit 1 }

    $aptGetElf = "$initrdRoot\apt-get.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $aptGetElf $aptGetObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar apt-get.elf"; exit 1 }

    # busybox multicall (base POSIX applets)
    $busyboxSrc = "$userDir\busybox.c"
    $busyboxObj = "$BUILD_DIR\userspace\busybox.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $busyboxSrc -o $busyboxObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar busybox.c"; exit 1 }

    $busyboxElf = "$initrdRoot\busybox"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $busyboxElf $busyboxObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar busybox"; exit 1 }

    # eter_update
    $eterUpdateSrc = "$userDir\eter_update.c"
    $eterUpdateObj = "$BUILD_DIR\userspace\eter_update.o"
    & $CC -m64 -mcmodel=large -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -Os -I"$userDir\libc\include" -c $eterUpdateSrc -o $eterUpdateObj
    if ($LASTEXITCODE -ne 0) { Write-Step "ERR" "Fallo al compilar eter_update.c"; exit 1 }

    $eterUpdateElf = "$initrdRoot\u.elf"
    $ErrorActionPreference = "Continue"
    & $LD -T "$userDir\linker.ld" -nostdlib -m elf_x86_64 -o $eterUpdateElf $eterUpdateObj $libcObjs 2>&1
    $ldExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($ldExit -ne 0) { Write-Step "ERR" "Fallo al enlazar eter_update.elf"; exit 1 }

    Write-Step "OK" "Userspace construido: $testElf, $testSyscallsElf, $testEpollElf, $testSigAltElf, $testSignalPosixElf, $testWaitidElf, $testProcfsElf, $testPtyJobElf, $testShebangExecElf, $testPtInterpRouteElf, $ldEterElf, $ptInterpDemoElf, $testAuxvElf, $testDlopenElf, $eterPosixValidateElf, $shElf, $eterlandElf, $mareaShellElf, $aptGetElf, $busyboxElf, $eterUpdateElf"
}

function Invoke-InitrdBuild {
    $initrdBin = "$BUILD_DIR\initrd.bin"
    $initrdSrc = "initrd_root"
    
    if (!(Test-Path $initrdSrc)) {
        New-Item -ItemType Directory -Path $initrdSrc -Force | Out-Null
    }

    Write-Step "BIN" "Generando Initrd.bin..."
    & python tools/gen_logo.py
    if (Test-Path "$initrdSrc\logo.raw") {
        # El splash raw no se usa en el arranque actual y ocupa demasiado initrd
        # para el buffer BIOS de VirtualBox.
        Remove-Item -Force "$initrdSrc\logo.raw"
    }
    & python tools/mkinitrd.py $initrdSrc $initrdBin
    if ($LASTEXITCODE -ne 0) {
        Write-Step "ERR" "Fallo al generar Initrd (Asegurate de tener Python instalado)"
        # No salimos con error fatal porque el sistema puede bootear sin initrd
    }
    else {
        $size = (Get-Item $initrdBin).Length
        Write-Step "OK" "Initrd: $size bytes"
    }
}

function Stop-LockingProcesses {
    # Kill any QEMU or VirtualBox processes that might be locking the image file
    $procs = @("qemu-system-x86_64", "qemu-system-i386", "VirtualBoxVM", "VBoxHeadless")
    $killed = $false
    foreach ($p in $procs) {
        $running = Get-Process -Name $p -ErrorAction SilentlyContinue
        if ($running) {
            Write-Step "IMG" "Deteniendo $p (PID $($running.Id -join ','))..."
            $running | Stop-Process -Force -ErrorAction SilentlyContinue
            $killed = $true
        }
    }
    if ($killed) {
        Start-Sleep -Milliseconds 1500
    }
}

function Invoke-ImageBuild {
    Write-Step "IMG" "Generando imagen de disco..."

    $imagePath = Join-Path (Get-Location) $OS_IMAGE

    # Crear imagen vacía de 32 MB (Vanguard-style large image)
    $imageSize = 65536 * 512  # 33,554,432 bytes
    $imageData = New-Object byte[] $imageSize

    # Leer el bootloader y copiarlo al inicio
    $bootPath = Join-Path (Get-Location) $BOOT_BIN
    $bootData = [System.IO.File]::ReadAllBytes($bootPath)
    [System.Array]::Copy($bootData, 0, $imageData, 0, $bootData.Length)

    # Leer el kernel y copiarlo después de MBR + Stage2
    # Stage 2 = 16 sectores, así que el kernel va en sector 17 (offset = 17 * 512)
    $kernelPath = Join-Path (Get-Location) $KERNEL_BIN
    $kernelData = [System.IO.File]::ReadAllBytes($kernelPath)
    $kernelOffset = 17 * 512
    [System.Array]::Copy($kernelData, 0, $imageData, $kernelOffset, $kernelData.Length)

    # Leer el Initrd y copiarlo después del kernel real.
    $initrdPath = "$BUILD_DIR\initrd.bin"
    if (Test-Path $initrdPath) {
        $initrdData = [System.IO.File]::ReadAllBytes($initrdPath)
        $kernelSectors = Get-KernelSectorCount
        $initrdOffset = (1 + 16 + $kernelSectors) * 512
        if ($initrdOffset + $initrdData.Length -le $imageSize) {
            [System.Array]::Copy($initrdData, 0, $imageData, $initrdOffset, $initrdData.Length)
            Write-Step "OK" "Initrd inyectado en la imagen."
        }
        else {
            Write-Step "ERR" "Initrd demasiado grande para el espacio asignado!"
        }
    }

    # Escribir imagen final (con retry automático si el archivo está bloqueado)
    $maxRetries = 3
    for ($attempt = 1; $attempt -le $maxRetries; $attempt++) {
        try {
            [System.IO.File]::WriteAllBytes($imagePath, $imageData)
            break  # Éxito
        }
        catch [System.IO.IOException] {
            if ($attempt -lt $maxRetries) {
                Write-Step "IMG" "Imagen bloqueada. Intentando liberar (intento $attempt/$maxRetries)..."
                Stop-LockingProcesses
            }
            else {
                throw  # Re-lanzar si agotamos reintentos
            }
        }
    }

    $totalSize = (Get-Item $imagePath).Length
    Write-Step "OK" "Imagen: $imagePath ($totalSize bytes)"

    # Exportar la actualización OTA al dominio/directorio local
    Invoke-ExportUpdate
}

function Invoke-VdiBuild {
    if (!$VBOXMANAGE) {
        Write-Step "ERR" "VBoxManage no encontrado. Instalar VirtualBox para generar VDI."
        Write-Host "    Descargar: https://www.virtualbox.org/wiki/Downloads" -ForegroundColor Gray
        return
    }

    $imgPath = Join-Path (Get-Location) $OS_IMAGE
    $vdiPath = Join-Path (Get-Location) $OS_VDI

    # Eliminar VDI anterior si existe (VBoxManage no sobreescribe)
    if (Test-Path $vdiPath) {
        Remove-Item -Force $vdiPath
    }

    Write-Step "VDI" "Convirtiendo $OS_IMAGE -> $OS_VDI"
    $ErrorActionPreference = "Continue"
    & $VBOXMANAGE convertfromraw $imgPath $vdiPath --format VDI 2>&1 | Out-Null
    $ErrorActionPreference = "Stop"

    if (!(Test-Path $vdiPath)) {
        Write-Step "ERR" "Fallo al convertir a VDI"
        return
    }

    $vdiSize = (Get-Item $vdiPath).Length
    Write-Step "OK" "VDI: $vdiPath ($vdiSize bytes)"
}

function Invoke-PxeBuild {
    Write-Step "PXE" "Generando imagen PXE (Monolitica)..."

    # 1. Asegurar que tenemos Kernel e Initrd construidos
    Invoke-KernelBuild
    Invoke-UserspaceBuild
    Invoke-InitrdBuild

    # 2. Compilar Bootloader con flag PXE y tamaño real del initrd
    Write-Step "ASM" "boot.asm (MODO PXE)"
    $bootDefines = Get-BootAsmDefines
    & $AS -f bin @bootDefines $BOOT_SRC -dPXE -o "$BUILD_DIR\boot_pxe.bin"
    if ($LASTEXITCODE -ne 0) {
        Write-Step "ERR" "Fallo al ensamblar el bootloader PXE"
        exit 1
    }

    # 3. Ensamblar la imagen monolitica (igual que Invoke-ImageBuild)
    $pxePath = "$BUILD_DIR\eteros.pxe"
    $imageSize = 2880 * 512
    $imageData = New-Object byte[] $imageSize
    
    # Mover bootloader PXE
    $bootData = [System.IO.File]::ReadAllBytes("$BUILD_DIR\boot_pxe.bin")
    [System.Array]::Copy($bootData, 0, $imageData, 0, $bootData.Length)
    
    # Mover kernel
    $kernelData = [System.IO.File]::ReadAllBytes($KERNEL_BIN)
    [System.Array]::Copy($kernelData, 0, $imageData, 17 * 512, $kernelData.Length)
    
    # Mover initrd
    $initrdPath = "$BUILD_DIR\initrd.bin"
    if (Test-Path $initrdPath) {
        $initrdData = [System.IO.File]::ReadAllBytes($initrdPath)
        [System.Array]::Copy($initrdData, 0, $imageData, (1 + 16 + 512) * 512, $initrdData.Length)
    }
    
    # Escribir el archivo final (truncado al tamaño real ocupado para ahorrar TFTP bandwidth)
    $kernelSectors = Get-KernelSectorCount
    $actualSize = (1 + 16 + $kernelSectors) * 512
    if (Test-Path $initrdPath) {
        $actualSize += (Get-Item $initrdPath).Length
    }
    $pxeData = New-Object byte[] $actualSize
    [System.Array]::Copy($imageData, 0, $pxeData, 0, $actualSize)
    
    [System.IO.File]::WriteAllBytes($pxePath, $pxeData)
    Write-Step "OK" "PXE Image: $pxePath ($actualSize bytes)"
    Write-Host "  Uso: Configura tu servidor DHCP/TFTP para servir este archivo." -ForegroundColor Gray
}

function Invoke-UsbBuild {
    Write-Step "IMG" "Generando imagen USB HDD (32MB)..."
    Invoke-KernelBuild
    Invoke-UserspaceBuild
    Invoke-InitrdBuild
    Invoke-BootBuild
    
    # Tamaño fijo de 32MB para asegurar compatibilidad con Etcher/BIOS
    $imagePath = "$BUILD_DIR\eteros_usb.img"
    $imageSize = 32 * 1024 * 1024 
    $imageData = New-Object byte[] $imageSize
    
    # 1. Bootloader (MBR + Stage 1) con Partition Table simulada
    $bootData = [System.IO.File]::ReadAllBytes($BOOT_BIN)
    [System.Array]::Copy($bootData, 0, $imageData, 0, 512)
    
    # 2. Bootloader Stage 2 (Sectores 2-17)
    # Nota: boot.asm asume que Stage 2 empieza en sector 2.
    [System.Array]::Copy($bootData, 512, $imageData, 512, $bootData.Length - 512)
    
    # 3. Kernel (Sector 18)
    $kernelPath = $KERNEL_BIN
    $kernelData = [System.IO.File]::ReadAllBytes($kernelPath)
    [System.Array]::Copy($kernelData, 0, $imageData, 17 * 512, $kernelData.Length)
    
    # 4. Initrd
    $initrdPath = "$BUILD_DIR\initrd.bin"
    if (Test-Path $initrdPath) {
        $initrdData = [System.IO.File]::ReadAllBytes($initrdPath)
        $kernelSectors = Get-KernelSectorCount
        $initrdOffset = (1 + 16 + $kernelSectors) * 512
        [System.Array]::Copy($initrdData, 0, $imageData, $initrdOffset, $initrdData.Length)
    }
    
    [System.IO.File]::WriteAllBytes($imagePath, $imageData)
    Write-Step "OK" "USB Image: $imagePath ($imageSize bytes)"
    Write-Host "  Uso: Graba este archivo con Balena Etcher en tu pendrive." -ForegroundColor Gray
}

function Invoke-IsoBuild {
    if (!$MKISOFS) {
        Write-Step "ERR" "mkisofs no encontrado. Instala cdrtools."
        Write-Host "    Para crear ISOs, instala: winget install Schily-Tools.Cdrtools" -ForegroundColor Yellow
        return
    }

    Write-Step "ISO" "Generando ISO bootable (El Torito)..."
    
    # Asegurar que tenemos la imagen base
    Invoke-KernelBuild
    Invoke-UserspaceBuild
    Invoke-InitrdBuild
    Invoke-BootBuild
    Invoke-ImageBuild
    
    $isoRoot = "$BUILD_DIR\iso_root"
    if (Test-Path $isoRoot) { Remove-Item -Recurse -Force $isoRoot }
    New-Item -ItemType Directory $isoRoot | Out-Null
    
    # El archivo de arranque DEBE estar dentro del ISO
    Copy-Item $OS_IMAGE "$isoRoot\boot.img" -Force
    
    $isoPath = "eteros_$Arch.iso"
    
    # mkisofs params:
    # -o : output file
    # -b : boot image file (relativo a iso_root)
    # -c : boot catalog file (generado por mkisofs)
    & $MKISOFS -o $isoPath -b boot.img -c boot.catalog -V "ETEROS" -quiet $isoRoot
    
    if (Test-Path $isoPath) {
        Write-Step "OK" "ISO generado: $isoPath"
        Write-Host "  Tip: Puedes usar Rufus para grabar este ISO en un USB." -ForegroundColor Gray
    }
    else {
        Write-Step "ERR" "Fallo al generar el archivo ISO."
    }
}

function Invoke-VBoxRun {
    if (!$VBOXMANAGE) {
        Write-Step "ERR" "VBoxManage no encontrado. Instalar VirtualBox."
        return
    }

    $vmName = "eterOS_$Arch"
    $vdiPath = (Join-Path (Get-Location) $OS_VDI).Replace('/', '\')

    # Verificar si la VM ya existe
    $ErrorActionPreference = "Continue"
    $vmExists = & $VBOXMANAGE showvminfo $vmName 2>&1
    $ErrorActionPreference = "Stop"

    if ($vmExists -match "Name:") {
        Write-Step "VBOX" "VM '$vmName' ya existe. Actualizando disco..."

        # Apagar si está corriendo
        $ErrorActionPreference = "Continue"
        & $VBOXMANAGE controlvm $vmName poweroff 2>&1 | Out-Null
        Start-Sleep -Seconds 2

        # Desmontar disco anterior
        & $VBOXMANAGE storageattach $vmName --storagectl "SATA" --port 0 --device 0 --medium none 2>&1 | Out-Null

        # Usar una configuracion de video conservadora para BIOS/VBE.
        & $VBOXMANAGE modifyvm $vmName --memory 768 --cpus 1 --vram 64 --graphicscontroller VBoxVGA --accelerate3d off --firmware bios --nic1 nat --nictype1 82540EM --mouse ps2 --keyboard ps2 --uart1 0x3F8 4 --uartmode1 file "$((Get-Location).Path)\build\$Arch\serial.log" 2>&1 | Out-Null
        & $VBOXMANAGE setextradata $vmName "CustomVideoMode1" "1024x768x32" 2>&1 | Out-Null

        # Cerrar medio anterior si existe  
        & $VBOXMANAGE closemedium disk $vdiPath 2>&1 | Out-Null
        $ErrorActionPreference = "Stop"

        # Regenerar VDI fresco
        Invoke-VdiBuild

        # Remontar disco nuevo
        $ErrorActionPreference = "Continue"
        & $VBOXMANAGE storageattach $vmName --storagectl "SATA" --port 0 --device 0 --type hdd --medium $vdiPath 2>&1 | Out-Null
        $ErrorActionPreference = "Stop"
    }
    else {
        Write-Step "VBOX" "Creando VM '$vmName' ($Arch)..."

        # Generar VDI
        Invoke-VdiBuild

        $ErrorActionPreference = "Continue"

        # Limpiar restos de VMs anteriores (archivos huérfanos)
        $vboxVmDir = Join-Path $env:USERPROFILE "VirtualBox VMs\$vmName"
        if (Test-Path $vboxVmDir) {
            Remove-Item -Recurse -Force $vboxVmDir
            Write-Step "VBOX" "Limpiando restos de VM anterior..."
        }

        # Crear VM
        & $VBOXMANAGE createvm --name $vmName --ostype $VBOX_OSTYPE --register 2>&1 | Out-Null

        # Configurar
        $longMode = if ($Arch -eq "x86_64") { "on" } else { "off" }
        
        & $VBOXMANAGE modifyvm $vmName `
            --memory 768 `
            --cpus 1 `
            --vram 64 `
            --firmware bios `
            --long-mode $longMode `
            --graphicscontroller VBoxVGA `
            --accelerate3d off `
            --nic1 nat `
            --nictype1 82540EM `
            --audio-driver none `
            --mouse ps2 `
            --keyboard ps2 `
            --uart1 0x3F8 4 `
            --uartmode1 file "$((Get-Location).Path)\build\$Arch\serial.log" 2>&1 | Out-Null
        & $VBOXMANAGE setextradata $vmName "CustomVideoMode1" "1024x768x32" 2>&1 | Out-Null

        # Crear controladora SATA
        & $VBOXMANAGE storagectl $vmName --name "SATA" --add sata --controller IntelAhci --portcount 1 2>&1 | Out-Null

        # Conectar VDI
        & $VBOXMANAGE storageattach $vmName --storagectl "SATA" --port 0 --device 0 --type hdd --medium $vdiPath 2>&1 | Out-Null

        # Orden de arranque: disco duro primero
        & $VBOXMANAGE modifyvm $vmName --boot1 disk --boot2 none --boot3 none --boot4 none 2>&1 | Out-Null

        $ErrorActionPreference = "Stop"

        Write-Step "OK" "VM '$vmName' creada (BIOS, 768MB RAM, 1 CPU, video VMSVGA, disco: $OS_VDI)"
    }

    # Iniciar la VM
    Write-Step "VBOX" "Iniciando $vmName en VirtualBox..."
    $ErrorActionPreference = "Continue"
    & $VBOXMANAGE startvm $vmName --type gui 2>&1 | Out-Null
    $ErrorActionPreference = "Stop"
    Write-Step "OK" "VM iniciada. Serial log: build\$Arch\serial.log"
}

function Invoke-QemuRun {
    param([bool]$DebugMode = $false, [bool]$NoGraphic = $false)

    Write-Step "QEMU" "Iniciando eterOS ($Arch)..."

    $qemuArgs = @(
        "-drive", "format=raw,file=$OS_IMAGE,index=0,media=disk",
        "-m", "512M",
        "-no-reboot",
        "-no-shutdown"
    )

    if ($NoGraphic) {
        $qemuArgs += "-nographic"
        # -nographic redirects serial 0 to stdio automatically so no need for -serial stdio
    }
    else {
        # Red de QEMU
        $qemuArgs += "-netdev", "user,id=n1"
        $qemuArgs += "-device", "e1000,netdev=n1"
        $qemuArgs += "-object", "filter-dump,id=f1,netdev=n1,file=network.pcap"
        # Regular mode with GUI -> use serial stdio
        $qemuArgs += "-serial", "stdio"
    }

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
    Write-Host "  Output IMG:   $OS_IMAGE"
    Write-Host "  Output VDI:   $OS_VDI"
    if ($VBOXMANAGE) {
        Write-Host "  VBoxManage:   $VBOXMANAGE" -ForegroundColor Green
    }
    else {
        Write-Host "  VBoxManage:   [NO ENCONTRADO - VDI deshabilitado]" -ForegroundColor Yellow
    }
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
        Invoke-KernelBuild
        Invoke-UserspaceBuild
        Invoke-InitrdBuild
        Invoke-BootBuild
        Invoke-ImageBuild
        Invoke-VdiBuild
        if ($MKISOFS) { Invoke-IsoBuild }
        Write-Host ""
        Write-Host "  ================================================" -ForegroundColor Green
        Write-Host "    eterOS construido exitosamente!" -ForegroundColor Green
        Write-Host "    IMG: $OS_IMAGE" -ForegroundColor White
        Write-Host "    VDI: $OS_VDI" -ForegroundColor White
        if ($MKISOFS) { Write-Host "    ISO: eteros_$Arch.iso" -ForegroundColor White }
        Write-Host "    Ejecutar: .\build.ps1 -Target run" -ForegroundColor White
        Write-Host "  ================================================" -ForegroundColor Green
        Write-Host ""
    }
    "pxe" {
        Initialize-BuildDirs
        Invoke-PxeBuild
    }
    "usb" {
        Initialize-BuildDirs
        Invoke-UsbBuild
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
        Invoke-KernelBuild
        Invoke-UserspaceBuild
        Invoke-InitrdBuild
        Invoke-BootBuild
        Invoke-ImageBuild
    }
    "run" {
        Initialize-BuildDirs
        Invoke-KernelBuild
        Invoke-UserspaceBuild
        Invoke-InitrdBuild
        Invoke-BootBuild
        Invoke-ImageBuild
        Invoke-QemuRun
    }
    "run-nographic" {
        Initialize-BuildDirs
        Invoke-KernelBuild
        Invoke-UserspaceBuild
        Invoke-InitrdBuild
        Invoke-BootBuild
        Invoke-ImageBuild
        Invoke-QemuRun -NoGraphic $true
    }
    "debug" {
        Initialize-BuildDirs
        Invoke-KernelBuild
        Invoke-UserspaceBuild
        Invoke-InitrdBuild
        Invoke-BootBuild
        Invoke-ImageBuild
        Invoke-QemuRun -DebugMode $true
    }
    "vdi" {
        Initialize-BuildDirs
        Invoke-KernelBuild
        Invoke-UserspaceBuild
        Invoke-InitrdBuild
        Invoke-BootBuild
        Invoke-ImageBuild
        Invoke-VdiBuild
    }
    "vbox" {
        Initialize-BuildDirs
        Invoke-KernelBuild
        Invoke-UserspaceBuild
        Invoke-InitrdBuild
        Invoke-BootBuild
        Invoke-ImageBuild
        Invoke-VBoxRun
    }
    "iso" {
        Initialize-BuildDirs
        Invoke-IsoBuild
    }
    "clean" {
        Invoke-BuildClean
    }
    "info" {
        Show-BuildInfo
    }
}
