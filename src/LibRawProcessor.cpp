#include "LibRawProcessor.h"
#include "WicIStreamDatastream.h"
#include <libraw/libraw.h>
#include <wincodec.h>
#include <algorithm>
#include <cstring>
#include <limits>

bool EnsureLibRawLoaded();

namespace
{
struct CodecTuning
{
    int demosaicQuality = 3;      // 3 = AHD (0=linear, 2=PPG, 3=AHD, 4=DCB, 11=DHT)
    int highlightMode = 2;        // 0=clip, 2=blend, 5=rebuild
    int noAutoBright = 0;         // 0=auto-bright with conservative thr, 1=fixed scale
    int chromaNoiseReduction = 0; // 0=off, 1=FBDD light, 2=FBDD full
    bool fastEmbeddedThumb = false;
    uint32_t outputBps = 8;       // 8 = 24bppBGR, 16 = 48bppBGR
    int outputColorSpace = 1;     // 1 = sRGB, 2 = Adobe RGB, 3 = Wide Gamut, 4 = ProPhoto, 5 = XYZ, 6 = ACES
};

class ComInitGuard
{
public:
    ComInitGuard()
    {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        shouldUninit_ = (hr == S_OK || hr == S_FALSE);
    }

    ~ComInitGuard()
    {
        if (shouldUninit_)
            CoUninitialize();
    }

