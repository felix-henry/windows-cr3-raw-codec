<#
.SYNOPSIS
    Installs and registers CR3WICCodec.dll (x64) as a native Windows WIC RAW Decoder.
.PARAMETER DllPath
    Optional path to the compiled CR3WICCodec.dll.
.PARAMETER InPlace
    Registers the DLL directly in the build/package folder instead of copying to Program Files\CR3WICCodec.
.PARAMETER RestartExplorer
    Gracefully restarts explorer.exe in the user context to flush cached thumbnails immediately.
#>
[CmdletBinding()]
param(
    [string]$DllPath = "",
    [switch]$InPlace,
    [switch]$RestartExplorer
)

$ErrorActionPreference = "Stop"

$IsGerman = ([System.Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'de')
function T([string]$En, [string]$De) { if ($IsGerman) { $De } else { $En } }

$ClsidCodec = "{D7E4A3B1-5C8F-4A9E-9B2D-6F1A8E3C4D50}"
$WicDecodersCategory = "{7ED96837-96F0-4812-B211-F13C24117ED3}"

# 1. Check Administrator privileges
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error (T "This script must be run as Administrator." "Dieses Skript muss als Administrator ausgefuehrt werden.")
    exit 1
}

# 2. Check VC++ Runtime & Microsoft Raw Image Extension
Write-Host (T "[1/5] Checking prerequisites..." "[1/5] Pruefe Systemvoraussetzungen...") -ForegroundColor Cyan
$vcRuntimeKey = "HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64"
$vcInstalled = (Test-Path $vcRuntimeKey) -and ((Get-ItemProperty -Path $vcRuntimeKey -ErrorAction SilentlyContinue).Installed -eq 1)
if (-not $vcInstalled -and -not (Test-Path "$env:SystemRoot\System32\vcruntime140_1.dll")) {
    Write-Warning (T "Microsoft Visual C++ 2015-2022 Redistributable (x64) does not appear to be installed." "Microsoft Visual C++ 2015-2022 Redistributable (x64) scheint nicht installiert zu sein.")
    Write-Warning (T "If the codec fails to load, install: https://aka.ms/vs/17/release/vc_redist.x64.exe" "Falls der Codec nicht laedt, installiere: https://aka.ms/vs/17/release/vc_redist.x64.exe")
}

try {
    $rawExt = Get-AppxPackage -Name "*RawImageExtension*" -ErrorAction SilentlyContinue
    if ($rawExt) {
        Write-Warning (T "Microsoft Raw Image Extension is installed ($($rawExt.Version))." "Microsoft Raw Image Extension ist installiert ($($rawExt.Version)).")
        Write-Warning (T "The UWP 'Photos' app may continue using it for newer cameras (e.g. Canon EOS R6 Mark III)." "Bei neuen Kameras (z. B. Canon EOS R6 Mark III) nutzt die Store-'Fotos'-App u. U. weiterhin diese Erweiterung.")
        Write-Host (T "   Tip: To remove it in Admin PowerShell: Get-AppxPackage *RawImageExtension* | Remove-AppxPackage" "   Hinweis: Zum Entfernen in Admin-PowerShell: Get-AppxPackage *RawImageExtension* | Remove-AppxPackage") -ForegroundColor Yellow
    } else {
        Write-Host (T "   Microsoft Raw Image Extension not found." "   Keine Microsoft Raw Image Extension gefunden.") -ForegroundColor Green
    }
} catch {
    Write-Warning "$(T 'Appx check skipped:' 'Appx-Pruefung uebersprungen:') $($_.Exception.Message)"
}

# 3. Resolve DLL path
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
    Write-Error (T "CR3WICCodec.dll not found. Build first (scripts\build.ps1) or pass -DllPath." "CR3WICCodec.dll wurde nicht gefunden. Bitte zuerst bauen (scripts\build.ps1) oder -DllPath angeben.")
    exit 1
}

