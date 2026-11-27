# Changelog

All notable changes to **Canon CR3 Native WIC Codec** are documented in this file.
On tagged releases (`v*`), GitHub Actions automatically extracts the matching `## vX.Y.Z` section together with the dynamic `## Release Downloads Template` below as the GitHub Release body.

## v1.5.1

Reliability & localization release replacing the two-stage Inno Setup wrapper with a **self-contained native single-EXE installer** (eliminating `%TEMP%\*.tmp` `ShellExecuteEx code 5` antivirus/Defender blocks on Windows 11), fixing German umlauts across all native dialogs, and introducing a single dynamic Release Downloads template in `CHANGELOG.md`.

### What's New in v1.5.1

- **Native Single-EXE `CR3WICCodec-Setup-*.exe` (`tools/setup_app.cpp`)**
  - Replaced Inno Setup (`ISCC.exe`) with a self-contained native C++ Win32 installer that decompresses its embedded Windows-LZMS payload (`scripts/pack_payload.ps1` + `Cabinet.dll`) directly in memory—no temporary `.tmp` executable is unpacked or launched in `%TEMP%`.
  - Features a native Windows `TaskDialog` install-mode selector (*Install for me only / no admin* vs. *Install for all users / Admin* with UAC shield icon), host architecture verification (`x64` vs. `ARM64`), lock-safe in-place DLL updates, automatic Microsoft VC++ Redistributable download/install, and full **Windows Settings → Apps** uninstall registration (migrating legacy Inno Setup uninstall keys cleanly).
- **German Umlaut Localization & Shared Installer Core**
  - Fixed all German UI dialogs in `Setup.exe`, `Install.exe`, and `Uninstall.exe` to use native UTF-8/UTF-16 umlauts (`ä, ö, ü, ß`) instead of ASCII transcriptions (`ae/oe/ue/ss`).
  - Consolidated VC++ check/install, lock-safe file replacement, registry/Apps registration, and architecture guards into shared headers (`tools/InstallCore.h`, `tools/SetupCommon.h`).
- **Dynamic GitHub Release Template (`.github/workflows/build-release.yml` & `CHANGELOG.md`)**
  - Moved the Downloads overview and per-method installation guide into one reusable `## Release Downloads Template` section in `CHANGELOG.md` using `{{TAG}}` / `{{VERSION}}` placeholders substituted automatically during GitHub Releases.

**Full Changelog:** [`v1.5.0...v1.5.1`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.5.0...v1.5.1)

## v1.5.0

Installer UX & non-admin accessibility release introducing **per-user (standard account) installation without Administrator privileges**, a **custom `.CR3` camera icon** across all executables, automatic Windows UI language detection, and frictionless in-place upgrades.

### What's New in v1.5.0

- **Per-User (`HKCU`) Installation Without Admin Rights**
  - **`CR3WICCodec.dll` (`Registry.cpp` / `DllMain.cpp` / `CR3Codec.def`):** Parameterized COM/WIC registration across `HKEY_LOCAL_MACHINE` and `HKEY_CURRENT_USER`, adding exported `DllInstall` support (`regsvr32 /s /n /i:user` and `/u /s /n /i:user`).
  - **Setup Installer (`CR3WICCodec.iss`):** Switched from forced `PrivilegesRequired=admin` to `PrivilegesRequired=lowest` with `PrivilegesRequiredOverridesAllowed=dialog` and `{autopf}\CR3WICCodec`, letting standard non-admin Windows accounts install directly into `%LocalAppData%\Programs\CR3WICCodec` (or choose system-wide admin installation), while triggering standard UAC elevation via `ShellExec` only if Microsoft's VC++ Redistributable actually needs to be installed.
  - **Portable `Install.exe` & `Uninstall.exe` (`install_app.cpp` / `uninstall_app.cpp` / `SetupCommon.h`):** Switched from mandatory `requireAdministrator` manifest to `asInvoker` with automatic elevation detection—standard users install cleanly into `%LocalAppData%\Programs\CR3WICCodec` + `HKCU`, and `Uninstall.exe` requests elevation only when cleaning up an existing machine-wide `HKLM` install.