    ComInitGuard(const ComInitGuard&) = delete;
    ComInitGuard& operator=(const ComInitGuard&) = delete;

private:
    bool shouldUninit_ = false;
};

uint32_t ReadBE32(const uint8_t* p)
{
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

uint64_t ReadBE64(const uint8_t* p)
{
    return (static_cast<uint64_t>(ReadBE32(p)) << 32) | ReadBE32(p + 4);
}

bool ReadExactFromStream(IStream* stream, void* dst, ULONG bytes)
{
    uint8_t* out = static_cast<uint8_t*>(dst);
    ULONG total = 0;
    while (total < bytes)
    {
        ULONG chunk = 0;
        const HRESULT hr = stream->Read(out + total, bytes - total, &chunk);
        if (FAILED(hr) || chunk == 0)
            return false;
        total += chunk;
    }
    return true;
}

std::string CopyTrimmedCString(const char* src, size_t maxLen)
{
    if (!src)
        return {};
    size_t len = 0;
    while (len < maxLen && src[len] != '\0')
        ++len;
    while (len > 0 && (src[len - 1] == ' ' || src[len - 1] == '\t' || src[len - 1] == '\r' || src[len - 1] == '\n'))
        --len;
    size_t start = 0;
    while (start < len && (src[start] == ' ' || src[start] == '\t'))
        ++start;
    return std::string(src + start, len - start);
}

DWORD ReadRegistryDwordOrDefault(const wchar_t* valueName, DWORD defaultValue)
{
    DWORD value = defaultValue;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\CR3WICCodec",
        valueName,
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    return (status == ERROR_SUCCESS) ? value : defaultValue;
}

CodecTuning LoadCodecTuning()
{
    CodecTuning cfg;
    cfg.demosaicQuality = static_cast<int>(ReadRegistryDwordOrDefault(L"DemosaicQuality", 3));
    cfg.highlightMode = static_cast<int>(ReadRegistryDwordOrDefault(L"HighlightMode", 2));
    cfg.noAutoBright = ReadRegistryDwordOrDefault(L"NoAutoBright", 0) ? 1 : 0;
    cfg.chromaNoiseReduction = static_cast<int>(ReadRegistryDwordOrDefault(L"ChromaNoiseReduction", 0));
    cfg.fastEmbeddedThumb = (ReadRegistryDwordOrDefault(L"FastEmbeddedThumb", 0) != 0);

    const DWORD bps = ReadRegistryDwordOrDefault(L"OutputBps", 8);
    cfg.outputBps = (bps == 16) ? 16u : 8u;

    const DWORD cs = ReadRegistryDwordOrDefault(L"OutputColorSpace", 1);
    cfg.outputColorSpace = (cs >= 1 && cs <= 6) ? static_cast<int>(cs) : 1;
    return cfg;
}

WICBitmapTransformOptions MapExifOrientationToWicTransform(uint16_t exifOrient)
{
    switch (exifOrient)
    {
    case 2:
        return WICBitmapTransformFlipHorizontal;
    case 3:
        return WICBitmapTransformRotate180;
    case 4:
        return WICBitmapTransformFlipVertical;
    case 5:
        return static_cast<WICBitmapTransformOptions>(WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal);
    case 6:
        return WICBitmapTransformRotate90;
    case 7:
        return static_cast<WICBitmapTransformOptions>(WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal);
    case 8:
        return WICBitmapTransformRotate270;
    default:
        return WICBitmapTransformRotate0;
    }
}

WICBitmapTransformOptions MapLibRawFlipToWicTransform(int flip)
{
    switch (flip)
    {
    case 3:
        return WICBitmapTransformRotate180;
    case 5:
        return WICBitmapTransformRotate270;
    case 6:
        return WICBitmapTransformRotate90;
    default:
        return WICBitmapTransformRotate0;
    }
}

uint16_t ReadJpegFrameExifOrientation(IWICBitmapFrameDecode* frame)
{
    if (!frame)
        return 0;

    IWICMetadataQueryReader* qr = nullptr;
    if (FAILED(frame->GetMetadataQueryReader(&qr)) || !qr)
        return 0;

    PROPVARIANT pv;
    PropVariantInit(&pv);
    uint16_t orient = 0;
    if (SUCCEEDED(qr->GetMetadataByName(L"/app1/ifd/{ushort=274}", &pv)))
    {
        if (pv.vt == VT_UI2)
            orient = pv.uiVal;
        else if (pv.vt == VT_UI4)
            orient = static_cast<uint16_t>(pv.ulVal);
    }
    PropVariantClear(&pv);
    qr->Release();
    return orient;
}

bool TryDecodeEmbeddedJpegThumb(LibRaw& processor, DecodedImageBGR& outImage)
{
    if (processor.unpack_thumb() != LIBRAW_SUCCESS)
        return false;

    int err = 0;
    libraw_processed_image_t* thumb = processor.dcraw_make_mem_thumb(&err);
    if (!thumb)
        return false;

    if (thumb->type != LIBRAW_IMAGE_JPEG || thumb->data_size == 0)
    {
        LibRaw::dcraw_clear_mem(thumb);
        return false;
    }

    ComInitGuard comGuard;
    bool ok = false;
    IWICImagingFactory* factory = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) && factory)
    {
        IWICStream* memStream = nullptr;
        if (SUCCEEDED(factory->CreateStream(&memStream)) && memStream)
        {
            if (SUCCEEDED(memStream->InitializeFromMemory(thumb->data, static_cast<DWORD>(thumb->data_size))))
            {
                IWICBitmapDecoder* jpegDec = nullptr;
                if (SUCCEEDED(factory->CreateDecoderFromStream(
                        memStream, &GUID_VendorMicrosoft, WICDecodeMetadataCacheOnDemand, &jpegDec)) &&
                    jpegDec)
                {
                    IWICBitmapFrameDecode* frame = nullptr;
                    if (SUCCEEDED(jpegDec->GetFrame(0, &frame)) && frame)
                    {
                        const uint16_t jpegOrient = ReadJpegFrameExifOrientation(frame);
                        IWICFormatConverter* conv = nullptr;
                        if (SUCCEEDED(factory->CreateFormatConverter(&conv)) && conv)
                        {
                            if (SUCCEEDED(conv->Initialize(
                                    frame,
                                    GUID_WICPixelFormat24bppBGR,
                                    WICBitmapDitherTypeNone,
                                    nullptr,
                                    0.0,
                                    WICBitmapPaletteTypeCustom)))
                            {
                                IWICBitmapSource* source = conv;
                                IWICBitmapFlipRotator* rotator = nullptr;

                                // Prefer the embedded JPEG's own EXIF Orientation if present (!= 1),
                                // otherwise fall back to LibRaw's CR3 orientation (sizes.flip).
                                const WICBitmapTransformOptions xform = (jpegOrient > 1)
                                    ? MapExifOrientationToWicTransform(jpegOrient)
                                    : (jpegOrient == 1 ? WICBitmapTransformRotate0
                                                       : MapLibRawFlipToWicTransform(processor.imgdata.sizes.flip));

                                if (xform != WICBitmapTransformRotate0 &&
                                    SUCCEEDED(factory->CreateBitmapFlipRotator(&rotator)) && rotator &&
                                    SUCCEEDED(rotator->Initialize(conv, xform)))
                                {
                                    source = rotator;
                                }

                                UINT w = 0, h = 0;
                                if (SUCCEEDED(source->GetSize(&w, &h)) && w > 0 && h > 0)
                                {
                                    const UINT stride = w * 3u;
                                    const size_t totalBytes = static_cast<size_t>(stride) * h;
                                    outImage.width = w;
                                    outImage.height = h;
                                    outImage.stride = stride;
                                    outImage.bytesPerPixel = 3;
                                    outImage.pixelFormat = GUID_WICPixelFormat24bppBGR;
                                    outImage.pixels.resize(totalBytes);
                                    if (SUCCEEDED(source->CopyPixels(nullptr, stride, static_cast<UINT>(totalBytes), outImage.pixels.data())))
                                    {
                                        ok = true;
                                    }
                                    else
                                    {
                                        outImage = {};
                                    }
                                }

                                if (rotator)
                                    rotator->Release();
                            }
                            conv->Release();
                        }
                        frame->Release();
                    }
                    jpegDec->Release();
                }
            }
            memStream->Release();
        }
        factory->Release();
    }

