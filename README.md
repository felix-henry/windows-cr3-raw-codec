# Canon CR3 Native WIC Codec (Windows 10 / 11)

[![Build and Release Windows (x64 & ARM64)](https://github.com/felix-henry/windows-cr3-raw-codec/actions/workflows/build-release.yml/badge.svg)](https://github.com/felix-henry/windows-cr3-raw-codec/actions/workflows/build-release.yml)

View Canon `.CR3` RAW photos in Windows 10 and 11 with true 14-bit colors—fixing the pink tint and missing previews caused by outdated built-in Windows decoders.

- **Works out of the box:** File Explorer thumbnails, preview pane (`Alt + P`), classic Windows photo viewers, and camera details (EXIF, lens, GPS, and star ratings).
- **Fast & accurate:** Powered by **LibRaw** with multi-core processing and smart caching for folders, SD cards, and network drives.
- **Flexible:** Optional settings for 16-bit output, wide-gamut color spaces, or ultra-fast embedded JPEG thumbnails (see the [Wiki](https://github.com/felix-henry/windows-cr3-raw-codec/wiki)).

---

## Before vs. After (Canon EOS R6 Mark III)

| Built-in Windows Decoder (Before) | Canon CR3 WIC Codec (After) |
| :---: | :---: |
| ![Before - Microsoft Raw Image Decoder pink tint](docs/images/before-microsoft-raw.jpg) | ![After - Canon CR3 WIC Codec natural colors](docs/images/after-cr3-wic-codec.jpg) |
| *Outdated camera support → heavy pink cast* | *True 14-bit RAW colors & camera white balance* |

<sub>Sample RAW: [Canon EOS R6 Mark III (.CR3, CC0 Public Domain)](docs/images/canon-eos-r6-mark-iii-sample.CR3) via [raw.pixls.us](https://raw.pixls.us/)</sub>

---

## Download & Install

**[Download Latest Release](https://github.com/felix-henry/windows-cr3-raw-codec/releases/latest)** *(Windows 10 / 11)*

**Which file should I download?**
- **Most Windows PCs & laptops (Intel / AMD):** **`CR3WICCodec-Setup-x64-v*.exe`** *(Recommended)*
- **Snapdragon / ARM64 Windows 11 laptops:** **`CR3WICCodec-Setup-arm64-v*.exe`**

### Ways to Install
1. **Setup Installer (`.exe` — Recommended):** Run the installer and pick **Install for me only** (no admin rights needed) or **Install for all users** (admin). Missing Visual C++ runtime files are downloaded automatically. Uninstall anytime via **Windows Settings → Apps**.
2. **Portable ZIP (`.zip`):** Unzip all files into a folder and double-click **`Install.exe`** (automatically installs for your user or all users if run as admin). Double-click **`Uninstall.exe`** or use **Windows Settings → Apps** to remove.
3. **Updating:** Simply run the newest `.exe` installer over your existing installation—or run `scripts\update.ps1 -FromGitHub` in an Administrator PowerShell for system-wide installs.

> **Old pink thumbnails still showing?** Open **Disk Cleanup (`cleanmgr.exe`)** on drive `C:`, select only **Thumbnails**, and clear the cache.

**Requirements:** Windows 10 (1809+) or Windows 11. If installing offline without Visual C++ 2015–2022 installed, download it first for [x64](https://aka.ms/vs/17/release/vc_redist.x64.exe) or [ARM64](https://aka.ms/vs/17/release/vc_redist.arm64.exe).

---

## Supported Cameras

Supports **all Canon `.CR3` cameras** supported by LibRaw, including:
- **EOS R series:** EOS R, RP, R1, R3, R5, R5 C, R5 Mark II, R6, R6 Mark II, **R6 Mark III**, R7, R8, R10, R50, R100
- **DSLR / EOS M / PowerShot:** EOS 90D, 250D, 1D X Mark III, M50 / M50 II, M6 II, PowerShot G5 X II / G7 X III / V1

---

## More Information

- **[Wiki — Troubleshooting, Registry Settings & Technical Details](https://github.com/felix-henry/windows-cr3-raw-codec/wiki)**

---

<div align="center">

[![Website](https://img.shields.io/badge/Website-felixhenry.de-111827?style=for-the-badge&labelColor=1f2937&color=0284c7)](https://felixhenry.de)
[![Latest Release](https://img.shields.io/github/v/release/felix-henry/windows-cr3-raw-codec?style=for-the-badge&label=Download&color=16a34a)](https://github.com/felix-henry/windows-cr3-raw-codec/releases/latest)
[![Wiki](https://img.shields.io/badge/Docs-Wiki-6366f1?style=for-the-badge)](https://github.com/felix-henry/windows-cr3-raw-codec/wiki)

**Built for Canon photographers on Windows 10 / 11** · **[felixhenry.de](https://felixhenry.de)**

<sub>[MIT License](LICENSE) · Powered by [LibRaw](https://www.libraw.org/) (LGPL 2.1 / CDDL 1.0) · [Third-Party Notices](THIRD_PARTY_NOTICES.md)</sub>  
<sub>Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org)</sub>

</div>