- **Streamlined Setup Wizard & Custom `.CR3` Application Icon**
  - Set `ShowLanguageDialog=no` (uses Windows UI language automatically: German or English) and `DirExistsWarning=no` (removes the redundant "folder already exists" confirmation when upgrading or reinstalling).
  - Added multi-resolution `assets/CR3WICCodec.ico` (16×16 through 256×256) and embedded it via `SetupIconFile` / `UninstallDisplayIcon` and `tools/Tools.rc` into Setup, `Install.exe`, `Uninstall.exe`, and `test_decoder.exe`.

### Installation & Upgrading

- **Fresh Install or Upgrade (Recommended)**: Download **`CR3WICCodec-Setup-x64-v1.5.0.exe`** (standard Intel/AMD Windows 10/11 PCs) or **`CR3WICCodec-Setup-arm64-v1.5.0.exe`** (Snapdragon / ARM64 Windows 11 PCs). Supports both **standard user accounts** (no admin rights needed when VC++ runtime is present) and **all-users admin installs**, overwriting previous versions cleanly.
- **Portable**: Download **`CR3WICCodec-x64-portable.zip`** or **`CR3WICCodec-arm64-portable.zip`**, extract to a permanent folder, and run `Install.exe`.
- **CLI Updater (Admin PowerShell, system-wide installs)**:
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File "C:\Program Files\CR3WICCodec\scripts\update.ps1" -FromGitHub
  ```

### Downloads

| File | Description |
| :--- | :--- |
| **`CR3WICCodec-Setup-x64-v1.5.0.exe`** | Windows x64 Setup Wizard (admin or per-user, auto DE/EN, custom `.CR3` icon, auto VC++ check/install) |
| **`CR3WICCodec-Setup-arm64-v1.5.0.exe`** | Native Windows ARM64 Setup Wizard (admin or per-user, auto DE/EN, custom `.CR3` icon, auto VC++ check/install) |
| **`CR3WICCodec-x64-portable.zip`** | Portable Windows x64 package (`CR3WICCodec.dll`, `libraw.dll`, `vcomp140.dll`, `test_decoder.exe`, `Install.exe`, `Uninstall.exe`, `scripts/`) |
| **`CR3WICCodec-arm64-portable.zip`** | Portable native Windows ARM64 package (`CR3WICCodec.dll`, `libraw.dll`, `vcomp140.dll`, `test_decoder.exe`, `Install.exe`, `Uninstall.exe`, `scripts/`) |
| **`SHA256SUMS.txt`** | Unified SHA-256 checksums for all x64 and ARM64 installers & archives |

**Full Changelog:** [`v1.4.0...v1.5.0`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.4.0...v1.5.0)

## v1.4.0

Platform & onboarding release introducing **native Windows ARM64 builds** (for Snapdragon / Copilot+ Windows 11 PCs) alongside **automatic Visual C++ Redistributable download & installation** right inside Setup and `Install.exe`.

### What's New in v1.4.0

- **Native Windows 11 ARM64 Support**
  - GitHub Actions now builds and tests both **x64** (`windows-latest`) and native **ARM64** (`windows-11-arm`) toolchains—including native ARM64 `libraw.dll` with OpenMP, `CR3WICCodec.dll`, `Install.exe`, `Uninstall.exe`, `test_decoder.exe`, and Inno Setup installers—with automated WIC smoke tests on native hardware for both architectures.
  - `scripts/build.ps1` now supports `-Arch auto|x64|arm64`, and `scripts/update.ps1` automatically selects `*arm64*portable*.zip` on ARM64 Windows vs. `*x64*portable*.zip` on x64 Windows.
- **Automatic Microsoft VC++ Redistributable Download & Installation**
  - **Setup Installer (`CR3WICCodec.iss`):** If the Microsoft Visual C++ 2015–2022 Redistributable (`x64` or `ARM64`) is missing, the Setup wizard now downloads the official Microsoft installer directly via a built-in download page and installs it (`/install /passive /norestart`) in `PrepareToInstall` before registering `CR3WICCodec.dll` (with browser fallback if offline).
  - **Portable `Install.exe` (`install_app.cpp`):** Offers one-click automatic download (`URLDownloadToFileW`) and installation of `vc_redist.x64.exe` / `vc_redist.arm64.exe`, then proceeds seamlessly with codec installation.

### Installation & Upgrading

- **Upgrade from an earlier version** directly in an Administrator PowerShell (selects `x64` / `arm64` automatically):
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File "C:\Program Files\CR3WICCodec\scripts\update.ps1" -FromGitHub
  ```
