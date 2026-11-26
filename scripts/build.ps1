<#
.SYNOPSIS
    Baut LibRaw (x64, OpenMP), CR3WICCodec.dll, test_decoder.exe und optional ein Release-Paket in dist\.
.PARAMETER LibRawPath
    Optionaler Pfad zu einem bereits vorhandenen LibRaw-Verzeichnis (Standard: third_party\LibRaw).
.PARAMETER Package
    Erstellt zusaetzlich ein portables ZIP-Archiv (und falls Inno Setup installiert ist, den Setup-Installer) unter dist\.
#>
[CmdletBinding()]
param(
    [string]$LibRawPath = "",
    [switch]$Package
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($LibRawPath)) {
    $LibRawPath = Join-Path $RepoRoot "third_party\LibRaw"
}

# 1. Visual Studio Build Tools finden
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere.exe nicht gefunden. Bitte Visual Studio 2022 (mit C++-Desktopentwicklung) installieren."
    exit 1
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($vsPath)) {
    Write-Error "Keine Visual Studio C++ x64 Toolchain gefunden."
    exit 1
}

$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    Write-Error "vcvars64.bat nicht gefunden unter: $vcvars"
    exit 1
}

# 2. LibRaw klonen falls nicht vorhanden
if (-not (Test-Path $LibRawPath)) {
    Write-Host "[1/4] Klone aktuellen LibRaw-Quellcode..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Force (Split-Path -Parent $LibRawPath) | Out-Null
    & git clone --depth 1 https://github.com/LibRaw/LibRaw.git $LibRawPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Host "[1/4] Verwende LibRaw unter: $LibRawPath" -ForegroundColor Gray
}

# 3. LibRaw mit OpenMP (universal x64 SSE2, ohne AVX2) bauen
$librawDll = Join-Path $LibRawPath "bin\libraw.dll"
$librawLib = Join-Path $LibRawPath "lib\libraw.lib"
Write-Host "[2/4] Baue LibRaw x64 mit OpenMP..." -ForegroundColor Cyan
$nmakeCmd = "`"$vcvars`" && cd /d `"$LibRawPath`" && nmake -f Makefile.msvc `"COPT_OPT=/openmp`" bin\libraw.dll lib\libraw.lib"
cmd /c $nmakeCmd
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $librawDll) -or -not (Test-Path $librawLib)) {
    Write-Error "LibRaw-Build fehlgeschlagen."
    exit 1
}

# 4. CMake Configure & Build
Write-Host "[3/4] Konfiguriere und baue CR3WICCodec.dll & test_decoder.exe..." -ForegroundColor Cyan
$buildDir = Join-Path $RepoRoot "build"
$cmakeCmd = "`"$vcvars`" && cmake -S `"$RepoRoot`" -B `"$buildDir`" -G Ninja -DCMAKE_BUILD_TYPE=Release -DLIBRAW_ROOT=`"$LibRawPath`" && cmake --build `"$buildDir`""
cmd /c $cmakeCmd
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake-Build fehlgeschlagen."
    exit 1
}

$binDir = Join-Path $buildDir "bin"
Write-Host "   Build erfolgreich: $binDir" -ForegroundColor Green

# 5. Optionales Release-Paket erstellen
if ($Package) {
    Write-Host "[4/4] Erstelle Release-Paket unter dist\..." -ForegroundColor Cyan
    $distDir = Join-Path $RepoRoot "dist"
    $stageDir = Join-Path $distDir "CR3WICCodec-x64"
    Remove-Item -Path $stageDir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force $stageDir | Out-Null
    New-Item -ItemType Directory -Force (Join-Path $stageDir "scripts") | Out-Null

    foreach ($file in @("Install.exe", "Uninstall.exe", "CR3WICCodec.dll", "libraw.dll", "vcomp140.dll", "test_decoder.exe")) {
        $src = Join-Path $binDir $file
        if (Test-Path $src) {
            Copy-Item -Path $src -Destination $stageDir -Force
        }
    }

    Copy-Item -Path (Join-Path $RepoRoot "scripts\install.ps1") -Destination (Join-Path $stageDir "scripts\install.ps1") -Force
    Copy-Item -Path (Join-Path $RepoRoot "scripts\uninstall.ps1") -Destination (Join-Path $stageDir "scripts\uninstall.ps1") -Force
    Copy-Item -Path (Join-Path $RepoRoot "README.md") -Destination $stageDir -Force
    Copy-Item -Path (Join-Path $RepoRoot "LICENSE") -Destination $stageDir -Force
    Copy-Item -Path (Join-Path $RepoRoot "THIRD_PARTY_NOTICES.md") -Destination $stageDir -Force

    foreach ($lic in @("LICENSE.LGPL", "LICENSE.CDDL", "COPYRIGHT")) {
        $licPath = Join-Path $LibRawPath $lic
        if (Test-Path $licPath) {
            Copy-Item -Path $licPath -Destination (Join-Path $stageDir "$lic.LibRaw.txt") -Force
        }
    }

    $zipPath = Join-Path $distDir "CR3WICCodec-x64-portable.zip"
    if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
    Compress-Archive -Path "$stageDir\*" -DestinationPath $zipPath
    Write-Host "   Portable ZIP erstellt: $zipPath" -ForegroundColor Green

    $iscc = Get-Command iscc.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
    if (-not $iscc) {
        $isccCandidate = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
        if (Test-Path $isccCandidate) { $iscc = $isccCandidate }
    }
    if ($iscc) {
        & $iscc "/DSourceDir=$stageDir" "/DOutputDir=$distDir" (Join-Path $RepoRoot "installer\CR3WICCodec.iss")
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        Write-Host "   Inno Setup Installer erstellt unter: $distDir" -ForegroundColor Green
    } else {
        Write-Host "   Hinweis: Inno Setup 6 (ISCC.exe) nicht gefunden - nur Portable-ZIP erstellt." -ForegroundColor Yellow
    }
} else {
    Write-Host "[4/4] Paketierung uebersprungen (starte mit -Package fuer dist\). " -ForegroundColor Gray
}
