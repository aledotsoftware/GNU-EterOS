<#
.SYNOPSIS
    Automates the packaging, signing, and uploading of éterOS build artifacts.

.DESCRIPTION
    This script takes the kernel and initrd binaries from the build directory,
    compresses them with Zstandard (.zst), generates a JSON manifest with
    SHA256 checksums, signs the manifest using an Ed25519 SSH key, and uploads
    everything to an FTP server in a structured format.

.PARAMETER Version
    The version string for this release (e.g., "1.0.0").

.PARAMETER KeyFile
    The path to the private SSH key (Ed25519) used for signing the manifest.
    The public key should be distributed with the OS verification tool.

.PARAMETER FtpServer
    The FTP server address (e.g., "ftp://ftp.example.com").

.PARAMETER FtpUser
    The username for FTP authentication.

.PARAMETER FtpPass
    The password for FTP authentication.

.PARAMETER BuildDir
    The directory containing the build artifacts (default: "build/x86_64").

.PARAMETER Arch
    The target architecture (default: "x86_64").

.EXAMPLE
    .\tools\updater\build_update.ps1 -Version "1.0.5" -KeyFile ".\secrets\eteros_key" `
        -FtpServer "ftp://update.eteros.org" -FtpUser "deploy" -FtpPass "secret"
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$Version,

    [Parameter(Mandatory=$true)]
    [string]$KeyFile,

    [Parameter(Mandatory=$true)]
    [string]$FtpServer,

    [Parameter(Mandatory=$true)]
    [string]$FtpUser,

    [Parameter(Mandatory=$true)]
    [string]$FtpPass,

    [string]$BuildDir = "build\x86_64",
    [string]$Arch = "x86_64"
)

$ErrorActionPreference = "Stop"

function Check-Command {
    param([string]$Name)
    if (!(Get-Command $Name -ErrorAction SilentlyContinue)) {
        Write-Host "Error: Command '$Name' not found. Please install it and ensure it is in your PATH." -ForegroundColor Red
        exit 1
    }
}

# 1. Check dependencies
Write-Host "Checking dependencies..." -ForegroundColor Cyan
Check-Command "zstd"
Check-Command "ssh-keygen"

# 2. Verify build artifacts
$kernelBin = Join-Path $BuildDir "kernel.bin"
$initrdBin = Join-Path $BuildDir "initrd.bin"

if (!(Test-Path $kernelBin)) {
    Write-Host "Error: Kernel binary not found at $kernelBin" -ForegroundColor Red
    exit 1
}
if (!(Test-Path $initrdBin)) {
    Write-Host "Warning: Initrd binary not found at $initrdBin. Continuing with kernel only..." -ForegroundColor Yellow
}

# 3. Create temporary directory
$tempPath = [System.IO.Path]::GetTempPath()
$tempDir = Join-Path $tempPath "eteros_update_$Version"
if (Test-Path $tempDir) { Remove-Item -Recurse -Force $tempDir }
New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
Write-Host "Working in temporary directory: $tempDir" -ForegroundColor Gray

# 4. Prepare OTA payloads and Compress artifacts
Write-Host "Preparing payloads and compressing artifacts..." -ForegroundColor Cyan

$filesToProcess = @()

# First, package kernel for direct OTA updates
$kernelOtaPath = "$tempDir\kernel.ota"
Write-Host "  Packing payload $($kernelOtaPath)..."
$packProc = Start-Process -FilePath "python3" -ArgumentList "tools\updater\pack_payload.py", "`"$kernelBin`"", "`"$kernelOtaPath`"" -PassThru -NoNewWindow -Wait
if ($packProc.ExitCode -ne 0) {
    Write-Host "Error: pack_payload.py exited with code $($packProc.ExitCode)" -ForegroundColor Red
    exit 1
}

$filesToProcess += @{Src=$kernelOtaPath; Name="kernel.ota"; Dst="$tempDir\kernel.ota.zst"}
$filesToProcess += @{Src=$kernelBin; Name="kernel"; Dst="$tempDir\kernel.zst"}

if (Test-Path $initrdBin) {
    $filesToProcess += @{Src=$initrdBin; Name="initrd"; Dst="$tempDir\initrd.zst"}
}