    LibRaw::dcraw_clear_mem(thumb);
    return ok;
}

void ConfigureLibRawParams(LibRaw& processor, bool halfSize, const CodecTuning& tuning)
{
    auto& p = processor.imgdata.params;
    p.use_camera_wb = 1;
    p.use_auto_wb = 0;
    p.use_camera_matrix = 1;
    p.output_color = tuning.outputColorSpace;
    p.output_bps = (!halfSize && tuning.outputBps == 16) ? 16 : 8;

    if (tuning.outputColorSpace == 1)
    {
        // IEC 61966-2-1 sRGB transfer curve
        p.gamm[0] = 1.0 / 2.4;
        p.gamm[1] = 12.92;
    }
    else if (tuning.outputColorSpace == 4)
    {
        // ProPhoto RGB (gamma 1.8)
        p.gamm[0] = 1.0 / 1.8;
        p.gamm[1] = 0.0;
    }
    else
    {
        // Adobe RGB / Wide Gamut / XYZ / ACES (gamma 2.2)
        p.gamm[0] = 1.0 / 2.2;
        p.gamm[1] = 0.0;
    }

    p.highlight = (tuning.highlightMode >= 0 && tuning.highlightMode <= 9) ? tuning.highlightMode : 2;
    p.no_auto_bright = tuning.noAutoBright;
    p.auto_bright_thr = 0.001f;  // Preserve highlight dynamic range (0.1% clipping ceiling)
    p.adjust_maximum_thr = 0.75f;
    p.fbdd_noiserd = (!halfSize && tuning.chromaNoiseReduction >= 0 && tuning.chromaNoiseReduction <= 2)
                         ? tuning.chromaNoiseReduction
                         : 0;
    p.half_size = halfSize ? 1 : 0;
    p.user_qual = halfSize ? 0 : tuning.demosaicQuality;
    p.four_color_rgb = 0;
    p.user_flip = -1;            // Respect camera EXIF orientation
}

void ExtractRawMetadata(const LibRaw& processor, const CodecTuning& tuning, RawMetadata& outMeta)
{
    const auto& idata = processor.imgdata.idata;
    const auto& other = processor.imgdata.other;
    const auto& lens = processor.imgdata.lens;
    const auto& shoot = processor.imgdata.shootinginfo;

    outMeta.make = CopyTrimmedCString(idata.make, sizeof(idata.make));
    outMeta.model = CopyTrimmedCString(idata.model, sizeof(idata.model));
    outMeta.software = CopyTrimmedCString(idata.software, sizeof(idata.software));
    outMeta.artist = CopyTrimmedCString(other.artist, sizeof(other.artist));
    outMeta.description = CopyTrimmedCString(other.desc, sizeof(other.desc));

    outMeta.lensMake = CopyTrimmedCString(lens.LensMake, sizeof(lens.LensMake));
    outMeta.lensModel = CopyTrimmedCString(lens.Lens, sizeof(lens.Lens));
    outMeta.lensSerial = CopyTrimmedCString(lens.LensSerial, sizeof(lens.LensSerial));
    outMeta.bodySerial = CopyTrimmedCString(shoot.BodySerial, sizeof(shoot.BodySerial));

    outMeta.timestamp = other.timestamp;
    outMeta.isoSpeed = other.iso_speed;
    outMeta.shutter = other.shutter;
    outMeta.aperture = other.aperture;
    outMeta.focalLen = other.focal_len;
    outMeta.focalLen35mm = lens.FocalLengthIn35mmFormat;
    outMeta.exposureProgram = shoot.ExposureProgram;
    outMeta.meteringMode = shoot.MeteringMode;
    outMeta.flip = processor.imgdata.sizes.flip;
    outMeta.exifColorSpace = (tuning.outputColorSpace == 1) ? 1 : 2;
}

