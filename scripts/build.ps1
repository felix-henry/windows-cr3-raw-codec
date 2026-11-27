<#
.SYNOPSIS
    Baut LibRaw (OpenMP), CR3WICCodec.dll, test_decoder.exe, Install.exe, Uninstall.exe und optional Release-Pakete in dist\.
.PARAMETER LibRawPath
    Optionaler Pfad zu einem bereits vorhandenen LibRaw-Verzeichnis (Standard: third_party\LibRaw).
.PARAMETER Arch
    Zielarchitektur: auto (Standard, erkennt Host-OS), x64 oder arm64.
.PARAMETER Package
    Erstellt zusaetzlich ein portables ZIP-Archiv, Inno-Setup-Installer (falls ISCC installiert ist) und SHA256SUMS.txt unter dist\.
#>
[CmdletBinding()]
param(
    [string]$LibRawPath = "",
    [ValidateSet("auto", "x64", "arm64")]
    [string]$Arch = "auto",
    [switch]$Package
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($LibRawPath)) {
    $LibRawPath = Join-Path $RepoRoot "third_party\LibRaw"
}

$HostArch = "x64"
try {
    $osArch = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString().ToLowerInvariant()
    if ($osArch -eq "arm64") { $HostArch = "arm64" }
} catch {
    if ($env:PROCESSOR_ARCHITECTURE -ieq "ARM64") { $HostArch = "arm64" }
}

if ($Arch -eq "auto") {
    $Arch = $HostArch
}

# 1. Visual Studio Build Tools finden
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere.exe nicht gefunden. Bitte Visual Studio 2022/2026 (mit C++-Desktopentwicklung) installieren."
    exit 1
}

$reqComponent = if ($Arch -eq "arm64") { "Microsoft.VisualStudio.Component.VC.Tools.ARM64" } else { "Microsoft.VisualStudio.Component.VC.Tools.x86.x64" }
$vsPath = & $vswhere -latest -products * -requires $reqComponent -property installationPath | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($vsPath)) {
    $vsPath = & $vswhere -latest -products * -property installationPath | Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($vsPath)) {
    Write-Error "Keine passende Visual Studio C++ Toolchain ($Arch) gefunden."
    exit 1
}

$vsDevCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path $vsDevCmd)) {
    Write-Error "VsDevCmd.bat nicht gefunden unter: $vsDevCmd"
    exit 1
}

$vcTargetArg = if ($Arch -eq "arm64") { "arm64" } else { "amd64" }
$vcHostArg = if ($HostArch -eq "arm64") { "arm64" } else { "amd64" }
$vcInitCmd = "`"$vsDevCmd`" -no_logo -arch=$vcTargetArg -host_arch=$vcHostArg"

# 2. LibRaw klonen falls nicht vorhanden
if (-not (Test-Path $LibRawPath)) {
    Write-Host "[1/4] Klone aktuellen LibRaw-Quellcode..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Force (Split-Path -Parent $LibRawPath) | Out-Null
    & git clone --depth 1 https://github.com/LibRaw/LibRaw.git $LibRawPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Host "[1/4] Verwende LibRaw unter: $LibRawPath" -ForegroundColor Gray
}

# 3. LibRaw mit OpenMP (universal x64 SSE2 bzw. nativ ARM64) bauen
$librawDll = Join-Path $LibRawPath "bin\libraw.dll"
$librawLib = Join-Path $LibRawPath "lib\libraw.lib"
Write-Host "[2/4] Baue LibRaw ($Arch) mit OpenMP..." -ForegroundColor Cyan
$nmakeCmd = "$vcInitCmd && cd /d `"$LibRawPath`" && nmake -f Makefile.msvc `"COPT_OPT=/openmp`" bin\libraw.dll lib\libraw.lib"
cmd /c $nmakeCmd
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $librawDll) -or -not (Test-Path $librawLib)) {
    Write-Error "LibRaw-Build ($Arch) fehlgeschlagen."
    exit 1
}

