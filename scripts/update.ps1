<#
.SYNOPSIS
    Updates Canon CR3 WIC RAW Codec in-place (from GitHub Releases or a local package folder).
.PARAMETER SourceDir
    Optional local directory containing the new CR3WICCodec.dll and libraw.dll.
.PARAMETER FromGitHub
    Forces checking and downloading the latest release from GitHub even if local files exist.
.PARAMETER Force
    Reinstalls even if the installed version matches or exceeds the latest release.
.PARAMETER CheckOnly
    Only checks and displays the installed vs. latest GitHub release version without making changes.
#>
[CmdletBinding()]
param(
    [string]$SourceDir = "",
    [switch]$FromGitHub,
    [switch]$Force,
    [switch]$CheckOnly
)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$IsGerman = ([System.Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'de')
function T([string]$En, [string]$De) { if ($IsGerman) { $De } else { $En } }

$RepoApiUrl = "https://api.github.com/repos/felix-henry/windows-cr3-raw-codec/releases/latest"
$ClsidCodec = "{D7E4A3B1-5C8F-4A9E-9B2D-6F1A8E3C4D50}"
$WicDecodersCategory = "{7ED96837-96F0-4812-B211-F13C24117ED3}"
$TargetDir = Join-Path $env:ProgramFiles "CR3WICCodec"

function Get-InstalledCodecVersion {
    $dll = Join-Path $TargetDir "CR3WICCodec.dll"
    if (-not (Test-Path $dll)) { return $null }
    try {
        $rawVer = (Get-Item $dll).VersionInfo.FileVersion
        if ($rawVer) {
            return [version]($rawVer.Split(' ')[0])
        }
    } catch {}
    return $null
}

function Normalize-Version([string]$tag) {
    $clean = $tag.Trim().TrimStart('v', 'V')
    $parts = $clean.Split('.')
    while ($parts.Count -lt 4) { $parts += "0" }
    return [version](($parts[0..3]) -join '.')
}

function Schedule-DeleteOnReboot([string]$Path) {
    if (-not ("Cr3Win32FileOps" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class Cr3Win32FileOps {
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern bool MoveFileEx(string lpExistingFileName, string lpNewFileName, int dwFlags);
}
"@
    }
    [void][Cr3Win32FileOps]::MoveFileEx($Path, $null, 4) # MOVEFILE_DELAY_UNTIL_REBOOT
}

function Copy-FileSafe([string]$Src, [string]$Dst) {
    try {
        Copy-Item -Path $Src -Destination $Dst -Force -ErrorAction Stop
    } catch {
        if (Test-Path $Dst) {
            $oldPath = "$Dst.old.$([DateTime]::UtcNow.Ticks)"
            Move-Item -Path $Dst -Destination $oldPath -Force
            Schedule-DeleteOnReboot $oldPath
            Copy-Item -Path $Src -Destination $Dst -Force
        } else {
            throw
        }
    }
}

$installedVer = Get-InstalledCodecVersion
Write-Host (T "Installed version : " "Installierte Version : ") -NoNewline -ForegroundColor Cyan
Write-Host ($(if ($installedVer) { $installedVer.ToString() } else { T "Not installed" "Nicht installiert" }))

# Determine source (local vs GitHub)
$RepoRoot = Split-Path -Parent $PSScriptRoot
$localCandidate = $null
if (-not [string]::IsNullOrWhiteSpace($SourceDir)) {
    $localCandidate = $SourceDir
} elseif (-not $FromGitHub) {
    foreach ($dir in @($RepoRoot, (Join-Path $RepoRoot "build\bin"), (Join-Path $RepoRoot "build\bin\Release"))) {
        if ((Test-Path (Join-Path $dir "CR3WICCodec.dll")) -and (Test-Path (Join-Path $dir "libraw.dll"))) {
            $localCandidate = $dir
            break
        }
    }
}

$tempExtractDir = $null
try {
    if ($CheckOnly -or $FromGitHub -or -not $localCandidate) {
        Write-Host (T "Checking latest release on GitHub..." "Pruefe neuestes Release auf GitHub...") -ForegroundColor Cyan
        $headers = @{ "User-Agent" = "CR3WICCodec-Updater" }
        $release = Invoke-RestMethod -Uri $RepoApiUrl -Headers $headers -UseBasicParsing
        $remoteVer = Normalize-Version $release.tag_name
        Write-Host (T "Latest GitHub tag : " "Neuestes GitHub-Tag  : ") -NoNewline -ForegroundColor Cyan
        Write-Host "$($release.tag_name) ($remoteVer)"

        if ($CheckOnly) {
            if ($installedVer -and $installedVer -ge $remoteVer) {
                Write-Host (T "You are already up to date." "Du bist bereits auf dem neuesten Stand.") -ForegroundColor Green
            } else {
                Write-Host (T "A newer version is available!" "Eine neuere Version ist verfuegbar!") -ForegroundColor Yellow
            }
            return
        }

        if ($installedVer -and ($installedVer -ge $remoteVer) -and -not $Force) {
            Write-Host (T "Already up to date ($installedVer). Use -Force to reinstall." "Bereits aktuell ($installedVer). Nutze -Force fuer eine Neuinstallation.") -ForegroundColor Green
            return
        }

        $zipAsset = $release.assets | Where-Object { $_.name -like "*portable*.zip" } | Select-Object -First 1
        if (-not $zipAsset) {
            Write-Error (T "No portable ZIP asset found in release $($release.tag_name)." "Kein portables ZIP-Asset im Release $($release.tag_name) gefunden.")
            exit 1
        }

        $tempRoot = Join-Path $env:TEMP "CR3WICCodecUpdate_$([Guid]::NewGuid().ToString('N'))"
        $zipFile = Join-Path $tempRoot $zipAsset.name
        $tempExtractDir = Join-Path $tempRoot "pkg"
        New-Item -ItemType Directory -Force $tempExtractDir | Out-Null

        Write-Host (T "Downloading $($zipAsset.name)..." "Lade $($zipAsset.name) herunter...") -ForegroundColor Cyan
        Invoke-WebRequest -Uri $zipAsset.browser_download_url -OutFile $zipFile -UseBasicParsing
        Expand-Archive -Path $zipFile -DestinationPath $tempExtractDir -Force
        $localCandidate = $tempExtractDir
    } else {
        Write-Host (T "Using local package : $localCandidate" "Verwende lokales Paket : $localCandidate") -ForegroundColor Gray
        $newDll = Join-Path $localCandidate "CR3WICCodec.dll"
        $newVer = [version]((Get-Item $newDll).VersionInfo.FileVersion.Split(' ')[0])
        if ($installedVer -and ($installedVer -ge $newVer) -and -not $Force) {
            Write-Host (T "Installed version ($installedVer) is already >= local version ($newVer). Use -Force to overwrite." "Installierte Version ($installedVer) ist bereits >= lokale Version ($newVer). Nutze -Force zum Ueberschreiben.") -ForegroundColor Green
            return
        }
    }

    # Elevation check before modifying Program Files / HKLM
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        Write-Error (T "Administrator privileges are required to install the update. Please run PowerShell as Administrator." "Fuer das Update sind Administratorrechte erforderlich. Bitte PowerShell als Administrator ausfuehren.")
        exit 1
    }

    New-Item -ItemType Directory -Path $TargetDir -Force | Out-Null

    # Clean up any unlocked *.old.* leftovers from previous updates
    Get-ChildItem -Path $TargetDir -Filter "*.old.*" -ErrorAction SilentlyContinue | ForEach-Object {
        Remove-Item -Path $_.FullName -Force -ErrorAction SilentlyContinue
    }

    Write-Host (T "Updating files in $TargetDir..." "Aktualisiere Dateien in $TargetDir...") -ForegroundColor Cyan
    foreach ($req in @("CR3WICCodec.dll", "libraw.dll")) {
        $src = Join-Path $localCandidate $req
        if (-not (Test-Path $src)) {
            Write-Error (T "Missing required file: $src" "Benoetigte Datei fehlt: $src")
            exit 1
        }
        Copy-FileSafe $src (Join-Path $TargetDir $req)
    }

    foreach ($opt in @("vcomp140.dll", "test_decoder.exe", "Install.exe", "Uninstall.exe", "README.md", "LICENSE", "THIRD_PARTY_NOTICES.md", "LICENSE.LGPL.LibRaw.txt", "LICENSE.CDDL.LibRaw.txt", "COPYRIGHT.LibRaw.txt")) {
        $src = Join-Path $localCandidate $opt
        if (Test-Path $src) {
            Copy-FileSafe $src (Join-Path $TargetDir $opt)
        }
    }

    $srcScriptsDir = Join-Path $localCandidate "scripts"
    if (Test-Path $srcScriptsDir) {
        $dstScriptsDir = Join-Path $TargetDir "scripts"
        New-Item -ItemType Directory -Force $dstScriptsDir | Out-Null
        Copy-Item -Path "$srcScriptsDir\*" -Destination $dstScriptsDir -Force -ErrorAction SilentlyContinue
    }

    & "$env:SystemRoot\System32\icacls.exe" "$TargetDir" /grant "*S-1-15-2-1:(OI)(CI)(RX)" /Q | Out-Null

    $targetDll = Join-Path $TargetDir "CR3WICCodec.dll"
    $regProc = Start-Process -FilePath "$env:SystemRoot\System32\regsvr32.exe" -ArgumentList "/s `"$targetDll`"" -Wait -PassThru
    if ($regProc.ExitCode -ne 0) {
        Write-Error "$(T 'regsvr32 failed with exit code' 'regsvr32 ist fehlgeschlagen mit Exit-Code') $($regProc.ExitCode)."
        exit $regProc.ExitCode
    }

    $hklmClsid = "HKLM:\SOFTWARE\Classes\CLSID\$ClsidCodec"
    $wicInstance = "HKLM:\SOFTWARE\Classes\CLSID\$WicDecodersCategory\Instance\$ClsidCodec"
    if (-not (Test-Path "$hklmClsid\Patterns\0") -or -not (Test-Path $wicInstance)) {
        Write-Error (T "WIC registry verification failed after update." "WIC-Registry-Pruefung nach dem Update fehlgeschlagen.")
        exit 1
    }

    $updatedVer = Get-InstalledCodecVersion
    Write-Host (T "`nUpdate complete! Active version: $updatedVer" "`nUpdate abgeschlossen! Aktive Version: $updatedVer") -ForegroundColor Green
    if (Get-ChildItem -Path $TargetDir -Filter "*.old.*" -ErrorAction SilentlyContinue) {
        Write-Host (T "Note: Previously loaded DLLs were safely swapped and scheduled for cleanup on next reboot." "Hinweis: Zuvor geladene DLLs wurden sauber ausgetauscht und werden beim naechsten Neustart automatisch bereinigt.") -ForegroundColor Yellow
    }
} finally {
    if ($tempExtractDir) {
        $parent = Split-Path -Parent $tempExtractDir
        Remove-Item -Path $parent -Recurse -Force -ErrorAction SilentlyContinue
    }
}
