<#
.SYNOPSIS
    Cleanly unregisters and removes CR3WICCodec.dll from Windows WIC & COM.
.PARAMETER DllPath
    Optional path to CR3WICCodec.dll if not present in the registry.
.PARAMETER RestartExplorer
    Gracefully restarts explorer.exe in the user context.
#>
[CmdletBinding()]
param(
    [string]$DllPath = "",
    [switch]$RestartExplorer
)

$ErrorActionPreference = "Stop"

$IsGerman = ([System.Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'de')
function T([string]$En, [string]$De) { if ($IsGerman) { $De } else { $En } }

$ClsidCodec = "{D7E4A3B1-5C8F-4A9E-9B2D-6F1A8E3C4D50}"
$WicDecodersCategory = "{7ED96837-96F0-4812-B211-F13C24117ED3}"
$HklmClsidRegPath = "HKLM:\SOFTWARE\Classes\CLSID\$ClsidCodec"
$WicInstanceRegPath = "HKLM:\SOFTWARE\Classes\CLSID\$WicDecodersCategory\Instance\$ClsidCodec"

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error (T "This script must be run as Administrator." "Dieses Skript muss als Administrator ausgefuehrt werden.")
    exit 1
}

if ([string]::IsNullOrWhiteSpace($DllPath)) {
    $inprocPath = "$HklmClsidRegPath\InprocServer32"
    if (Test-Path $inprocPath) {
        $DllPath = (Get-ItemProperty -Path $inprocPath -ErrorAction SilentlyContinue)."(default)"
    }
}

Write-Host (T "[1/3] Unregistering COM/WIC codec..." "[1/3] Deregistriere COM/WIC Codec...") -ForegroundColor Cyan
if (-not [string]::IsNullOrWhiteSpace($DllPath) -and (Test-Path $DllPath)) {
    $proc = Start-Process -FilePath "$env:SystemRoot\System32\regsvr32.exe" -ArgumentList "/u /s `"$DllPath`"" -Wait -PassThru
    if ($proc.ExitCode -ne 0) {
        Write-Warning "$(T 'regsvr32 /u returned exit code' 'regsvr32 /u lieferte Exit-Code') $($proc.ExitCode). $(T 'Cleaning up registry anyway.' 'Registry wird trotzdem bereinigt.')"
    }
}

if (Test-Path $WicInstanceRegPath) {
    Remove-Item -Path $WicInstanceRegPath -Recurse -Force -ErrorAction SilentlyContinue
}
if (Test-Path $HklmClsidRegPath) {
    Remove-Item -Path $HklmClsidRegPath -Recurse -Force -ErrorAction SilentlyContinue
}

$ProgramFilesDir = Join-Path $env:ProgramFiles "CR3WICCodec"
if (Test-Path $ProgramFilesDir) {
    Write-Host (T "[2/3] Removing installation folder ($ProgramFilesDir)..." "[2/3] Entferne Installationsverzeichnis ($ProgramFilesDir)...") -ForegroundColor Cyan
    Remove-Item -Path $ProgramFilesDir -Recurse -Force -ErrorAction SilentlyContinue
} else {
    Write-Host (T "[2/3] No installation folder found in Program Files." "[2/3] Kein Installationsverzeichnis in Program Files vorhanden.") -ForegroundColor Gray
}

if ($RestartExplorer) {
    Write-Host (T "[3/3] Restarting Explorer cleanly..." "[3/3] Starte Explorer im Benutzerkontext neu...") -ForegroundColor Cyan
    & "$env:SystemRoot\System32\taskkill.exe" /IM explorer.exe 2>$null | Out-Null
    Start-Sleep -Milliseconds 1500
    $thumbCacheDir = Join-Path $env:LOCALAPPDATA "Microsoft\Windows\Explorer"
    if (Test-Path $thumbCacheDir) {
        Get-ChildItem -Path $thumbCacheDir -Filter "thumbcache_*.db" -Force -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
    }
    $activeExplorer = Get-Process -Name "explorer" -ErrorAction SilentlyContinue | Where-Object { $_.Threads.Count -gt 5 }
    if (-not $activeExplorer) {
        Start-Process -FilePath "$env:WINDIR\System32\runas.exe" -ArgumentList "/trustlevel:0x20000 `"$env:WINDIR\explorer.exe`"" -WindowStyle Hidden -ErrorAction SilentlyContinue
    }
} else {
    Write-Host (T "[3/3] Explorer left running." "[3/3] Explorer bleibt aktiv.") -ForegroundColor Green
}

Write-Host (T "`nUninstallation complete." "`nDeinstallation abgeschlossen.") -ForegroundColor Green
