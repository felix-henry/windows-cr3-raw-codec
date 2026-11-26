# Canon CR3 Native WIC Codec (Windows 10/11 x64)

Native **Windows Imaging Component (WIC)** decoder for Canon `.CR3` RAW files, powered by current **LibRaw** with **OpenMP**.
Makes Windows Explorer (thumbnails & preview pane), Paint, and classic WIC viewers render **true 14-bit RAW colors** without the magenta/pink tint of outdated Microsoft codecs.

---

## Visual Comparison (Canon EOS R6 Mark III)

| Microsoft Raw Image Decoder (Before) | Canon CR3 WIC Codec (After) |
| :---: | :---: |
| ![Before - Microsoft Raw Image Decoder pink tint](docs/images/before-microsoft-raw.jpg) | ![After - Canon CR3 WIC Codec natural colors](docs/images/after-cr3-wic-codec.jpg) |
| *Outdated color matrix / black level → strong magenta wash* | *Current LibRaw 14-bit RAW demosaicing & camera WB* |

<sub>Sample RAW: Canon EOS R6 Mark III ([CC0 Public Domain](https://creativecommons.org/publicdomain/zero/1.0/) via [raw.pixls.us](https://raw.pixls.us/))</sub>

---

## Download & Install

**[Download Latest Release](https://github.com/felix-henry/windows-cr3-raw-codec/releases/latest)** *(Windows 10 / 11 64-bit)*

1. **Setup Installer (`CR3WICCodec-Setup-x64-v*.exe`):** Run the setup wizard. Uninstall anytime via **Windows Settings → Apps**.
2. **Portable ZIP (`CR3WICCodec-x64-portable.zip`):** Extract and double-click **`Install.exe`** (uses UAC elevation, no PowerShell needed). Double-click **`Uninstall.exe`** to remove.

**Requirements:** Windows 10 (1809+) / Windows 11 (x64) and [VC++ 2015–2022 Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe).

---

## Supported Cameras

Supports **all Canon `.CR3` cameras** supported by LibRaw, including:
- **EOS R series:** EOS R, RP, R1, R3, R5, R5 C, R5 Mark II, R6, R6 Mark II, **R6 Mark III**, R7, R8, R10, R50, R100
- **DSLR / EOS M / PowerShot:** EOS 90D, 250D, 1D X Mark III, M50 / M50 II, M6 II, PowerShot G5 X II / G7 X III / V1

---

## Documentation & License

- **[Technical Guide, Build Instructions, Registry Tuning & Troubleshooting](docs/Technical-Guide.md)** *(ready for GitHub Wiki)*
- **License:** [MIT License](LICENSE) · LibRaw (`libraw.dll`) under LGPL 2.1 / CDDL 1.0 ([THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md))