- **Fresh Install (Recommended)**: Download **`CR3WICCodec-Setup-x64-v1.4.0.exe`** (Windows x64) or **`CR3WICCodec-Setup-arm64-v1.4.0.exe`** (Windows ARM64). Missing VC++ runtimes are installed automatically.
- **Portable**: Download **`CR3WICCodec-x64-portable.zip`** or **`CR3WICCodec-arm64-portable.zip`**, extract to a permanent folder, and run `Install.exe`.

### Downloads

| File | Description |
| :--- | :--- |
| **`CR3WICCodec-Setup-x64-v1.4.0.exe`** | Windows x64 Setup Wizard (DE/EN) with automatic VC++ runtime download/install & Uninstaller entry |
| **`CR3WICCodec-Setup-arm64-v1.4.0.exe`** | Native Windows ARM64 Setup Wizard (DE/EN) for Snapdragon / Copilot+ PCs with automatic VC++ runtime download/install |
| **`CR3WICCodec-x64-portable.zip`** | Portable Windows x64 package (`CR3WICCodec.dll`, `libraw.dll`, `vcomp140.dll`, `test_decoder.exe`, `Install.exe`, `Uninstall.exe`, `scripts/`) |
| **`CR3WICCodec-arm64-portable.zip`** | Portable native Windows ARM64 package (`CR3WICCodec.dll`, `libraw.dll`, `vcomp140.dll`, `test_decoder.exe`, `Install.exe`, `Uninstall.exe`, `scripts/`) |
| **`SHA256SUMS.txt`** | Unified SHA-256 checksums for all x64 and ARM64 installers & archives |

**Full Changelog:** [`v1.3.1...v1.4.0`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.3.1...v1.4.0)

## v1.3.1

Maintenance & packaging polish ensuring the PowerShell updater (`scripts\update.ps1`) is always bundled by the Setup installer and `Install.exe`, fixing self-update detection when invoked from the install directory, and cleaning up fallback pixel-copy code.

### What's Changed in v1.3.1

- **Bundle `scripts\` in Setup & `Install.exe` (`CR3WICCodec.iss` & `install_app.cpp`)**
  - Inno Setup (`CR3WICCodec-Setup-x64-v1.3.1.exe`) and the portable `Install.exe` now install `scripts\update.ps1`, `scripts\install.ps1`, and `scripts\uninstall.ps1` into `C:\Program Files\CR3WICCodec\scripts\`.
  - `Install.exe` now also uses lock-safe `MoveFileExW` replacement for optional DLLs like `vcomp140.dll`.
- **Self-Directory Fix in `scripts/update.ps1`**
  - Prevented `update.ps1` from mistaking the active `C:\Program Files\CR3WICCodec` installation folder for a local update source when run without `-FromGitHub`.
- **Code & Documentation Cleanup**
  - Consolidated the 8-bit/16-bit multi-channel fallback copy path in `LibRawProcessor.cpp` into a single `CopyMemImageChannelsToBGR<SampleT>` template helper.
  - Updated `README.md` with the exact installed `scripts\update.ps1` command and a concise summary of `v1.3.x` capabilities.

### Installation & Upgrading

- **Upgrade from an earlier version** directly in an Administrator PowerShell:
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File "C:\Program Files\CR3WICCodec\scripts\update.ps1" -FromGitHub
  ```
- **Fresh Install (Recommended)**: Download and run **`CR3WICCodec-Setup-x64-v1.3.1.exe`** below.
- **Portable**: Download **`CR3WICCodec-x64-portable.zip`**, extract it to a permanent folder, and run `Install.exe` (or `scripts\install.ps1` as Administrator).

### Downloads

| File | Description |
| :--- | :--- |
| **`CR3WICCodec-Setup-x64-v1.3.1.exe`** | Windows Installer (DE/EN) with automatic elevation, Explorer thumbnail cache refresh, and Windows Uninstaller entry |
| **`CR3WICCodec-x64-portable.zip`** | Portable archive containing `CR3WICCodec.dll`, `libraw.dll`, `vcomp140.dll`, `test_decoder.exe`, `Install.exe`, `Uninstall.exe`, `scripts\update.ps1`, and PowerShell helpers |
| **`SHA256SUMS.txt`** | SHA-256 checksums for verifying the setup executable and portable ZIP |

**Full Changelog:** [`v1.3.0...v1.3.1`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.3.0...v1.3.1)

## v1.3.0

