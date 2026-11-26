<#
.SYNOPSIS
    Installiert und registriert CR3WICCodec.dll (x64) als nativen Windows WIC RAW Decoder.
.PARAMETER DllPath
    Optionaler Pfad zur kompilierten CR3WICCodec.dll.
.PARAMETER InPlace
    Registriert die DLL direkt im Quell-/Build-Ordner statt nach C:\Program Files\CR3WICCodec zu kopieren.
.PARAMETER RestartExplorer
    Startet den Windows-Explorer am Ende geordnet im Benutzerkontext neu, um alte Thumbnails sofort zu verwerfen.
    Standardmaessig deaktiviert, um laufende Windows-11-Explorer-Sitzungen nicht zu unterbrechen.
#>
[CmdletBinding()]
param(
    [string]$DllPath = "",
    [switch]$InPlace,
    [switch]$RestartExplorer
)

$ErrorActionPreference = "Stop"

$ClsidCodec = "{D7E4A3B1-5C8F-4A9E-9B2D-6F1A8E3C4D50}"
$WicDecodersCategory = "{7ED96837-96F0-4812-B211-F13C24117ED3}"

# 1. Administrator-Rechte pruefen
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "Dieses Skript muss als Administrator ausgefuehrt werden (Rechtsklick -> 'Als Administrator ausfuehren')."
    exit 1
}

# 2. VC++ Runtime & Konflikt-Pruefung
Write-Host "[1/5] Pruefe Systemvoraussetzungen..." -ForegroundColor Cyan
$vcRuntimeKey = "HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64"
$vcInstalled = (Test-Path $vcRuntimeKey) -and ((Get-ItemProperty -Path $vcRuntimeKey -ErrorAction SilentlyContinue).Installed -eq 1)
if (-not $vcInstalled -and -not (Test-Path "$env:SystemRoot\System32\vcruntime140_1.dll")) {
    Write-Warning "Microsoft Visual C++ 2015-2022 Redistributable (x64) scheint nicht installiert zu sein."
    Write-Warning "Falls der Codec nicht laedt, installiere: https://aka.ms/vs/17/release/vc_redist.x64.exe"
}

try {
    $rawExt = Get-AppxPackage -Name "*RawImageExtension*" -ErrorAction SilentlyContinue
    if ($rawExt) {
        Write-Warning "Microsoft Raw Image Extension ist installiert ($($rawExt.Version))."
        Write-Warning "Bei neuen Kameras (z. B. Canon EOS R6 Mark III) nutzt die Store-'Fotos'-App u. U. weiterhin diese Erweiterung."
        Write-Host "   Hinweis: Zum Entfernen in Admin-PowerShell: Get-AppxPackage *RawImageExtension* | Remove-AppxPackage" -ForegroundColor Yellow
    } else {
        Write-Host "   Keine Microsoft Raw Image Extension gefunden." -ForegroundColor Green
    }
} catch {
    Write-Warning "Appx-Pruefung uebersprungen: $($_.Exception.Message)"
}

# 3. DLL-Pfad aufloesen
$RepoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($DllPath)) {
    $candidates = @(
        (Join-Path $RepoRoot "CR3WICCodec.dll"),
        (Join-Path $RepoRoot "build\bin\Release\CR3WICCodec.dll"),
        (Join-Path $RepoRoot "build\bin\CR3WICCodec.dll"),
        (Join-Path $RepoRoot "build\Release\CR3WICCodec.dll"),
        (Join-Path $RepoRoot "build\CR3WICCodec.dll")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) {
            $DllPath = $c
            break
        }
    }
}

if ([string]::IsNullOrWhiteSpace($DllPath) -or -not (Test-Path $DllPath)) {
    Write-Error "CR3WICCodec.dll wurde nicht gefunden. Bitte zuerst bauen (scripts\build.ps1) oder -DllPath angeben."
    exit 1
}

$SourceDir = Split-Path -Parent (Resolve-Path $DllPath)
$LibRawDll = Join-Path $SourceDir "libraw.dll"
if (-not (Test-Path $LibRawDll)) {
    $fallbackLibRaw = Join-Path $RepoRoot "third_party\LibRaw\bin\libraw.dll"
    if (Test-Path $fallbackLibRaw) {
        $LibRawDll = $fallbackLibRaw
    } else {
        Write-Error "libraw.dll wurde nicht neben CR3WICCodec.dll oder in third_party\LibRaw\bin gefunden."
        exit 1
    }
}
$VcompDll = Join-Path $SourceDir "vcomp140.dll"

