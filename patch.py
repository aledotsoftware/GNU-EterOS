import re
with open("p:\\EterOS\\build.ps1", "r", encoding="utf-8") as f:
    data = f.read()

# find catch block
pattern = r"catch \[System\.IO\.IOException\] \{.*?(?=function Invoke-PxeBuild \{)"
replacement = """catch [System.IO.IOException] {
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

"""

new_data = re.sub(pattern, replacement, data, flags=re.DOTALL)
with open("p:\\EterOS\\build.ps1", "w", encoding="utf-8") as f:
    f.write(new_data)