I/O, metadata, and WIC compliance release adding a **4× 64 KB LRU stream cache**, zero-reparse **embedded JPEG offset caching** for previews and fast thumbnails, **extended EXIF / GPS / Rating metadata**, and full **`IWICBitmapSourceTransform`** rotation/flip support.

### What's New in v1.3.0

- **4× 64 KB LRU Stream Cache (`WicIStreamDatastream`)**
  - Replaced the single linear 64 KB read buffer with a 4-slot (256 KB total) aligned LRU block cache so random-access ISOBMFF/EXIF/CRX header reads no longer drop cached blocks on seeks—noticeably faster on NAS shares and SD cards, while keeping contiguous reads `≥ 128 KB` zero-copy bypassed.
- **Cached Embedded JPEG Offset for `GetPreview()` & `FastEmbeddedThumb`**
  - `LibRawProcessor::ReadHeader` now caches the stream offset, byte length, and orientation of the embedded CR3 JPEG preview (`imgdata.thumbs_list`).
  - `CR3Decoder::GetPreview()` and `FastEmbeddedThumb = 1` read and decode that JPEG directly from `IStream` via `DecodeEmbeddedJpegFromStream` without re-instantiating `LibRaw`, falling back seamlessly to LibRaw if unavailable.
- **Extended EXIF, GPS & XMP Rating Metadata (`GetMetadataQueryReader` / `IWICMetadataBlockReader`)**
  - Added an `exif_parser_callback` in `ReadHeader` and GPS/XMP extraction to expose:
    - `ExposureBiasValue` (`/ifd/exif/{ushort=37380}`, EV `VT_I8`)
    - `Flash` (`/ifd/exif/{ushort=37385}`) & `WhiteBalance` (`/ifd/exif/{ushort=41987}`)
    - `SubSecTimeOriginal` (`/ifd/exif/{ushort=37521}`) & `Copyright` (`/ifd/{ushort=33432}`)
    - In-camera `xmp:Rating` mapped to Windows Explorer star ratings (`/ifd/{ushort=18246}` & `{ushort=18249}`)
    - Full GPS IFD (`/ifd/gps/{ushort=1..6}`: Latitude/Longitude + Ref, Altitude + Ref) when present in `.CR3`.
- **Complete `IWICBitmapSourceTransform` Support (All Rotations & Flips)**
  - `CR3FrameDecode::DoesSupportTransform` and `CopyPixels` now support all standard `WICBitmapTransformOptions` (`Rotate0/90/180/270`, `FlipHorizontal`, `FlipVertical`, and combinations) in canonical WIC order (**Scale → Clip → Flip/Rotate**).

### Verified Checks

- Built cleanly (`CR3WICCodec.dll` `1.3.0.0`) and verified with `test_decoder.exe --thumb --meta --preview --transform --dll` on **Canon EOS R6 Mark III** (`.CR3`):
  - `QueryCapability`: `hr=0x00000000`, `caps=0x12`
  - `Metadata Blocks: 1` (`Canon EOS R6 Mark III`, `RF50mm F1.4 L VCM`, ISO `100`, `f/7.1`, `1/20 s`, `50.0 mm`, `+0.00 EV`, Flash `0x0`, WB `Auto`, SubSec `82`, Rating `0/5`)
  - `Preview Dimensions`: `1620 x 1080` · `Transform Scaled`: `1200 x 801` · `Transform Rotate90`: `801 x 1200 (OK)`

### Installation & Upgrading

