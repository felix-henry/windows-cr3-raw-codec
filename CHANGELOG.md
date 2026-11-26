# Changelog

All notable changes to **Canon CR3 Native WIC Codec** are documented in this file.
On tagged releases (`v*`), GitHub Actions automatically extracts the matching `## vX.Y.Z` section as the GitHub Release body.

## v1.1.0

Performance, concurrency, and build-pipeline release bringing zero-copy WIC bitmap caching, instant in-memory thumbnail reuse, an optional ultra-fast embedded JPEG thumbnail mode, unified versioning, and automated CI decoder verification.

### What's New in v1.1.0

- **Zero-Copy Frame & Thumbnail Caching (`CR3Decoder`)**
  - Decoded `thumbnail` and `fullImage` buffers are now shared via `std::shared_ptr<const DecodedImageBGR24>` across decoder frames and `CR3BitmapSource` instances instead of copying multi-megabyte BGR24 buffers.
  - `CopyPixels` now releases the decoder mutex before copying scanlines (`CopyBGR24Rect`), improving responsiveness when Windows Explorer or Photos reads tiles concurrently.
- **2×2 In-Memory Thumbnail Reuse**
  - If the full-resolution RAW image has already been decoded when `GetThumbnail` is requested, the half-size thumbnail is now downsampled directly in RAM via 2×2 box averaging (`DownsampleHalf2x2`) instead of running LibRaw a second time.
- **Optional `FastEmbeddedThumb` Registry Mode (`HKLM\SOFTWARE\CR3WICCodec`)**
  - Added an optional `FastEmbeddedThumb` (`DWORD`, default `0`) tuning setting for large folders where maximum Explorer thumbnail speed is preferred over RAW-matched colors.
  - When set to `1`, thumbnails try decoding the embedded JPEG preview inside the `.CR3` via LibRaw + Windows WIC JPEG decoder (including EXIF rotation) and automatically fall back to `half_size = 1` RAW demosaicing if unavailable.
  - `ReadHeader` now also loads `LoadCodecTuning()` consistently.
- **Centralized Version Synchronization (`1.1.0.0`)**
  - Introduced `src/Version.h` driven directly by `CMakeLists.txt` (`CR3_VER_MAJOR`, `CR3_VER_MINOR`, `CR3_VER_PATCH`) so the Windows DLL version resource (`CR3Codec.rc`) and COM Registry metadata (`Registry.cpp`) always stay in sync.
- **Automated CI Smoke Test**
  - GitHub Actions now runs `test_decoder.exe` on `docs/images/canon-eos-r6-mark-iii-sample.CR3` (`--thumb --dll`) on every build/release to verify header dimensions, thumbnail output, and full 14-bit RAW decoding.

### Verified Checks

- Tested with **Canon EOS R6 Mark III** (`.CR3`, `6986 × 4664` full RAW, `3493 × 2332` half-size thumbnail) via `test_decoder.exe --thumb --dll`:
  - `QueryCapability`: `hr=0x00000000`, `caps=0x12`
  - Consistent 14-bit color & clipping metrics across full decode and thumbnail generation.

### Installation & Upgrading