HRESULT MapLibRawError(int err)
{
    switch (err)
    {
    case LIBRAW_SUCCESS:
        return S_OK;
    case LIBRAW_UNSUFFICIENT_MEMORY:
        return E_OUTOFMEMORY;
    case LIBRAW_FILE_UNSUPPORTED:
    case LIBRAW_REQUEST_FOR_NONEXISTENT_IMAGE:
    case LIBRAW_DATA_ERROR:
        return WINCODEC_ERR_BADIMAGE;
    case LIBRAW_IO_ERROR:
        return WINCODEC_ERR_STREAMREAD;
    default:
        return E_FAIL;
    }
}

class StreamPositionGuard
{
public:
    explicit StreamPositionGuard(IStream* stream) : stream_(stream)
    {
        if (stream_)
        {
            LARGE_INTEGER zero{};
            ULARGE_INTEGER cur{};
            if (SUCCEEDED(stream_->Seek(zero, STREAM_SEEK_CUR, &cur)))
            {
                savedPos_ = cur.QuadPart;
                hasSavedPos_ = true;
            }
        }
    }

    ~StreamPositionGuard()
    {
        if (stream_ && hasSavedPos_)
        {
            LARGE_INTEGER li{};
            li.QuadPart = static_cast<LONGLONG>(savedPos_);
            stream_->Seek(li, STREAM_SEEK_SET, nullptr);
        }
    }

private:
    IStream* stream_ = nullptr;
    ULONGLONG savedPos_ = 0;
    bool hasSavedPos_ = false;
};
} // namespace

bool LibRawProcessor::ProbeStream(IStream* stream)
{
    if (!stream)
        return false;

    StreamPositionGuard guard(stream);

    LARGE_INTEGER zero{};
    if (FAILED(stream->Seek(zero, STREAM_SEEK_SET, nullptr)))
        return false;

    uint8_t header[16]{};
    if (!ReadExactFromStream(stream, header, 16))
        return false;

    const uint32_t ftypSize = ReadBE32(header);
    if (ftypSize < 16 || ftypSize > 65536)
        return false;
    if (std::memcmp(header + 4, "ftypcrx ", 8) != 0)
        return false;

    STATSTG stat{};
    uint64_t totalSize = 0;
    const bool hasTotalSize = SUCCEEDED(stream->Stat(&stat, STATFLAG_NONAME));
    if (hasTotalSize)
    {
        totalSize = stat.cbSize.QuadPart;
        if (ftypSize >= totalSize)
            return false;
    }

    // Walk top-level ISOBMFF boxes quickly to verify a 'moov' container exists
    uint64_t offset = ftypSize;
    for (int i = 0; i < 64; ++i)
    {
        if (hasTotalSize && offset + 8 > totalSize)
            return false;

        LARGE_INTEGER li{};
        li.QuadPart = static_cast<LONGLONG>(offset);
        if (FAILED(stream->Seek(li, STREAM_SEEK_SET, nullptr)))
            return false;

        uint8_t boxHdr[16]{};
        if (!ReadExactFromStream(stream, boxHdr, 8))
            return false;

        const uint32_t boxSize32 = ReadBE32(boxHdr);
        if (std::memcmp(boxHdr + 4, "moov", 4) == 0)
            return (boxSize32 == 0 || boxSize32 >= 8);

        uint64_t boxSize = boxSize32;
        if (boxSize32 == 1)
        {
            if (!ReadExactFromStream(stream, boxHdr + 8, 8))
                return false;
            boxSize = ReadBE64(boxHdr + 8);
            if (boxSize < 16)
                return false;
        }
        else if (boxSize32 == 0)
        {
            return false;
        }
        else if (boxSize < 8)
        {
            return false;
        }

        if (offset > (std::numeric_limits<uint64_t>::max)() - boxSize)
            return false;
        offset += boxSize;
    }

    return false;
}

