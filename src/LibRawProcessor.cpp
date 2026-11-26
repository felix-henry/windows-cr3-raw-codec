#include "LibRawProcessor.h"
#include "WicIStreamDatastream.h"
#include <libraw/libraw.h>
#include <wincodec.h>
#include <cstring>

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
};

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
    return cfg;
}

bool TryDecodeEmbeddedJpegThumb(LibRaw& processor, DecodedImageBGR24& outImage)
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
                                const int flip = processor.imgdata.sizes.flip;
                                WICBitmapTransformOptions xform = WICBitmapTransformRotate0;
                                if (flip == 3)
                                    xform = WICBitmapTransformRotate180;
                                else if (flip == 5)
                                    xform = WICBitmapTransformRotate270;
                                else if (flip == 6)
                                    xform = WICBitmapTransformRotate90;

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
    p.output_color = 1;          // sRGB
    p.output_bps = 8;            // 8-bit sRGB gamma output for WIC 24bppBGR
    p.gamm[0] = 1.0 / 2.4;       // IEC 61966-2-1 sRGB gamma
    p.gamm[1] = 12.92;
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
    ULONG bytesRead = 0;
    if (FAILED(stream->Read(header, 12, &bytesRead)) || bytesRead < 12)
        return false;

    if (std::memcmp(header + 4, "ftypcrx ", 8) != 0)
        return false;

    RawImageInfo info{};
    return SUCCEEDED(ReadHeader(stream, info)) && info.isValid;
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
        outInfo.isValid = true;
        return S_OK;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

HRESULT LibRawProcessor::DecodeToBGR24(IStream* stream, bool halfSize, DecodedImageBGR24& outImage)
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

        LibRaw processor(LIBRAW_OPTIONS_NO_DATAERR_CALLBACK);
        const CodecTuning tuning = LoadCodecTuning();
        ConfigureLibRawParams(processor, halfSize, tuning);

        int rc = processor.open_datastream(&datastream);
        if (rc != LIBRAW_SUCCESS)
            return MapLibRawError(rc);

        if (halfSize && tuning.fastEmbeddedThumb)
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

        if (width <= 0 || height <= 0 || bps != 8)
            return WINCODEC_ERR_BADIMAGE;

        const uint32_t w = static_cast<uint32_t>(width);
        const uint32_t h = static_cast<uint32_t>(height);
        const uint32_t stride = w * 3u;
        const size_t totalBytes = static_cast<size_t>(stride) * static_cast<size_t>(h);

        outImage.width = w;
        outImage.height = h;
        outImage.stride = stride;
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
