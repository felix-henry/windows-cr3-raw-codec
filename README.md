# Canon CR3 Native WIC Codec (Windows 10 / 11)

View Canon `.CR3` RAW photos natively in Windows 10 and 11 with accurate 14-bit colors—fixing the pink tint and missing previews caused by outdated built-in Windows decoders.

- **Works everywhere in Windows:** File Explorer thumbnails, preview pane (`Alt + P`), classic photo viewers, and camera metadata (EXIF, lens, GPS, and star ratings).
- **Fast & reliable:** Powered by **LibRaw** with multi-core decoding and smart caching for local drives, SD cards, and NAS folders.
- **Customizable:** Optional settings for 16-bit output, wide-gamut color spaces, or instant embedded JPEG thumbnails (see the [Wiki](https://github.com/felix-henry/windows-cr3-raw-codec/wiki)).

---

## Before vs. After (Canon EOS R6 Mark III)

| Built-in Windows Decoder (Before) | Canon CR3 WIC Codec (After) |
| :---: | :---: |
| ![Before - Microsoft Raw Image Decoder pink tint](docs/images/before-microsoft-raw.jpg) | ![After - Canon CR3 WIC Codec natural colors](docs/images/after-cr3-wic-codec.jpg) |
| *Outdated camera support → heavy pink cast* | *True 14-bit RAW colors & camera white balance* |

<sub>Sample RAW: [Canon EOS R6 Mark III (.CR3, CC0 Public Domain)](docs/images/canon-eos-r6-mark-iii-sample.CR3) via [raw.pixls.us](https://raw.pixls.us/)</sub>

---

## Download & Install

[![Download Latest Release](https://img.shields.io/badge/Download-Latest%20Release-16a34a?style=for-the-badge)](https://github.com/felix-henry/windows-cr3-raw-codec/releases/latest)

### 1. Pick the right file
- **Standard Windows PCs (Intel / AMD):** `CR3WICCodec-Setup-x64-v*.exe` *(Recommended)*
- **Snapdragon / ARM64 Windows 11 PCs:** `CR3WICCodec-Setup-arm64-v*.exe`

### 2. Install or update
- **Setup Installer (`.exe` — Recommended):** Double-click to install—works **for your user only** *(no admin rights needed)* or **for all users**. Uninstall anytime via **Windows Settings → Apps**.
- **Portable ZIP (`.zip`):** Extract all files into a folder first, then double-click **`Install.exe`** (remove via **`Uninstall.exe`**).
- **Updating:** Simply run the newest `.exe` installer over your existing installation. *(A PowerShell updater `scripts\update.ps1` is also included.)*

> [!TIP]
> **Old pink thumbnails still showing?** Open **Disk Cleanup (`cleanmgr.exe`)** on `C:`, select only **Thumbnails**, and clear the cache.

**Requirements:** Windows 10 (1809+) or Windows 11. Online installs download the Microsoft Visual C++ Runtime automatically; for offline PCs, install it first ([x64](https://aka.ms/vs/17/release/vc_redist.x64.exe) · [ARM64](https://aka.ms/vs/17/release/vc_redist.arm64.exe)).

---

## Supported Cameras

Supports **all Canon `.CR3` cameras** supported by LibRaw, including:
- **EOS R series:** EOS R, RP, R1, R3, R5, R5 C, R5 Mark II, R6, R6 Mark II, **R6 Mark III**, R7, R8, R10, R50, R100
- **DSLR / EOS M / PowerShot:** EOS 90D, 250D, 1D X Mark III, M50 / M50 II, M6 II, PowerShot G5 X II / G7 X III / V1

---

## Docs & Release History

- **[Wiki](https://github.com/felix-henry/windows-cr3-raw-codec/wiki)** — Troubleshooting, optional registry settings, and technical details
- **[Release Notes](RELEASES.md)** — Short summaries of each version *(or full technical notes in [CHANGELOG.md](CHANGELOG.md))*

---

<div align="center">

[![Website](https://img.shields.io/badge/Website-felixhenry.de-111827?style=for-the-badge&labelColor=1f2937&color=0284c7)](https://felixhenry.de)
[![Latest Release](https://img.shields.io/badge/Download-Latest%20Release-16a34a?style=for-the-badge)](https://github.com/felix-henry/windows-cr3-raw-codec/releases/latest)
[![Wiki](https://img.shields.io/badge/Docs-Wiki-6366f1?style=for-the-badge)](https://github.com/felix-henry/windows-cr3-raw-codec/wiki)

**Built for Canon photographers on Windows 10 / 11** · **[felixhenry.de](https://felixhenry.de)**

[![Build and Release Windows (x64 & ARM64)](https://github.com/felix-henry/windows-cr3-raw-codec/actions/workflows/build-release.yml/badge.svg)](https://github.com/felix-henry/windows-cr3-raw-codec/actions/workflows/build-release.yml)

<sub>[MIT License](LICENSE) · Powered by [LibRaw](https://www.libraw.org/) (LGPL 2.1 / CDDL 1.0) · [Third-Party Notices](THIRD_PARTY_NOTICES.md)</sub>  
<sub>Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org)</sub>

</div>