$SourceDir = Split-Path -Parent (Resolve-Path $DllPath)
$LibRawDll = Join-Path $SourceDir "libraw.dll"
if (-not (Test-Path $LibRawDll)) {
    $fallbackLibRaw = Join-Path $RepoRoot "third_party\LibRaw\bin\libraw.dll"
    if (Test-Path $fallbackLibRaw) {
        $LibRawDll = $fallbackLibRaw
    } else {
        Write-Error (T "libraw.dll not found alongside CR3WICCodec.dll or in third_party\LibRaw\bin." "libraw.dll wurde nicht neben CR3WICCodec.dll oder in third_party\LibRaw\bin gefunden.")
        exit 1
    }
}
$VcompDll = Join-Path $SourceDir "vcomp140.dll"

# 4. Copy files & grant AppContainer read permissions
Write-Host (T "[2/5] Installing codec files..." "[2/5] Installiere Codec-Dateien...") -ForegroundColor Cyan
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
        Write-Error (T "Target file in $TargetDir is currently in use (e.g. by Explorer). Close Explorer windows/sign out or pass -RestartExplorer." "Zieldatei in $TargetDir wird derzeit verwendet (z. B. von Explorer). Bitte Explorer schliessen/abmelden oder -RestartExplorer nutzen.")
        exit 1
    }
}

& "$env:SystemRoot\System32\icacls.exe" "$TargetDir" /grant "*S-1-15-2-1:(OI)(CI)(RX)" /Q | Out-Null
Write-Host "   DLL: $TargetDll" -ForegroundColor Gray

# 5. COM / WIC registration via regsvr32
Write-Host (T "[3/5] Registering COM/WIC server via regsvr32..." "[3/5] Registriere COM/WIC-Server via regsvr32...") -ForegroundColor Cyan
$regProc = Start-Process -FilePath "$env:SystemRoot\System32\regsvr32.exe" -ArgumentList "/s `"$TargetDll`"" -Wait -PassThru
if ($regProc.ExitCode -ne 0) {
    Write-Error "$(T 'regsvr32 failed with exit code' 'regsvr32 ist fehlgeschlagen mit Exit-Code') $($regProc.ExitCode)."
    exit $regProc.ExitCode
}

Write-Host (T "[4/5] Verifying WIC registry entries..." "[4/5] Verifiziere WIC-Registry-Eintraege...") -ForegroundColor Cyan
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
        Write-Error "$(T 'Expected WIC registry key missing after regsvr32:' 'Erwarteter WIC-Registry-Key fehlt nach regsvr32:') $requiredKey"
        exit 1
    }
}
Write-Host (T "   COM & WIC registry keys (including Patterns & Formats) verified." "   COM & WIC Registry-Keys inkl. Patterns & Formats verifiziert.") -ForegroundColor Green

# 6. Optional Explorer restart
$thumbCacheDir = Join-Path $env:LOCALAPPDATA "Microsoft\Windows\Explorer"
if ($RestartExplorer) {
    Write-Host (T "[5/5] Restarting Explorer cleanly and clearing thumbnail cache..." "[5/5] Beende Explorer geordnet, leere Thumbnail-Cache und starte Explorer im Benutzerkontext neu...") -ForegroundColor Cyan
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
    Write-Host (T "[5/5] Explorer left running (no restart)." "[5/5] Explorer bleibt stabil aktiv (kein Neustart).") -ForegroundColor Green
    Write-Host (T "   Tip: If old cached .CR3 thumbnails remain visible, run Disk Cleanup (cleanmgr.exe -> Thumbnails)." "   Hinweis: Falls alte gecachte .CR3-Thumbnails sichtbar bleiben, einfach ueber Datentraegerbereinigung (cleanmgr.exe -> Miniaturansichten) leeren.") -ForegroundColor Gray
}

Write-Host (T "`nInstallation complete! .CR3 files now render via CR3WICCodec.dll (LibRaw)." "`nInstallation abgeschlossen! .CR3-Dateien werden nun ueber CR3WICCodec.dll (LibRaw) gerendert.") -ForegroundColor Green
