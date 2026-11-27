# Release Notes (Plain English)

Short, easy-to-read release descriptions used by GitHub Releases when a new version tag (`v*`) is pushed.  
For full technical details, see [CHANGELOG.md](CHANGELOG.md).

## v1.5.1

Reliability and language update that replaces Inno Setup with a single native Windows installer, making installation smoother and avoiding common antivirus blocks.

### What's New
- **New single-file Setup (`.exe`):** Installs directly from memory with no temporary helper files in `%TEMP%`, avoiding false-positive blocks from antivirus tools.
- **Simple install choice:** Pick **Install for me only** (no admin rights needed) or **Install for all users** (admin), with automatic architecture check (`x64` vs. `ARM64`), automatic VC++ setup if missing, and a clean **Windows Settings → Apps** entry.
- **Proper German umlauts:** Fixed German dialogs across the installer, uninstaller, and portable tools (`ä, ö, ü, ß`).
- **Cleaner release notes:** Added a single shared download and install guide template for all releases.

**Full Changelog:** [`v1.5.0...v1.5.1`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.5.0...v1.5.1)

## v1.5.0

Adds installation without administrator rights, a custom app icon, and a cleaner setup flow.

### What's New
- **No admin rights required:** Standard Windows user accounts can now install the codec directly for their own user profile (`%LocalAppData%\Programs\CR3WICCodec`).
- **Custom app icon:** Added a camera + `.CR3` icon across the installer and setup tools.
- **Fewer setup prompts:** Automatically picks German or English based on your Windows language and overwrites older installs without extra popups.

**Full Changelog:** [`v1.4.0...v1.5.0`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.4.0...v1.5.0)

## v1.4.0

Adds native ARM64 support and automatic Visual C++ runtime installation.

### What's New
- **Windows ARM64 support:** Added native builds and installers for Snapdragon / Copilot+ Windows 11 PCs alongside standard x64 PCs.
- **Automatic VC++ setup:** Setup and `Install.exe` now automatically download and install the required Microsoft Visual C++ Redistributable if it is missing.

**Full Changelog:** [`v1.3.1...v1.4.0`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.3.1...v1.4.0)

## v1.3.1

Small packaging and updater fix release.

### What's New
- Bundled the PowerShell helper and updater scripts (`scripts/`) inside both installers so `update.ps1` is always included.
- Fixed a self-update path check when running `update.ps1` directly from the install folder.

**Full Changelog:** [`v1.3.0...v1.3.1`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.3.0...v1.3.1)

## v1.3.0

Speed, metadata, and rotation improvements across Explorer and photo viewers.

### What's New
- **Faster browsing on NAS & SD cards:** Added a multi-block read cache so folder metadata loads much faster over network shares.
- **Faster previews:** Cached embedded JPEG preview offsets for instant preview pane loading and fast thumbnail mode.
- **More camera details in Explorer:** Added exposure bias, flash, white balance, sub-second time, copyright, camera star ratings, and GPS coordinates.
- **Full rotation & flip support:** Properly handles rotated and flipped previews in Windows viewers.

**Full Changelog:** [`v1.2.0...v1.3.0`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.2.0...v1.3.0)

## v1.2.0

Adds camera EXIF details in Windows Explorer, fast preview scaling, and optional 16-bit / wide-gamut output.

### What's New
- **EXIF details in Explorer:** Shows camera model, lens, ISO, shutter speed, aperture, focal length, and capture date in Windows Explorer.
- **Faster previews & scaling:** Added dedicated preview support and smart downscaling without decoding the full RAW first.
- **16-bit & wide-gamut options:** Optional registry settings for 16-bit color depth and Adobe RGB / ProPhoto / Wide Gamut / ACES color spaces.
- **Download verification:** Added `SHA256SUMS.txt` checksums to every release.

**Full Changelog:** [`v1.1.0...v1.2.0`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.1.0...v1.2.0)

## v1.1.0

Performance and speed improvements for browsing large folders.