- **Upgrade from an earlier version** directly in PowerShell:
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File "C:\Program Files\CR3WICCodec\update.ps1" -FromGitHub
  ```
- **Fresh Install (Recommended)**: Download and run **`CR3WICCodec-Setup-x64-v1.3.0.exe`** below.
- **Portable**: Download **`CR3WICCodec-x64-portable.zip`**, extract it to a permanent folder, and run `Install.exe` (or `install.ps1` as Administrator).

### Downloads

| File | Description |
| :--- | :--- |
| **`CR3WICCodec-Setup-x64-v1.3.0.exe`** | Windows Installer (DE/EN) with automatic elevation, Explorer thumbnail cache refresh, and Windows Uninstaller entry |
| **`CR3WICCodec-x64-portable.zip`** | Portable archive containing `CR3WICCodec.dll`, `libraw.dll`, `vcomp140.dll`, `test_decoder.exe`, `Install.exe`, `Uninstall.exe`, `update.ps1`, and PowerShell helpers |
| **`SHA256SUMS.txt`** | SHA-256 checksums for verifying the setup executable and portable ZIP |

**Full Changelog:** [`v1.2.0...v1.3.0`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.2.0...v1.3.0)

## v1.2.0

Major WIC integration and feature release adding **EXIF metadata reading** for Windows Explorer & Photo viewers, fast **`IWICBitmapSourceTransform`** & **`GetPreview()`** support, optional **16-bit (`48bppBGR`) / wide-gamut color spaces**, faster ISOBMFF stream probing, and **SHA256 release verification**.

### What's New in v1.2.0

- **EXIF & Camera Metadata in Windows (`GetMetadataQueryReader` & `IWICMetadataBlockReader`)**
  - Implemented `IWICBitmapDecoder::GetMetadataQueryReader`, `IWICBitmapFrameDecode::GetMetadataQueryReader`, and `IWICMetadataBlockReader` backed by Windows' native TIFF/EXIF metadata engine.
  - Exposes Camera Make/Model, Body Serial, Lens Make/Model/Serial, ISO, Shutter Speed, Aperture (`f/`), Focal Length (incl. 35mm eq.), Exposure Program, Metering Mode, `DateTimeOriginal`, Software, Artist, Description, and Color Space directly to Windows Explorer Details / InfoTips and WIC apps.
- **Fast Previews & Scaling (`GetPreview` & `IWICBitmapSourceTransform`)**
  - Implemented `CR3Decoder::GetPreview()` using the embedded camera JPEG preview (with automatic half-size RAW fallback).
  - Implemented `IWICBitmapSourceTransform` (`DoesSupportTransform`, `GetClosestSize`, `GetClosestPixelFormat`, `CopyPixels`) on `CR3FrameDecode`, routing scaled requests ≤ half RAW size through the fast half-size RAW buffer instead of decoding full resolution first.
- **Optional 16-Bit (`48bppBGR`) & Wide-Gamut Output (`HKLM\SOFTWARE\CR3WICCodec`)**
  - Registered `GUID_WICPixelFormat48bppBGR` alongside `GUID_WICPixelFormat24bppBGR`.
  - Added `OutputBps` (`DWORD`, default `8`; set `16` for 16-bit `48bppBGR`) and `OutputColorSpace` (`DWORD`, default `1` = sRGB; `2` = Adobe RGB, `3` = Wide Gamut, `4` = ProPhoto RGB, `5` = XYZ, `6` = ACES) with matching `GetColorContexts` reporting.
- **Faster `ProbeStream` & Orientation Polish**
  - `LibRawProcessor::ProbeStream` now validates the `.CR3` ISOBMFF `ftypcrx ` + `moov` box structure directly instead of initializing `LibRaw` twice per open.
  - `TryDecodeEmbeddedJpegThumb` now checks the embedded JPEG's own EXIF orientation before falling back to `sizes.flip` and guards COM initialization on worker threads (`ComInitGuard`).
- **Automated Release Checksums & CMake ARM64 Readiness**
  - `scripts/build.ps1 -Package` and GitHub Actions now publish `SHA256SUMS.txt`; `scripts/update.ps1` verifies downloaded portable ZIPs against `SHA256SUMS.txt` before installing.
  - `CMakeLists.txt` generalizes 64-bit platform checks and OpenMP runtime lookup for both x64 and ARM64 toolchains.

### Verified Checks

- Built cleanly (`CR3WICCodec.dll` `1.2.0.0`) and verified with `test_decoder.exe --thumb --meta --preview --transform --dll` on **Canon EOS R6 Mark III** (`.CR3`):
  - `QueryCapability`: `hr=0x00000000`, `caps=0x12`
  - `Metadata Blocks: 1` (`Canon EOS R6 Mark III`, `RF50mm F1.4 L VCM`, ISO `100`, `f/7.1`, `1/20 s`, `50.0 mm`, `2026:01:22 15:41:04`)
  - `Preview Dimensions`: `6960 x 4640` · `Transform Scaled`: `1200 x 801 (closest native 3493 x 2332)`

### Installation & Upgrading

- **Upgrade from an earlier version** directly in PowerShell:
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File "C:\Program Files\CR3WICCodec\update.ps1" -FromGitHub
  ```
