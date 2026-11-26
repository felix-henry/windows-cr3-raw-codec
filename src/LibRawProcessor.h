#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <cstdint>
#include <vector>

struct RawImageInfo
{
    uint32_t width = 0;
    uint32_t height = 0;
    bool isValid = false;
};

struct DecodedImageBGR24
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    std::vector<uint8_t> pixels;
};

class LibRawProcessor
{
public:
    // Checks whether the stream begins with a Canon CR3 ISOBMFF signature (ftypcrx )
    // and can be opened by LibRaw. Always restores the original stream position.
    static bool ProbeStream(IStream* stream);

    // Parses RAW metadata and final oriented dimensions without running unpack/demosaic.
    static HRESULT ReadHeader(IStream* stream, RawImageInfo& outInfo);

    // Runs full LibRaw unpack + dcraw_process() demosaicing into 24-bit sRGB BGR pixels.
    // When halfSize is true, enables half_size=1 for fast RAW thumbnail generation.
    static HRESULT DecodeToBGR24(IStream* stream, bool halfSize, DecodedImageBGR24& outImage);
};