# 4. CMake Configure & Build
Write-Host "[3/4] Konfiguriere und baue CR3WICCodec ($Arch)..." -ForegroundColor Cyan
$buildDir = Join-Path $RepoRoot "build"
$sysProcArg = if ($Arch -eq "arm64") { "-DCMAKE_SYSTEM_PROCESSOR=ARM64" } else { "-DCMAKE_SYSTEM_PROCESSOR=AMD64" }
$cmakeCmd = "$vcInitCmd && cmake -S `"$RepoRoot`" -B `"$buildDir`" -G Ninja -DCMAKE_BUILD_TYPE=Release $sysProcArg -DLIBRAW_ROOT=`"$LibRawPath`" && cmake --build `"$buildDir`""
cmd /c $cmakeCmd
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake-Build ($Arch) fehlgeschlagen."
    exit 1
}

$binDir = Join-Path $buildDir "bin"
Write-Host "   Build erfolgreich ($Arch): $binDir" -ForegroundColor Green

# 5. Optionales Release-Paket erstellen
if ($Package) {
    Write-Host "[4/4] Erstelle Release-Paket ($Arch) unter dist\..." -ForegroundColor Cyan
    $distDir = Join-Path $RepoRoot "dist"
    $stageDir = Join-Path $distDir "CR3WICCodec-$Arch"
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
    Copy-Item -Path (Join-Path $RepoRoot "scripts\update.ps1") -Destination (Join-Path $stageDir "scripts\update.ps1") -Force
    Copy-Item -Path (Join-Path $RepoRoot "README.md") -Destination $stageDir -Force
    Copy-Item -Path (Join-Path $RepoRoot "LICENSE") -Destination $stageDir -Force
    Copy-Item -Path (Join-Path $RepoRoot "THIRD_PARTY_NOTICES.md") -Destination $stageDir -Force

    foreach ($lic in @("LICENSE.LGPL", "LICENSE.CDDL", "COPYRIGHT")) {
        $licPath = Join-Path $LibRawPath $lic
        if (Test-Path $licPath) {
            Copy-Item -Path $licPath -Destination (Join-Path $stageDir "$lic.LibRaw.txt") -Force
        }
    }

    $zipPath = Join-Path $distDir "CR3WICCodec-$Arch-portable.zip"
    if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
    Compress-Archive -Path "$stageDir\*" -DestinationPath $zipPath
    Write-Host "   Portable ZIP erstellt: $zipPath" -ForegroundColor Green

    $appVersion = "0.0.0"
    $cmakeTxt = Get-Content -Path (Join-Path $RepoRoot "CMakeLists.txt") -Raw
    if ($cmakeTxt -match "project\s*\(\s*CR3WICCodec\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)") {
        $appVersion = $Matches[1]
    }

    $payloadPath = (Join-Path $buildDir "payload-$Arch.bin") -replace '\\', '/'
    & (Join-Path $RepoRoot "scripts\pack_payload.ps1") -SourceDir $stageDir -OutFile $payloadPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $setupBuildCmd = "$vcInitCmd && cmake -S `"$RepoRoot`" -B `"$buildDir`" -G Ninja -DCMAKE_BUILD_TYPE=Release $sysProcArg -DLIBRAW_ROOT=`"$LibRawPath`" -DCR3_PAYLOAD_FILE=`"$payloadPath`" && cmake --build `"$buildDir`" --target Setup"
    cmd /c $setupBuildCmd
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Setup.exe Build ($Arch) fehlgeschlagen."
        exit 1
    }

    $setupOutName = "CR3WICCodec-Setup-$Arch-v$appVersion.exe"
    Copy-Item -Path (Join-Path $binDir "Setup.exe") -Destination (Join-Path $distDir $setupOutName) -Force
    Write-Host "   Standalone Setup ($Arch) erstellt: $(Join-Path $distDir $setupOutName)" -ForegroundColor Green

    $sumsPath = Join-Path $distDir "SHA256SUMS.txt"
    $hashLines = Get-ChildItem -Path $distDir -File | Where-Object { $_.Extension -in @(".zip", ".exe") } | Sort-Object Name | ForEach-Object {
        $h = (Get-FileHash -Path $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$h  $($_.Name)"
    }
    if ($hashLines) {
        Set-Content -Path $sumsPath -Value $hashLines -Encoding ASCII
        Write-Host "   SHA256SUMS.txt erstellt: $sumsPath" -ForegroundColor Green
    }
} else {
    Write-Host "[4/4] Paketierung uebersprungen (starte mit -Package fuer dist\). " -ForegroundColor Gray
}
