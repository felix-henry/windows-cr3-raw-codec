<#
.SYNOPSIS
    Packs a staged CR3WICCodec release folder into a single Windows-LZMS-compressed payload blob for Setup.exe.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDir,
    [Parameter(Mandatory = $true)]
    [string]$OutFile
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $SourceDir)) {
    Write-Error "Source directory not found: $SourceDir"
    exit 1
}

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class Cr3CabinetCompress {
    public const uint COMPRESS_ALGORITHM_LZMS = 5;

    [DllImport("cabinet.dll", SetLastError = true)]
    public static extern bool CreateCompressor(uint Algorithm, IntPtr AllocationRoutines, out IntPtr CompressorHandle);

    [DllImport("cabinet.dll", SetLastError = true)]
    public static extern bool Compress(
        IntPtr CompressorHandle,
        byte[] UncompressedData,
        UIntPtr UncompressedDataSize,
        byte[] CompressedBuffer,
        UIntPtr CompressedBufferSize,
        out UIntPtr CompressedDataSize);

    [DllImport("cabinet.dll", SetLastError = true)]
    public static extern bool CloseCompressor(IntPtr CompressorHandle);

    public static byte[] CompressLzms(byte[] input) {
        IntPtr handle;
        if (!CreateCompressor(COMPRESS_ALGORITHM_LZMS, IntPtr.Zero, out handle)) {
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        }
        try {
            UIntPtr needed;
            Compress(handle, input, (UIntPtr)input.Length, null, UIntPtr.Zero, out needed);
            byte[] outBuf = new byte[(int)needed.ToUInt64()];
            UIntPtr actual;
            if (!Compress(handle, input, (UIntPtr)input.Length, outBuf, needed, out actual)) {
                throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
            }
            if ((int)actual.ToUInt64() != outBuf.Length) {
                Array.Resize(ref outBuf, (int)actual.ToUInt64());
            }
            return outBuf;
        } finally {
            CloseCompressor(handle);
        }
    }
}
"@

$srcRoot = (Resolve-Path $SourceDir).Path.TrimEnd('\')
$files = Get-ChildItem -Path $srcRoot -Recurse -File |
    Where-Object { $_.Name -ine "Install.exe" } |
    Sort-Object FullName

if (-not $files) {
    Write-Error "No payload files found under $srcRoot"
    exit 1
}

$ms = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter $ms, ([System.Text.Encoding]::Unicode)

# Header: magic "CR3P" (4 bytes) + uint32 fileCount
$bw.Write([byte[]]@(0x43, 0x52, 0x33, 0x50))
$bw.Write([uint32]$files.Count)

foreach ($f in $files) {
    $rel = $f.FullName.Substring($srcRoot.Length + 1)
    $relBytes = [System.Text.Encoding]::Unicode.GetBytes($rel)
    $data = [System.IO.File]::ReadAllBytes($f.FullName)

    $bw.Write([uint32]($relBytes.Length / 2))
    $bw.Write($relBytes)
    $bw.Write([uint32]$data.Length)
    $bw.Write($data)
}
$bw.Flush()
$rawPayload = $ms.ToArray()
$bw.Dispose()
$ms.Dispose()

$compressed = [Cr3CabinetCompress]::CompressLzms($rawPayload)

$outDir = Split-Path -Parent $OutFile
if ($outDir) { New-Item -ItemType Directory -Force $outDir | Out-Null }

# Outer container header: magic "CR3Z" (4 bytes) + uint64 rawSize + uint64 compSize + compressed LZMS bytes
$outFs = [System.IO.File]::Create($OutFile)
$outBw = New-Object System.IO.BinaryWriter $outFs
$outBw.Write([byte[]]@(0x43, 0x52, 0x33, 0x5A))
$outBw.Write([uint64]$rawPayload.LongLength)
$outBw.Write([uint64]$compressed.LongLength)
$outBw.Write($compressed)
$outBw.Flush()
$outBw.Dispose()
$outFs.Dispose()

Write-Host ("Packed {0} files: {1:N0} bytes raw -> {2:N0} bytes LZMS ({3})" -f $files.Count, $rawPayload.Length, ($compressed.Length + 20), $OutFile)