foreach ($file in $filesToProcess) {
    # Using call operator & for better safety
    Write-Host "  Compressing $($file.Name)..."
    try {
        $proc = Start-Process -FilePath "zstd" -ArgumentList "-19", "`"$($file.Src)`"", "-o", "`"$($file.Dst)`"" -PassThru -NoNewWindow -Wait
        if ($proc.ExitCode -ne 0) {
            throw "zstd exited with code $($proc.ExitCode)"
        }
    } catch {
        Write-Host "Error: Failed to compress $($file.Name): $_" -ForegroundColor Red
        exit 1
    }

    if (!(Test-Path $file.Dst)) {
        Write-Host "Error: Output file $($file.Dst) not created." -ForegroundColor Red
        exit 1
    }
}

# 5. Generate Manifest
Write-Host "Generating manifest..." -ForegroundColor Cyan

$manifest = @{
    version = $Version
    timestamp = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    arch = $Arch
    files = @{}
}

foreach ($file in $filesToProcess) {
    $hash = Get-FileHash -Path $file.Dst -Algorithm SHA256
    $size = (Get-Item $file.Dst).Length

    $manifest.files[$file.Name] = @{
        path = Split-Path $file.Dst -Leaf
        sha256 = $hash.Hash.ToLower()
        size = $size
    }
}

$manifestJson = $manifest | ConvertTo-Json -Depth 3
$manifestPath = Join-Path $tempDir "manifest.json"
# Ensure UTF-8 without BOM if possible, or just UTF8
$manifestJson | Set-Content -Path $manifestPath -Encoding UTF8

# 6. Sign Manifest
Write-Host "Signing manifest..." -ForegroundColor Cyan
if (!(Test-Path $KeyFile)) {
    Write-Host "Error: Private key file not found at $KeyFile" -ForegroundColor Red
    exit 1
}

# ssh-keygen -Y sign -f key_file -n namespace file
$namespace = "eteros-update"
try {
    $proc = Start-Process -FilePath "ssh-keygen" -ArgumentList "-Y", "sign", "-f", "`"$KeyFile`"", "-n", "$namespace", "`"$manifestPath`"" -PassThru -NoNewWindow -Wait
    if ($proc.ExitCode -ne 0) {
         throw "ssh-keygen exited with code $($proc.ExitCode)"
    }
} catch {
    Write-Host "Error: Failed to sign manifest: $_" -ForegroundColor Red
    exit 1
}

$sigPath = "$manifestPath.sig"
if (!(Test-Path $sigPath)) {
    Write-Host "Error: Signature file not found at $sigPath" -ForegroundColor Red
    exit 1
}

# 7. Upload to FTP
Write-Host "Uploading to FTP..." -ForegroundColor Cyan

# Ensure FTP URL ends with /
if (!$FtpServer.EndsWith("/")) { $FtpServer += "/" }
$targetUrl = "${FtpServer}updates/${Version}/"

$webClient = New-Object System.Net.WebClient
$webClient.Credentials = New-Object System.Net.NetworkCredential($FtpUser, $FtpPass)

# Create directory (FTP MKD)
try {
    Write-Host "  Creating directory: updates/$Version"
    $mkdRequest = [System.Net.WebRequest]::Create($targetUrl)
    $mkdRequest.Method = [System.Net.WebRequestMethods+Ftp]::MakeDirectory
    $mkdRequest.Credentials = $webClient.Credentials
    $mkdRequest.GetResponse().Close()
} catch {
    # Ignore if directory already exists (550)
    $msg = $_.Exception.Message
    if ($msg -notmatch "550") {
        Write-Host "  (Directory creation warning: $msg)" -ForegroundColor Yellow
    }
}

$filesToUpload = @($manifestPath, $sigPath, $kernelOtaPath)
foreach ($file in $filesToProcess) {
    $filesToUpload += $file.Dst
}

foreach ($filePath in $filesToUpload) {
    $fileName = Split-Path $filePath -Leaf
    $uploadUrl = "${targetUrl}${fileName}"
    Write-Host "  Uploading $fileName..."
    try {
        $webClient.UploadFile($uploadUrl, "STOR", $filePath) | Out-Null
    } catch {
        Write-Host "Error uploading $fileName: $_" -ForegroundColor Red
        exit 1
    }
}

# 8. Cleanup
Write-Host "Cleaning up..." -ForegroundColor Cyan
Remove-Item -Recurse -Force $tempDir

Write-Host "Update release $Version deployed successfully!" -ForegroundColor Green
