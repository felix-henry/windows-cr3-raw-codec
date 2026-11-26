# Technical Guide & Wiki

This page covers advanced configuration, troubleshooting, architecture, and building from source.

---

## 1. Troubleshooting

### Old pink thumbnails still appear in Explorer
Windows caches previously generated thumbnails in `%LOCALAPPDATA%\Microsoft\Windows\Explorer\thumbcache_*.db`.
Run **Disk Cleanup** (`cleanmgr.exe`) on drive `C:`, check **Thumbnails** only, and click **OK**.

### Windows 11 Store "Photos" app still shows a pink tint
The UWP/Store **Microsoft Photos** app runs in an AppContainer sandbox and may directly invoke `Microsoft.RawImageExtension` instead of external WIC codecs. Windows Explorer, Paint, and classic Win32 WIC viewers use `CR3WICCodec`.
To remove Microsoft's Store Raw Image Extension (Administrator PowerShell):
```powershell
Get-AppxPackage *RawImageExtension* | Remove-AppxPackage
```

---

## 2. Verification CLI (`test_decoder.exe`)

`test_decoder.exe` uses **only** the Windows WIC subsystem (`IWICImagingFactory`) to verify that Windows selects `CR3WICCodec`:
```powershell
.\test_decoder.exe "C:\Path\To\Photo.CR3" "output.png" --thumb
```
Expected output:
```text
WIC Decoder selected : Canon CR3 WIC RAW Decoder
WIC Decoder CLSID    : {D7E4A3B1-5C8F-4A9E-9B2D-6F1A8E3C4D50} (CR3WICCodec MATCH)
```

---

## 3. Optional Runtime Tuning (Registry)

Customize LibRaw behavior without recompiling by creating `DWORD` values under `HKLM\SOFTWARE\CR3WICCodec`:

| Value Name | Default | Description |
|---|---|---|
| `DemosaicQuality` | `3` | `0` = Linear, `2` = PPG, `3` = AHD, `4` = DCB, `11` = DHT |
| `HighlightMode` | `2` | `0` = Clip, `2` = Blend (anti-magenta), `5` = Rebuild |
| `NoAutoBright` | `0` | `0` = Conservative auto-brightness (`0.1%`), `1` = Fixed scaling |
| `ChromaNoiseReduction` | `0` | `0` = Off, `1` = FBDD light, `2` = FBDD full (for high ISO) |

---

## 4. Manual PowerShell Install / Uninstall

```powershell
# Install (Administrator PowerShell)
powershell -ExecutionPolicy Bypass -File .\scripts\install.ps1

# Uninstall (Administrator PowerShell)
powershell -ExecutionPolicy Bypass -File .\scripts\uninstall.ps1
```
Pass `-RestartExplorer` if you want the script to restart `explorer.exe` in the normal user context (`runas /trustlevel:0x20000`).

---

## 5. Architecture Overview

- **`CR3WICCodec.dll`:** x64 COM In-Proc Server (`IWICBitmapDecoder`, `IWICBitmapFrameDecode`, `IClassFactory`) registered under `WICBitmapDecoders` with 24-byte + 8-byte CR3 ISOBMFF header patterns (`ftypcrx `) and `WICComponentUnsigned`.
- **Stream I/O:** `WicIStreamDatastream` implements `LibRaw_abstract_datastream` with a 64 KB read-ahead buffer over COM `IStream*`.
- **Delay-loaded LibRaw:** Linked with `/DELAYLOAD:libraw.dll`; `EnsureLibRawLoaded()` preloads sibling `vcomp140.dll` and `libraw.dll` using `LOAD_WITH_ALTERED_SEARCH_PATH`.
- **Thumbnails:** `GetThumbnail()` runs LibRaw with `half_size = 1` and `user_qual = 0` for fast, color-accurate RAW previews.

---

## 6. Building from Source

Requires Visual Studio 2022 (Desktop development with C++), CMake/Ninja, and Git:
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Package
```
This clones LibRaw (`master`), builds `libraw.dll` with `/openmp` (universal x64/SSE2, no AVX2 requirement), builds `CR3WICCodec.dll`, `test_decoder.exe`, `Install.exe`, and `Uninstall.exe`, and packages `dist\CR3WICCodec-x64-portable.zip` (plus an Inno Setup `.exe` if Inno Setup 6 is installed).