- **Upgrade from an earlier version (v1.0.0 / v1.0.1 / v1.0.2)** directly in PowerShell:
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File "C:\Program Files\CR3WICCodec\update.ps1" -FromGitHub
  ```
- **Fresh Install (Recommended)**: Download and run **`CR3WICCodec-Setup-x64-v1.1.0.exe`** below.
- **Portable**: Download **`CR3WICCodec-x64-portable.zip`**, extract it to a permanent folder, and run `Install.exe` (or `install.ps1` as Administrator).

### Downloads

| File | Description |
| :--- | :--- |
| **`CR3WICCodec-Setup-x64-v1.1.0.exe`** | Windows Installer (DE/EN) with automatic elevation, Explorer thumbnail cache refresh, and Windows Uninstaller entry |
| **`CR3WICCodec-x64-portable.zip`** | Portable archive containing `CR3WICCodec.dll`, `libraw.dll`, `vcomp140.dll`, `test_decoder.exe`, `Install.exe`, `Uninstall.exe`, `update.ps1`, and PowerShell helpers |

**Full Changelog:** [`v1.0.2...v1.1.0`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.0.2...v1.1.0)

## v1.0.2

Adds an automated update script and lock-safe in-place upgrades so updating never requires closing or restarting Windows Explorer.

### What's Changed
- **New Update Script (`scripts/update.ps1`):**
  - Checks the locally installed `CR3WICCodec.dll` version against a local folder or the latest GitHub Release (`-FromGitHub`, `-CheckOnly`, `-SourceDir`, `-Force`).
  - Automatically downloads and extracts the latest portable release package and installs it in-place (auto-localized DE / EN).
- **Safe In-Place DLL Replacement (`Install.exe` & `update.ps1`):**
  - If `CR3WICCodec.dll` or `libraw.dll` is currently locked by Windows Explorer, the old file is safely renamed (`*.old.*`) and scheduled for automatic cleanup on the next reboot (`MoveFileExW` / `MOVEFILE_DELAY_UNTIL_REBOOT`) before copying the new DLLs—no `explorer.exe` restart needed.
- **Docs & Packaging:** Added update instructions to `README.md`, bundled `scripts/update.ps1` into the portable ZIP, and bumped version metadata to `1.0.2.0`.

**Full Changelog:** [`v1.0.1...v1.0.2`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.0.1...v1.0.2)

## v1.0.1

Small localization update for the native setup tools and PowerShell scripts.

### What's Changed
- **Automatic UI Localization (DE / EN):**
  - `Install.exe` and `Uninstall.exe` now detect the Windows display language (`GetUserDefaultUILanguage`) and automatically show German dialogs on German Windows and English dialogs on all other systems.
  - `scripts/install.ps1` and `scripts/uninstall.ps1` also adapt all status, warning, and error messages based on `CurrentUICulture`.
- **Version Metadata:** Updated DLL, CMake, and Inno Setup version metadata to `1.0.1.0`.

**Full Changelog:** [`v1.0.0...v1.0.1`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.0.0...v1.0.1)

## v1.0.0

Initial release of a native **Windows Imaging Component (WIC)** COM decoder for Canon `.CR3` RAW files, built on the latest **LibRaw** (`master`) with **OpenMP** multi-core acceleration.

Designed to replace outdated Microsoft RAW decoding on Windows Explorer, Preview Pane, Paint, and Win32 WIC viewers—rendering true **14-bit demosaiced RAW data** with accurate colors and highlight preservation instead of embedded JPEGs or magenta/pink highlight casts.

### What's Included / Key Features
- **Full 14-Bit RAW Demosaicing:** Uses LibRaw (`unpack()` + `dcraw_process()` with AHD interpolation), camera white balance (`use_camera_wb = 1`), camera color matrix, IEC sRGB gamma, and smooth highlight blending (`highlight = 2`) to prevent pink/magenta tinting.
- **Fast RAW Thumbnails:** `IWICBitmapFrameDecode::GetThumbnail` runs a dedicated `half_size = 1` LibRaw path for fast Windows Explorer previews with full RAW color accuracy.
- **Broad Canon CR3 Support:** Supports modern Canon `.CR3` cameras supported by LibRaw, including **EOS R6 Mark III**, R5 Mark II, R1, R3, R5, R6, R6 Mark II, R7, R8, R10, R50, R100, 90D, 250D, 1D X Mark III, M50, and PowerShot V1 / G-series.
- **OpenMP Multi-Core Acceleration:** `libraw.dll` compiled with `/openmp` and universal x64/SSE2 compatibility (no AVX2 requirement, works on all 64-bit Windows 10/11 CPUs).
- **WIC Arbitration & AppContainer Safety:** Registered with extended 24-byte + 8-byte ISOBMFF `.CR3` (`ftypcrx `) patterns, `ArbitrationPriority = 20`, `WICComponentUnsigned`, `/DELAYLOAD:libraw.dll` sibling loading, and `ALL APPLICATION PACKAGES` read/execute ACLs.
- **Runtime Tuning via Registry:** Optional `DWORD` overrides under `HKLM\SOFTWARE\CR3WICCodec` (`DemosaicQuality`, `HighlightMode`, `NoAutoBright`, `ChromaNoiseReduction`).
- **Verification CLI (`test_decoder.exe`):** Standalone WIC test tool (statically linked CRT) that decodes `.CR3` files strictly through Windows WIC (`IWICImagingFactory`), prints the selected decoder CLSID/stats, and exports PNGs.

### Downloads & Installation
- **`CR3WICCodec-Setup-x64-v1.0.0.exe`** *(Recommended)* — Full Windows setup wizard with clean uninstallation via **Windows Settings → Apps**.
- **`CR3WICCodec-x64-portable.zip`** — Portable package containing `Install.exe` and `Uninstall.exe` (one-click UAC elevation, no PowerShell needed) plus `test_decoder.exe` and manual PowerShell scripts.

**Requirements:** Windows 10 (1809+) or Windows 11 (x64) and [Microsoft Visual C++ 2015–2022 Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe).