- **Fresh Install (Recommended)**: Download and run **`CR3WICCodec-Setup-x64-v1.2.0.exe`** below.
- **Portable**: Download **`CR3WICCodec-x64-portable.zip`**, extract it to a permanent folder, and run `Install.exe` (or `install.ps1` as Administrator).

### Downloads

| File | Description |
| :--- | :--- |
| **`CR3WICCodec-Setup-x64-v1.2.0.exe`** | Windows Installer (DE/EN) with automatic elevation, Explorer thumbnail cache refresh, and Windows Uninstaller entry |
| **`CR3WICCodec-x64-portable.zip`** | Portable archive containing `CR3WICCodec.dll`, `libraw.dll`, `vcomp140.dll`, `test_decoder.exe`, `Install.exe`, `Uninstall.exe`, `update.ps1`, and PowerShell helpers |
| **`SHA256SUMS.txt`** | SHA-256 checksums for verifying the setup executable and portable ZIP |

**Full Changelog:** [`v1.1.0...v1.2.0`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.1.0...v1.2.0)

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

## Release Downloads Template

### Which Download Should I Pick?

- **Normal Windows 10/11 PCs & Laptops (Intel / AMD):** Download **`CR3WICCodec-Setup-x64-{{TAG}}.exe`** *(Recommended for 99% of users)*
- **Snapdragon / Copilot+ ARM64 Laptops:** Download **`CR3WICCodec-Setup-arm64-{{TAG}}.exe`**

### Downloads Overview ({{TAG}})

| File | Who is it for? | What do you get? |
| :--- | :--- | :--- |
| **`CR3WICCodec-Setup-x64-{{TAG}}.exe`** | **Most Windows PCs (Intel / AMD)** | Self-contained native installer (`v{{VERSION}}`) with no `%TEMP%` `.tmp` unpacker, per-user (no admin) or all-users mode, auto DE/EN language, custom `.CR3` icon, auto VC++ check/install, and **Windows Settings → Apps** entry. |
| **`CR3WICCodec-Setup-arm64-{{TAG}}.exe`** | **ARM64 / Snapdragon Windows 11 PCs** | Same self-contained native installer natively compiled for Windows ARM64 hardware (not for Intel/AMD PCs). |
| **`CR3WICCodec-x64-portable.zip`** | **Power users (Intel / AMD)** | Standalone ZIP archive containing `CR3WICCodec.dll`, `libraw.dll`, `vcomp140.dll`, `Install.exe`, `Uninstall.exe`, `test_decoder.exe`, and `scripts/`. |
| **`CR3WICCodec-arm64-portable.zip`** | **Power users (ARM64 / Snapdragon)** | Same portable ZIP archive natively compiled for ARM64 Windows 11 PCs. |
| **`SHA256SUMS.txt`** | **Verification (optional)** | Unified SHA-256 checksums for all `{{TAG}}` `.exe` installers and `.zip` archives. |

### How to Install (`.exe` Setup vs. `.zip` Portable)

- **Via Setup Installer (`CR3WICCodec-Setup-*-{{TAG}}.exe` — Recommended):**
  1. Download the `.exe` matching your PC (`x64` for standard Intel/AMD PCs, or `arm64` for Snapdragon PCs) and double-click it *(if Windows SmartScreen appears, click **More info → Run anyway**)*.
  2. Choose **Install for me only** (no administrator rights needed; installs to `%LocalAppData%\Programs\CR3WICCodec`) or **Install for all users** (administrator; installs to `C:\Program Files\CR3WICCodec`).
  3. Missing Visual C++ 2015–2022 runtimes are downloaded and installed automatically if needed. Uninstall anytime via **Windows Settings → Apps**.
- **Via Portable ZIP (`CR3WICCodec-*-portable.zip`):**
  1. Extract the **entire ZIP archive** into a folder first (do not run files directly from inside the Windows ZIP preview).
  2. Double-click **`Install.exe`** (automatically uses system-wide mode when run as Administrator, or per-user mode without admin rights) — or run `scripts\install.ps1` in an Administrator PowerShell.
  3. Remove anytime by double-clicking **`Uninstall.exe`** or via **Windows Settings → Apps**.

> **Tip after installing or upgrading:** If previously cached pink/blank `.CR3` thumbnails stay visible in Windows Explorer, open **Disk Cleanup (`cleanmgr.exe`)** on `C:`, check **only "Thumbnails"**, and clear the cache.
