# Canon CR3 Native WIC Codec (Windows 10/11 x64)

A native **Windows Imaging Component (WIC)** COM decoder (`CR3WICCodec.dll`) powered by the latest **LibRaw** (`master` with OpenMP multi-core decoding and native support for **all Canon `.CR3` cameras**, including **EOS R6 Mark III, R5 Mark II, R1, R3, R5, R6, R7, R8, R50**, etc.).

Windows Explorer (Thumbnails & Preview Pane), Paint, classic Windows Photo Viewer, and WIC-compatible applications render **true 14-bit demosaiced RAW data** with camera white balance and full dynamic range instead of the embedded JPEG or the magenta/pink tint produced by outdated Microsoft codecs.

---

## Download & Easy Install (Windows 10/11 x64)

**[Download Latest Release](https://github.com/felix-henry/windows-cr3-raw-codec/releases/latest)** — no coding or PowerShell required:

1. **Recommended — Installer (`CR3WICCodec-Setup-x64-v*.exe`):**
   Download and run the setup wizard. Can be uninstalled anytime via **Windows Settings → Apps**.
2. **Portable ZIP (`CR3WICCodec-x64-portable.zip`):**
   Extract the ZIP and double-click **`Install.exe`** (automatically asks for Administrator permission via UAC and registers everything). Double-click **`Uninstall.exe`** to remove cleanly.

---

## Features

- **True 14-bit RAW Demosaicing:** Uses LibRaw `unpack()` + `dcraw_process()` (AHD by default) with camera white balance (`use_camera_wb = 1`), camera color matrix, IEC sRGB gamma, and smooth highlight blending (`highlight = 2`) to eliminate pink/magenta clipped highlights.
- **Fast Multi-Core Decoding:** LibRaw compiled with **OpenMP** (`/openmp`) and universal x64/SSE2 instructions (compatible with all Windows 10/11 x64 CPUs, even without AVX2).
- **Fast RAW Thumbnails:** `IWICBitmapFrameDecode::GetThumbnail` runs LibRaw with `half_size = 1` for fast Explorer previews that still retain full RAW color accuracy.
- **Safe & Self-Contained Deployment:** Uses `/DELAYLOAD:libraw.dll` with explicit sibling loading (`libraw.dll` + `vcomp140.dll`) and `WICComponentUnsigned` metadata so Explorer/WIC hosts never fail due to DLL search paths.
- **Zero Risky Explorer Kills:** Installer registers cleanly without force-killing `explorer.exe`.

---

## System Requirements

- Windows 10 (1809+) or Windows 11 (64-bit / x64)
- [Microsoft Visual C++ 2015–2022 Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe) (already present on most Windows systems)

---

## Manual PowerShell Install / Uninstall (Advanced)

If you prefer PowerShell over `Setup.exe` / `Install.exe`:
```powershell
# Install (Administrator PowerShell)
powershell -ExecutionPolicy Bypass -File .\scripts\install.ps1

# Uninstall (Administrator PowerShell)
powershell -ExecutionPolicy Bypass -File .\scripts\uninstall.ps1
```

> **Note on Microsoft Raw Image Extension:** If `Microsoft.RawImageExtension` is installed from the Microsoft Store, the UWP **Microsoft Photos** app may continue using its own sandboxed decoder. Windows Explorer, Paint, and standard WIC apps will prefer `CR3WICCodec`. If you want to remove the Store extension:
> ```powershell
> Get-AppxPackage *RawImageExtension* | Remove-AppxPackage
> ```

> **Refreshing already-cached thumbnails:** If a folder still shows old cached `.CR3` thumbnails, run Windows **Disk Cleanup** (`cleanmgr.exe`) and select **Thumbnails** only.

---

## Verification CLI (`test_decoder.exe`)

`test_decoder.exe` uses **only** the Windows WIC subsystem (`IWICImagingFactory`) to verify that Windows routes `.CR3` files to `CR3WICCodec`:
```powershell
.\test_decoder.exe "C:\Path\To\Photo.CR3" "output.png" --thumb
```
Expected output:
```text
WIC Decoder selected : Canon CR3 WIC RAW Decoder
WIC Decoder CLSID    : {D7E4A3B1-5C8F-4A9E-9B2D-6F1A8E3C4D50} (CR3WICCodec MATCH)
```

---

## Optional Runtime Tuning (Registry)

Advanced users can customize LibRaw behavior without recompiling by creating DWORD values under `HKLM\SOFTWARE\CR3WICCodec`:

| Value Name | Default | Description |
|---|---|---|
| `DemosaicQuality` | `3` | `0`=Linear, `2`=PPG, `3`=AHD, `4`=DCB, `11`=DHT |
| `HighlightMode` | `2` | `0`=Clip, `2`=Blend (anti-magenta), `5`=Rebuild |
| `NoAutoBright` | `0` | `0`=Conservative auto-brightness (`0.1%`), `1`=Fixed scaling |
| `ChromaNoiseReduction` | `0` | `0`=Off, `1`=FBDD light, `2`=FBDD full (great for high ISO) |

---

## Building from Source

Requires Visual Studio 2022 (Desktop development with C++) and Git:
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Package
```
This automatically clones LibRaw, builds `libraw.dll` with OpenMP, builds `CR3WICCodec.dll` and `test_decoder.exe`, and creates a portable ZIP (plus an Inno Setup installer if Inno Setup 6 is installed) in `dist\`.

---

## License

- Codec source code: **MIT License** ([LICENSE](LICENSE))
- Bundled LibRaw (`libraw.dll`): **LGPL 2.1 / CDDL 1.0** ([THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md))