HRESULT LibRawProcessor::ReadHeader(IStream* stream, RawImageInfo& outInfo)
{
    outInfo = {};
    if (!stream)
        return E_INVALIDARG;

    if (!EnsureLibRawLoaded())
        return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);

    try
    {
        WicIStreamDatastream datastream(stream);
        if (!datastream.valid())
            return WINCODEC_ERR_STREAMREAD;

        LibRaw processor(LIBRAW_OPTIONS_NO_DATAERR_CALLBACK);
        const CodecTuning tuning = LoadCodecTuning();
        ConfigureLibRawParams(processor, false, tuning);

        int rc = processor.open_datastream(&datastream);
        if (rc != LIBRAW_SUCCESS)
            return MapLibRawError(rc);

        rc = processor.adjust_sizes_info_only();
        if (rc != LIBRAW_SUCCESS)
            return MapLibRawError(rc);

        int width = 0;
        int height = 0;
        int colors = 0;
        int bps = 0;
        processor.get_mem_image_format(&width, &height, &colors, &bps);

        if (width <= 0 || height <= 0)
            return WINCODEC_ERR_BADIMAGE;

        outInfo.width = static_cast<uint32_t>(width);
        outInfo.height = static_cast<uint32_t>(height);
        outInfo.halfWidth = (outInfo.width >= 2) ? (outInfo.width / 2u) : outInfo.width;
        outInfo.halfHeight = (outInfo.height >= 2) ? (outInfo.height / 2u) : outInfo.height;
        outInfo.outputBps = tuning.outputBps;
        outInfo.outputColorSpace = tuning.outputColorSpace;
        ExtractRawMetadata(processor, tuning, outInfo.metadata);
        outInfo.isValid = true;
        return S_OK;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

HRESULT LibRawProcessor::DecodeToImage(IStream* stream, CR3DecodeMode mode, DecodedImageBGR& outImage)
{
    outImage = {};
    if (!stream)
        return E_INVALIDARG;

    if (!EnsureLibRawLoaded())
        return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);

    try
    {
        WicIStreamDatastream datastream(stream);
        if (!datastream.valid())
            return WINCODEC_ERR_STREAMREAD;

        const bool halfSize = (mode != CR3DecodeMode::FullRaw);
        LibRaw processor(LIBRAW_OPTIONS_NO_DATAERR_CALLBACK);
        const CodecTuning tuning = LoadCodecTuning();
        ConfigureLibRawParams(processor, halfSize, tuning);

        int rc = processor.open_datastream(&datastream);
        if (rc != LIBRAW_SUCCESS)
            return MapLibRawError(rc);

        const bool tryEmbedded = (mode == CR3DecodeMode::PreviewJpeg) ||
                                 (mode == CR3DecodeMode::HalfSizeRaw && tuning.fastEmbeddedThumb);
        if (tryEmbedded)
        {
            if (TryDecodeEmbeddedJpegThumb(processor, outImage))
            {
                processor.recycle();
                return S_OK;
            }
        }

        rc = processor.unpack();
        if (rc != LIBRAW_SUCCESS)
            return MapLibRawError(rc);

        rc = processor.dcraw_process();
        if (rc != LIBRAW_SUCCESS)
            return MapLibRawError(rc);

        int width = 0;
        int height = 0;
        int colors = 0;
        int bps = 0;
        processor.get_mem_image_format(&width, &height, &colors, &bps);

        if (width <= 0 || height <= 0 || (bps != 8 && bps != 16))
            return WINCODEC_ERR_BADIMAGE;

        const uint32_t w = static_cast<uint32_t>(width);
        const uint32_t h = static_cast<uint32_t>(height);
        const uint32_t bytesPerChannel = (bps == 16) ? 2u : 1u;
        const uint32_t bpp = 3u * bytesPerChannel;
        const uint32_t stride = w * bpp;
        const size_t totalBytes = static_cast<size_t>(stride) * static_cast<size_t>(h);

        outImage.width = w;
        outImage.height = h;
        outImage.stride = stride;
        outImage.bytesPerPixel = bpp;
        outImage.pixelFormat = (bps == 16) ? GUID_WICPixelFormat48bppBGR : GUID_WICPixelFormat24bppBGR;
        outImage.pixels.resize(totalBytes);

        if (colors == 3)
        {
            rc = processor.copy_mem_image(outImage.pixels.data(), static_cast<int>(stride), 1 /* BGR */);
            if (rc != LIBRAW_SUCCESS)
                return MapLibRawError(rc);
        }
        else
        {
            libraw_processed_image_t* img = processor.dcraw_make_mem_image(&rc);
            if (!img)
                return MapLibRawError(rc != LIBRAW_SUCCESS ? rc : LIBRAW_UNSPECIFIED_ERROR);

            if (bps == 8)
            {
                if (img->colors == 1)
                {
                    for (uint32_t y = 0; y < h; ++y)
                    {
                        const uint8_t* srcRow = img->data + static_cast<size_t>(y) * w;
                        uint8_t* dstRow = outImage.pixels.data() + static_cast<size_t>(y) * stride;
                        for (uint32_t x = 0; x < w; ++x)
                        {
                            const uint8_t v = srcRow[x];
                            dstRow[x * 3 + 0] = v;
                            dstRow[x * 3 + 1] = v;
                            dstRow[x * 3 + 2] = v;
                        }
                    }
                }
                else if (img->colors >= 3)
                {
                    const uint32_t c = static_cast<uint32_t>(img->colors);
                    for (uint32_t y = 0; y < h; ++y)
                    {
                        const uint8_t* srcRow = img->data + static_cast<size_t>(y) * w * c;
                        uint8_t* dstRow = outImage.pixels.data() + static_cast<size_t>(y) * stride;
                        for (uint32_t x = 0; x < w; ++x)
                        {
                            dstRow[x * 3 + 0] = srcRow[x * c + 2];
                            dstRow[x * 3 + 1] = srcRow[x * c + 1];
                            dstRow[x * 3 + 2] = srcRow[x * c + 0];
                        }
                    }
                }
                else
                {
                    LibRaw::dcraw_clear_mem(img);
                    return WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
                }
            }
            else
            {
                const auto* src16 = reinterpret_cast<const uint16_t*>(img->data);
                auto* dst16 = reinterpret_cast<uint16_t*>(outImage.pixels.data());
                if (img->colors == 1)
                {
                    for (uint32_t y = 0; y < h; ++y)
                    {
                        const uint16_t* srcRow = src16 + static_cast<size_t>(y) * w;
                        uint16_t* dstRow = dst16 + static_cast<size_t>(y) * w * 3u;
                        for (uint32_t x = 0; x < w; ++x)
                        {
                            const uint16_t v = srcRow[x];
                            dstRow[x * 3 + 0] = v;
                            dstRow[x * 3 + 1] = v;
                            dstRow[x * 3 + 2] = v;
                        }
                    }
                }
                else if (img->colors >= 3)
                {
                    const uint32_t c = static_cast<uint32_t>(img->colors);
                    for (uint32_t y = 0; y < h; ++y)
                    {
                        const uint16_t* srcRow = src16 + static_cast<size_t>(y) * w * c;
                        uint16_t* dstRow = dst16 + static_cast<size_t>(y) * w * 3u;
                        for (uint32_t x = 0; x < w; ++x)
                        {
                            dstRow[x * 3 + 0] = srcRow[x * c + 2];
                            dstRow[x * 3 + 1] = srcRow[x * c + 1];
                            dstRow[x * 3 + 2] = srcRow[x * c + 0];
                        }
                    }
                }
                else
                {
                    LibRaw::dcraw_clear_mem(img);
                    return WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
                }
            }

            LibRaw::dcraw_clear_mem(img);
        }

        processor.recycle();
        return S_OK;
    }
    catch (...)
    {
        outImage = {};
        return E_FAIL;
    }
}
