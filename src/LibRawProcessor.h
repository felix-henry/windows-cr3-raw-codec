#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <wincodec.h>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

struct RawMetadata
{
    std::string make;
    std::string model;
    std::string software;
    std::string artist;
    std::string description;
    std::string copyright;
    std::string subSecTimeOriginal;
    std::string lensMake;
    std::string lensModel;
    std::string lensSerial;
    std::string bodySerial;
    std::time_t timestamp = 0;
    float isoSpeed = 0.0f;
    float shutter = 0.0f;
    float aperture = 0.0f;
    float focalLen = 0.0f;
    uint16_t focalLen35mm = 0;
    short exposureProgram = -1;
    short meteringMode = -1;
    bool hasExposureBias = false;
    int32_t exposureBiasNum = 0;
    int32_t exposureBiasDen = 1;
    bool hasFlash = false;
    uint16_t flash = 0;
    bool hasWhiteBalance = false;
    uint16_t whiteBalance = 0;
    int rating = -1; // 0..5 from XMP if present, else -1
    bool hasGps = false;
    float gpsLat[3]{};
    float gpsLon[3]{};
    float gpsAltitude = 0.0f;
    char gpsLatRef = 0;
    char gpsLonRef = 0;
    char gpsAltRef = 0;
    int flip = 0;
    int exifColorSpace = 1;
};

struct RawImageInfo
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t halfWidth = 0;
    uint32_t halfHeight = 0;
    uint32_t outputBps = 8;      // 8 -> 24bppBGR, 16 -> 48bppBGR
    int outputColorSpace = 1;    // 1 = sRGB, 2 = Adobe RGB, 3..6 = LibRaw output_color
    bool fastEmbeddedThumb = false;
    uint64_t embeddedJpegOffset = 0;
    uint32_t embeddedJpegLength = 0;
    int embeddedJpegFlip = 0;
    bool isValid = false;
    RawMetadata metadata;
};

struct DecodedImageBGR
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t bytesPerPixel = 3;
    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat24bppBGR;
    std::vector<uint8_t> pixels;
};

enum class CR3DecodeMode
{
    FullRaw,      // Full-res LibRaw demosaic (respects OutputBps & OutputColorSpace)
    HalfSizeRaw,  // Fast 24bppBGR half-size RAW (or embedded JPEG if FastEmbeddedThumb=1)
    PreviewJpeg   // Fast embedded JPEG preview with HalfSizeRaw fallback
};

class LibRawProcessor
{
public:
    // Fast ISOBMFF Canon CR3 container validation (ftypcrx  + moov box check).
    // Always restores the original stream position.
    static bool ProbeStream(IStream* stream);

    // Parses RAW dimensions, tuning config, embedded JPEG location, and extended
    // EXIF/GPS metadata without running unpack/demosaic.
    static HRESULT ReadHeader(IStream* stream, RawImageInfo& outInfo);

    // Directly decodes an embedded JPEG preview at a previously cached stream offset
    // without re-instantiating LibRaw.
    static HRESULT DecodeEmbeddedJpegFromStream(
        IStream* stream,
        uint64_t offset,
        uint32_t length,
        int fallbackFlip,
        DecodedImageBGR& outImage);

    // Runs LibRaw decode according to mode into 24bppBGR or 48bppBGR pixels.
    static HRESULT DecodeToImage(IStream* stream, CR3DecodeMode mode, DecodedImageBGR& outImage);
};
