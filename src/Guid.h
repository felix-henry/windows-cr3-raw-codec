#pragma once

#include <guiddef.h>

// {D7E4A3B1-5C8F-4A9E-9B2D-6F1A8E3C4D50}
// CLSID of the Canon CR3 WIC Bitmap Decoder
DEFINE_GUID(CLSID_CR3WICDecoder,
    0xd7e4a3b1, 0x5c8f, 0x4a9e, 0x9b, 0x2d, 0x6f, 0x1a, 0x8e, 0x3c, 0x4d, 0x50);

// {8A2F4C6E-9D1B-4E7A-B3C5-1D7F9E2A4B60}
// Container format GUID for Canon CR3 (ISOBMFF / ftypcrx )
DEFINE_GUID(GUID_ContainerFormatCR3,
    0x8a2f4c6e, 0x9d1b, 0x4e7a, 0xb3, 0xc5, 0x1d, 0x7f, 0x9e, 0x2a, 0x4b, 0x60);

// {3F9B1C7D-2E4A-4B8C-A6D1-5E8F2C3A9B70}
// Vendor GUID for this CR3 WIC Codec
DEFINE_GUID(GUID_VendorCanonCR3Codec,
    0x3f9b1c7d, 0x2e4a, 0x4b8c, 0xa6, 0xd1, 0x5e, 0x8f, 0x2c, 0x3a, 0x9b, 0x70);

// String forms for COM / WIC registry registration
inline constexpr wchar_t kClsidCR3WICDecoderStr[] = L"{D7E4A3B1-5C8F-4A9E-9B2D-6F1A8E3C4D50}";
inline constexpr wchar_t kContainerFormatCR3Str[] = L"{8A2F4C6E-9D1B-4E7A-B3C5-1D7F9E2A4B60}";
inline constexpr wchar_t kVendorCanonCR3CodecStr[] = L"{3F9B1C7D-2E4A-4B8C-A6D1-5E8F2C3A9B70}";
inline constexpr wchar_t kWicBitmapDecodersCategoryStr[] = L"{7ED96837-96F0-4812-B211-F13C24117ED3}";