# 4. In Zielverzeichnis kopieren & AppContainer-Leserechte setzen
Write-Host "[2/5] Installiere Codec-Dateien..." -ForegroundColor Cyan
if ($InPlace) {
    $TargetDir = $SourceDir
    $TargetDll = (Resolve-Path $DllPath).Path
    $TargetLibRaw = Join-Path $SourceDir "libraw.dll"
    if (-not (Test-Path $TargetLibRaw)) {
        Copy-Item -Path $LibRawDll -Destination $TargetLibRaw -Force
    }
} else {
    $TargetDir = Join-Path $env:ProgramFiles "CR3WICCodec"
    New-Item -ItemType Directory -Path $TargetDir -Force | Out-Null
    $TargetDll = Join-Path $TargetDir "CR3WICCodec.dll"
    $TargetLibRaw = Join-Path $TargetDir "libraw.dll"
    try {
        Copy-Item -Path $DllPath -Destination $TargetDll -Force
        Copy-Item -Path $LibRawDll -Destination $TargetLibRaw -Force
        if (Test-Path $VcompDll) {
            Copy-Item -Path $VcompDll -Destination (Join-Path $TargetDir "vcomp140.dll") -Force
        }
    } catch {
        Write-Error "Zieldatei in $TargetDir wird derzeit verwendet (z. B. von Explorer). Bitte Explorer schliessen/abmelden oder -RestartExplorer nutzen."
        exit 1
    }
}

# Leserechte fuer ALL APPLICATION PACKAGES (S-1-15-2-1) sicherstellen
& "$env:SystemRoot\System32\icacls.exe" "$TargetDir" /grant "*S-1-15-2-1:(OI)(CI)(RX)" /Q | Out-Null
Write-Host "   Ziel-DLL: $TargetDll" -ForegroundColor Gray

# 5. COM-Registrierung via regsvr32
Write-Host "[3/5] Registriere COM/WIC-Server via regsvr32..." -ForegroundColor Cyan
$regProc = Start-Process -FilePath "$env:SystemRoot\System32\regsvr32.exe" -ArgumentList "/s `"$TargetDll`"" -Wait -PassThru
if ($regProc.ExitCode -ne 0) {
    Write-Error "regsvr32 ist mit Exit-Code $($regProc.ExitCode) fehlgeschlagen."
    exit $regProc.ExitCode
}

Write-Host "[4/5] Verifiziere WIC-Registry-Eintraege..." -ForegroundColor Cyan
$ClsidRegPath = "Registry::HKEY_CLASSES_ROOT\CLSID\$ClsidCodec"
$HklmClsidRegPath = "HKLM:\SOFTWARE\Classes\CLSID\$ClsidCodec"
$WicInstanceRegPath = "HKLM:\SOFTWARE\Classes\CLSID\$WicDecodersCategory\Instance\$ClsidCodec"

foreach ($requiredKey in @(
    $ClsidRegPath,
    $HklmClsidRegPath,
    "$HklmClsidRegPath\InprocServer32",
    "$HklmClsidRegPath\Patterns\0",
    "$HklmClsidRegPath\Formats",
    $WicInstanceRegPath
)) {
    if (-not (Test-Path $requiredKey)) {
        Write-Error "Erwarteter WIC-Registry-Key fehlt nach regsvr32: $requiredKey"
        exit 1
    }
}
Write-Host "   COM & WIC Registry-Keys inkl. Patterns & Formats verifiziert." -ForegroundColor Green

# 6. Thumbnail-Cache bereinigen (Explorer bleibt standardmaessig unberuehrt)
$thumbCacheDir = Join-Path $env:LOCALAPPDATA "Microsoft\Windows\Explorer"
if ($RestartExplorer) {
    Write-Host "[5/5] Beende Explorer geordnet, leere Thumbnail-Cache und starte Explorer im Benutzerkontext neu..." -ForegroundColor Cyan
    & "$env:SystemRoot\System32\taskkill.exe" /IM explorer.exe 2>$null | Out-Null
    Start-Sleep -Milliseconds 1500
    if (Test-Path $thumbCacheDir) {
        Get-ChildItem -Path $thumbCacheDir -Filter "thumbcache_*.db" -Force -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
        Get-ChildItem -Path $thumbCacheDir -Filter "iconcache_*.db" -Force -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
    }
    $activeExplorer = Get-Process -Name "explorer" -ErrorAction SilentlyContinue | Where-Object { $_.Threads.Count -gt 5 }
    if (-not $activeExplorer) {
        Start-Process -FilePath "$env:WINDIR\System32\runas.exe" -ArgumentList "/trustlevel:0x20000 `"$env:WINDIR\explorer.exe`"" -WindowStyle Hidden -ErrorAction SilentlyContinue
    }
} else {
    Write-Host "[5/5] Explorer bleibt stabil aktiv (kein Neustart)." -ForegroundColor Green
    Write-Host "   Hinweis: Falls alte gecachte .CR3-Thumbnails sichtbar bleiben, einfach ueber Datentraegerbereinigung (cleanmgr.exe -> Miniaturansichten) leeren." -ForegroundColor Gray
}

Write-Host "`nInstallation abgeschlossen! .CR3-Dateien werden nun ueber CR3WICCodec.dll (LibRaw) gerendert." -ForegroundColor Green