### What's New
- **Less memory copying & smoother scrolling:** Reuses already-decoded images in memory when generating thumbnails and improves multi-threaded loading.
- **Optional fast thumbnail mode:** Added a `FastEmbeddedThumb` registry option to use the camera's built-in JPEG for maximum folder speed.
- **Consistent versioning & automated CI checks:** Synced version info across files and added automated sample-photo tests on every build.

**Full Changelog:** [`v1.0.2...v1.1.0`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.0.2...v1.1.0)

## v1.0.2

Adds in-place updating without restarting Windows Explorer.

### What's New
- Added `update.ps1` to check and install new GitHub releases automatically.
- Safe in-place updates even if Windows Explorer currently holds the codec DLL open.

**Full Changelog:** [`v1.0.1...v1.0.2`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.0.1...v1.0.2)

## v1.0.1

Adds German and English language detection.

### What's New
- `Install.exe`, `Uninstall.exe`, and the PowerShell scripts now automatically show German text on German Windows and English text elsewhere.

**Full Changelog:** [`v1.0.0...v1.0.1`](https://github.com/felix-henry/windows-cr3-raw-codec/compare/v1.0.0...v1.0.1)

## v1.0.0

First release of the native Canon `.CR3` RAW decoder for Windows 10 and 11.

### What's New
- **True 14-bit RAW colors:** Replaces broken or pink Microsoft `.CR3` previews in Windows Explorer, preview pane, Paint, and classic photo viewers using **LibRaw**.
- **Broad camera support:** Works with modern Canon `.CR3` bodies including EOS R6 Mark III, R5 Mark II, R1, R3, R5, R6 / R6 II, R7, R8, R10, R50, R100, and PowerShot / EOS M models.
- **Easy install:** Includes both a guided setup installer and a portable ZIP with one-click `Install.exe` / `Uninstall.exe`.

## Release Downloads Template

### Which Download Should I Pick?

- **Normal Windows PCs & laptops (Intel / AMD):** **`CR3WICCodec-Setup-x64-{{TAG}}.exe`** *(Recommended for most users)*
- **Snapdragon / ARM64 Windows 11 laptops:** **`CR3WICCodec-Setup-arm64-{{TAG}}.exe`**

### Downloads Overview ({{TAG}})

| File | Best for | What it includes |
| :--- | :--- | :--- |
| **`CR3WICCodec-Setup-x64-{{TAG}}.exe`** | **Most Windows PCs (Intel / AMD)** | Guided single-file installer (`v{{VERSION}}`). Works with or without admin rights, auto-detects language (DE/EN), installs VC++ runtimes if missing, and adds an entry in **Windows Settings → Apps**. |
| **`CR3WICCodec-Setup-arm64-{{TAG}}.exe`** | **Snapdragon / ARM64 PCs** | Same guided installer built natively for Windows 11 ARM64. |
| **`CR3WICCodec-x64-portable.zip`** | **Portable / manual setup (Intel / AMD)** | Standalone ZIP with the codec files, `Install.exe`, `Uninstall.exe`, and PowerShell scripts. |
| **`CR3WICCodec-arm64-portable.zip`** | **Portable / manual setup (ARM64)** | Same portable ZIP built natively for Windows 11 ARM64. |
| **`SHA256SUMS.txt`** | **Optional verification** | Checksums to verify your downloaded files. |

### How to Install (`.exe` vs. `.zip`)

- **Using the Setup Installer (`.exe` — Recommended):**
  1. Download the `.exe` for your PC (`x64` or `arm64`) and double-click it *(if Windows SmartScreen shows up, click **More info → Run anyway**)*.
  2. Choose **Install for me only** (no admin rights needed) or **Install for all users** (admin).
  3. Uninstall anytime in **Windows Settings → Apps**.
- **Using the Portable ZIP (`.zip`):**
  1. Unzip the entire archive into a folder first (don't run files straight from the ZIP preview).
  2. Double-click **`Install.exe`** (or run `scripts\install.ps1` in an Administrator PowerShell).
  3. Uninstall anytime via **`Uninstall.exe`** or **Windows Settings → Apps**.

> **Tip:** If old pink thumbnails still show up after installing, run **Disk Cleanup (`cleanmgr.exe`)** on `C:`, select only **Thumbnails**, and clear the cache.
