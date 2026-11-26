#include "CR3Decoder.h"
#include "ClassFactory.h"
#include "Guid.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <new>

namespace
{
HRESULT CopyImageRect(const DecodedImageBGR& img, const WICRect* prc, UINT cbStride, UINT cbBufferSize, BYTE* pbBuffer)
{
    if (!pbBuffer)
        return E_INVALIDARG;

    WICRect rect{0, 0, static_cast<INT>(img.width), static_cast<INT>(img.height)};
    if (prc)
    {
        if (prc->X < 0 || prc->Y < 0 || prc->Width < 0 || prc->Height < 0)
            return E_INVALIDARG;
        if (static_cast<uint64_t>(prc->X) + static_cast<uint64_t>(prc->Width) > img.width ||
            static_cast<uint64_t>(prc->Y) + static_cast<uint64_t>(prc->Height) > img.height)
        {
            return E_INVALIDARG;
        }
        rect = *prc;
    }

    if (rect.Width == 0 || rect.Height == 0)
        return S_OK;

    const UINT rowBytes = static_cast<UINT>(rect.Width) * img.bytesPerPixel;
    if (cbStride < rowBytes)
        return E_INVALIDARG;

    const uint64_t requiredBytes = static_cast<uint64_t>(rect.Height - 1) * cbStride + rowBytes;
    if (cbBufferSize < requiredBytes)
        return WINCODEC_ERR_INSUFFICIENTBUFFER;

    const size_t srcXOffset = static_cast<size_t>(rect.X) * img.bytesPerPixel;
    for (INT row = 0; row < rect.Height; ++row)
    {
        const uint8_t* src = img.pixels.data() + static_cast<size_t>(rect.Y + row) * img.stride + srcXOffset;
        BYTE* dst = pbBuffer + static_cast<size_t>(row) * cbStride;
        std::memcpy(dst, src, rowBytes);
    }

    return S_OK;
}

HRESULT ProvideColorContext(int exifColorSpace, UINT cCount, IWICColorContext** ppIColorContexts, UINT* pcActualCount)
{
    if (!pcActualCount)
        return E_INVALIDARG;

    *pcActualCount = 1;
    if (!ppIColorContexts)
        return S_OK;

    if (cCount < 1)
        return WINCODEC_ERR_INSUFFICIENTBUFFER;

    if (!ppIColorContexts[0])
        return E_INVALIDARG;

    const UINT cs = (exifColorSpace == 2) ? 2u : 1u; // 1 = sRGB, 2 = Adobe RGB
    return ppIColorContexts[0]->InitializeFromExifColorSpace(cs);
}

std::shared_ptr<const DecodedImageBGR> DownsampleHalf2x2To24bpp(const DecodedImageBGR& src)
{
    if (src.width < 2 || src.height < 2)
        return nullptr;

    auto dst = std::make_shared<DecodedImageBGR>();
    dst->width = src.width / 2u;
    dst->height = src.height / 2u;
    dst->bytesPerPixel = 3;
    dst->pixelFormat = GUID_WICPixelFormat24bppBGR;
    dst->stride = dst->width * 3u;
    dst->pixels.resize(static_cast<size_t>(dst->stride) * dst->height);

    if (src.bytesPerPixel == 3)
    {
        for (uint32_t y = 0; y < dst->height; ++y)
        {
            const uint8_t* row0 = src.pixels.data() + static_cast<size_t>(y * 2u) * src.stride;
            const uint8_t* row1 = row0 + src.stride;
            uint8_t* out = dst->pixels.data() + static_cast<size_t>(y) * dst->stride;
            for (uint32_t x = 0; x < dst->width; ++x)
            {
                const size_t sx0 = static_cast<size_t>(x * 2u) * 3u;
                const size_t sx1 = sx0 + 3u;
                for (int c = 0; c < 3; ++c)
                {
                    const uint32_t sum = static_cast<uint32_t>(row0[sx0 + c]) +
                                         static_cast<uint32_t>(row0[sx1 + c]) +
                                         static_cast<uint32_t>(row1[sx0 + c]) +
                                         static_cast<uint32_t>(row1[sx1 + c]) + 2u;
                    out[x * 3u + c] = static_cast<uint8_t>(sum >> 2);
                }
            }
        }
        return dst;
    }

    if (src.bytesPerPixel == 6)
    {
        const uint32_t srcStride16 = src.stride / 2u;
        const auto* src16 = reinterpret_cast<const uint16_t*>(src.pixels.data());
        for (uint32_t y = 0; y < dst->height; ++y)
        {
            const uint16_t* row0 = src16 + static_cast<size_t>(y * 2u) * srcStride16;
            const uint16_t* row1 = row0 + srcStride16;
            uint8_t* out = dst->pixels.data() + static_cast<size_t>(y) * dst->stride;
            for (uint32_t x = 0; x < dst->width; ++x)
            {
                const size_t sx0 = static_cast<size_t>(x * 2u) * 3u;
                const size_t sx1 = sx0 + 3u;
                for (int c = 0; c < 3; ++c)
                {
                    const uint32_t sum16 = static_cast<uint32_t>(row0[sx0 + c]) +
                                           static_cast<uint32_t>(row0[sx1 + c]) +
                                           static_cast<uint32_t>(row1[sx0 + c]) +
                                           static_cast<uint32_t>(row1[sx1 + c]) + 2u;
                    out[x * 3u + c] = static_cast<uint8_t>((sum16 >> 2) >> 8);
                }
            }
        }
        return dst;
    }

    return nullptr;
}

void WriteAsciiMeta(IWICMetadataQueryWriter* qw, const wchar_t* name, const std::string& val)
{
    if (!qw || !name || val.empty())
        return;

    PROPVARIANT pv;
    PropVariantInit(&pv);
    pv.vt = VT_LPSTR;
    pv.pszVal = const_cast<LPSTR>(val.c_str());
    qw->SetMetadataByName(name, &pv);
}

void WriteUint8Meta(IWICMetadataQueryWriter* qw, const wchar_t* name, uint8_t val)
{
    if (!qw || !name)
        return;

    PROPVARIANT pv;
    PropVariantInit(&pv);
    pv.vt = VT_UI1;
    pv.bVal = val;
    qw->SetMetadataByName(name, &pv);
}

void WriteUint16Meta(IWICMetadataQueryWriter* qw, const wchar_t* name, uint16_t val)
{
    if (!qw || !name)
        return;

    PROPVARIANT pv;
    PropVariantInit(&pv);
    pv.vt = VT_UI2;
    pv.uiVal = val;
    qw->SetMetadataByName(name, &pv);
}

void WriteRationalMeta(IWICMetadataQueryWriter* qw, const wchar_t* name, uint32_t num, uint32_t den)
{
    if (!qw || !name || den == 0)
        return;

    PROPVARIANT pv;
    PropVariantInit(&pv);
    pv.vt = VT_UI8;
    pv.uhVal.LowPart = num;
    pv.uhVal.HighPart = den;
    qw->SetMetadataByName(name, &pv);
}

void WriteSignedRationalMeta(IWICMetadataQueryWriter* qw, const wchar_t* name, int32_t num, int32_t den)
{
    if (!qw || !name || den == 0)
        return;

    PROPVARIANT pv;
    PropVariantInit(&pv);
    pv.vt = VT_I8;
    pv.hVal.LowPart = static_cast<DWORD>(num);
    pv.hVal.HighPart = static_cast<LONG>(den);
    qw->SetMetadataByName(name, &pv);
}

void WriteGpsCoordMeta(IWICMetadataQueryWriter* qw, const wchar_t* name, const float dms[3])
{
    if (!qw || !name)
        return;

    ULARGE_INTEGER elems[3]{};
    elems[0].LowPart = static_cast<DWORD>(std::round(std::max(0.0f, dms[0])));
    elems[0].HighPart = 1u;
    elems[1].LowPart = static_cast<DWORD>(std::round(std::max(0.0f, dms[1])));
    elems[1].HighPart = 1u;
    elems[2].LowPart = static_cast<DWORD>(std::round(std::max(0.0f, dms[2]) * 10000.0f));
    elems[2].HighPart = 10000u;

    PROPVARIANT pv;
    PropVariantInit(&pv);
    pv.vt = VT_VECTOR | VT_UI8;
    pv.cauh.cElems = 3;
    pv.cauh.pElems = elems;
    qw->SetMetadataByName(name, &pv);
}

void FloatToRational(float value, uint32_t& outNum, uint32_t& outDen)
{
    if (value <= 0.0f)
    {
        outNum = 0;
        outDen = 1;
        return;
    }

    if (value < 0.5f)
    {
        const double inv = 1.0 / static_cast<double>(value);
        const double rounded = std::round(inv);
        if (rounded >= 1.0 && std::abs(inv - rounded) / inv < 0.02)
        {
            outNum = 1;
            outDen = static_cast<uint32_t>(rounded);
            return;
        }
        outNum = static_cast<uint32_t>(std::round(static_cast<double>(value) * 1000000.0));
        outDen = 1000000u;
        return;
    }

    outNum = static_cast<uint32_t>(std::round(static_cast<double>(value) * 100.0));
    outDen = 100u;
}

std::string FormatExifTimestamp(std::time_t ts)
{
    if (ts <= 0)
        return {};

    std::tm tmVal{};
    if (localtime_s(&tmVal, &ts) != 0)
        return {};

    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%04d:%02d:%02d %02d:%02d:%02d",
        tmVal.tm_year + 1900,
        tmVal.tm_mon + 1,
        tmVal.tm_mday,
        tmVal.tm_hour,
        tmVal.tm_min,
        tmVal.tm_sec);
    return buf;
}

HRESULT EnsureMetadataBuiltLocked(CR3DecoderState& state)
{
    if (!state.initialized)
        return WINCODEC_ERR_NOTINITIALIZED;

    if (state.metadataAttempted)
        return (state.metaQueryReader && state.metaBlockReader) ? S_OK : WINCODEC_ERR_UNSUPPORTEDOPERATION;

    state.metadataAttempted = true;

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory)
        return FAILED(hr) ? hr : E_FAIL;

    IStream* memStream = nullptr;
    hr = CreateStreamOnHGlobal(nullptr, TRUE, &memStream);
    if (FAILED(hr) || !memStream)
    {
        factory->Release();
        return FAILED(hr) ? hr : E_OUTOFMEMORY;
    }

    IWICBitmapEncoder* encoder = nullptr;
    hr = factory->CreateEncoder(GUID_ContainerFormatTiff, nullptr, &encoder);
    if (SUCCEEDED(hr) && encoder)
    {
        hr = encoder->Initialize(memStream, WICBitmapEncoderNoCache);
        if (SUCCEEDED(hr))
        {
            IWICBitmapFrameEncode* frameEnc = nullptr;
            hr = encoder->CreateNewFrame(&frameEnc, nullptr);
            if (SUCCEEDED(hr) && frameEnc)
            {
                hr = frameEnc->Initialize(nullptr);
                if (SUCCEEDED(hr))
                    hr = frameEnc->SetSize(1, 1);
                WICPixelFormatGUID fmt = GUID_WICPixelFormat24bppBGR;
                if (SUCCEEDED(hr))
                    hr = frameEnc->SetPixelFormat(&fmt);

                if (SUCCEEDED(hr))
                {
                    IWICMetadataQueryWriter* qw = nullptr;
                    if (SUCCEEDED(frameEnc->GetMetadataQueryWriter(&qw)) && qw)
                    {
                        const RawMetadata& m = state.info.metadata;
                        WriteAsciiMeta(qw, L"/ifd/{ushort=270}", m.description);
                        WriteAsciiMeta(qw, L"/ifd/{ushort=271}", m.make);
                        WriteAsciiMeta(qw, L"/ifd/{ushort=272}", m.model);
                        WriteUint16Meta(qw, L"/ifd/{ushort=274}", 1); // Pixels are already oriented upright
                        WriteAsciiMeta(qw, L"/ifd/{ushort=305}", m.software);
                        WriteAsciiMeta(qw, L"/ifd/{ushort=315}", m.artist);
                        WriteAsciiMeta(qw, L"/ifd/{ushort=33432}", m.copyright);
                        if (m.rating >= 0 && m.rating <= 5)
                        {
                            static constexpr uint16_t kRatingPercents[6] = {0, 1, 25, 50, 75, 99};
                            WriteUint16Meta(qw, L"/ifd/{ushort=18246}", static_cast<uint16_t>(m.rating));
                            WriteUint16Meta(qw, L"/ifd/{ushort=18249}", kRatingPercents[m.rating]);
                        }

                        const std::string exifDate = FormatExifTimestamp(m.timestamp);
                        WriteAsciiMeta(qw, L"/ifd/{ushort=306}", exifDate);
                        WriteAsciiMeta(qw, L"/ifd/exif/{ushort=36867}", exifDate);
                        WriteAsciiMeta(qw, L"/ifd/exif/{ushort=36868}", exifDate);
                        WriteAsciiMeta(qw, L"/ifd/exif/{ushort=37521}", m.subSecTimeOriginal);

                        if (m.shutter > 0.0f)
                        {
                            uint32_t num = 0, den = 1;
                            FloatToRational(m.shutter, num, den);
                            WriteRationalMeta(qw, L"/ifd/exif/{ushort=33434}", num, den);
                        }
                        if (m.aperture > 0.0f)
                        {
                            uint32_t num = 0, den = 1;
                            FloatToRational(m.aperture, num, den);
                            WriteRationalMeta(qw, L"/ifd/exif/{ushort=33437}", num, den);
                        }
                        if (m.exposureProgram >= 0 && m.exposureProgram <= 8)
                            WriteUint16Meta(qw, L"/ifd/exif/{ushort=34850}", static_cast<uint16_t>(m.exposureProgram));
                        if (m.isoSpeed > 0.0f && m.isoSpeed <= 65535.0f)
                            WriteUint16Meta(qw, L"/ifd/exif/{ushort=34855}", static_cast<uint16_t>(std::round(m.isoSpeed)));
                        if (m.hasExposureBias)
                            WriteSignedRationalMeta(qw, L"/ifd/exif/{ushort=37380}", m.exposureBiasNum, m.exposureBiasDen);
                        if (m.meteringMode >= 0 && m.meteringMode <= 255)
                            WriteUint16Meta(qw, L"/ifd/exif/{ushort=37383}", static_cast<uint16_t>(m.meteringMode));
                        if (m.hasFlash)
                            WriteUint16Meta(qw, L"/ifd/exif/{ushort=37385}", m.flash);
                        if (m.focalLen > 0.0f)
                        {
                            uint32_t num = 0, den = 1;
                            FloatToRational(m.focalLen, num, den);
                            WriteRationalMeta(qw, L"/ifd/exif/{ushort=37386}", num, den);
                        }
                        WriteUint16Meta(qw, L"/ifd/exif/{ushort=40961}", static_cast<uint16_t>(m.exifColorSpace));
                        if (m.hasWhiteBalance)
                            WriteUint16Meta(qw, L"/ifd/exif/{ushort=41987}", m.whiteBalance);
                        if (m.focalLen35mm > 0)
                            WriteUint16Meta(qw, L"/ifd/exif/{ushort=41989}", m.focalLen35mm);
                        WriteAsciiMeta(qw, L"/ifd/exif/{ushort=42033}", m.bodySerial);
                        WriteAsciiMeta(qw, L"/ifd/exif/{ushort=42035}", m.lensMake);
                        WriteAsciiMeta(qw, L"/ifd/exif/{ushort=42036}", m.lensModel);
                        WriteAsciiMeta(qw, L"/ifd/exif/{ushort=42037}", m.lensSerial);

                        if (m.hasGps)
                        {
                            if (m.gpsLatRef == 'N' || m.gpsLatRef == 'S')
                            {
                                WriteAsciiMeta(qw, L"/ifd/gps/{ushort=1}", std::string(1, m.gpsLatRef));
                                WriteGpsCoordMeta(qw, L"/ifd/gps/{ushort=2}", m.gpsLat);
                            }
                            if (m.gpsLonRef == 'E' || m.gpsLonRef == 'W')
                            {
                                WriteAsciiMeta(qw, L"/ifd/gps/{ushort=3}", std::string(1, m.gpsLonRef));
                                WriteGpsCoordMeta(qw, L"/ifd/gps/{ushort=4}", m.gpsLon);
                            }
                            WriteUint8Meta(qw, L"/ifd/gps/{ushort=5}", (m.gpsAltRef == 1) ? 1 : 0);
                            uint32_t altNum = 0, altDen = 1;
                            FloatToRational(std::abs(m.gpsAltitude), altNum, altDen);
                            WriteRationalMeta(qw, L"/ifd/gps/{ushort=6}", altNum, altDen);
                        }

                        qw->Release();
                    }

                    BYTE stubPixel[3]{0, 0, 0};
                    hr = frameEnc->WritePixels(1, 3, 3, stubPixel);
                    if (SUCCEEDED(hr))
                        hr = frameEnc->Commit();
                    if (SUCCEEDED(hr))
                        hr = encoder->Commit();
                }
                frameEnc->Release();
            }
        }
        encoder->Release();
    }

    if (SUCCEEDED(hr))
    {
        LARGE_INTEGER zero{};
        hr = memStream->Seek(zero, STREAM_SEEK_SET, nullptr);
    }

    if (SUCCEEDED(hr))
    {
        hr = factory->CreateDecoderFromStream(
            memStream, &GUID_VendorMicrosoft, WICDecodeMetadataCacheOnLoad, &state.metaTiffDecoder);
    }
    if (SUCCEEDED(hr) && state.metaTiffDecoder)
        hr = state.metaTiffDecoder->GetFrame(0, &state.metaTiffFrame);
    if (SUCCEEDED(hr) && state.metaTiffFrame)
        hr = state.metaTiffFrame->GetMetadataQueryReader(&state.metaQueryReader);
    if (SUCCEEDED(hr) && state.metaTiffFrame)
        hr = state.metaTiffFrame->QueryInterface(IID_PPV_ARGS(&state.metaBlockReader));

    if (FAILED(hr) || !state.metaQueryReader || !state.metaBlockReader)
    {
        state.releaseMetadataLocked();
        state.metadataAttempted = true;
        hr = WINCODEC_ERR_UNSUPPORTEDOPERATION;
    }

    memStream->Release();
    factory->Release();
    return hr;
}
} // namespace

// ============================================================================
// CR3BitmapSource
// ============================================================================

CR3BitmapSource::CR3BitmapSource(std::shared_ptr<const DecodedImageBGR> image)
    : image_(std::move(image))
{
    ModuleAddRef();
}

CR3BitmapSource::~CR3BitmapSource()
{
    ModuleRelease();
}

HRESULT STDMETHODCALLTYPE CR3BitmapSource::QueryInterface(REFIID riid, void** ppvObject)
{
    if (!ppvObject)
        return E_INVALIDARG;

    *ppvObject = nullptr;
    if (riid == IID_IUnknown || riid == IID_IWICBitmapSource)
    {
        *ppvObject = static_cast<IWICBitmapSource*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CR3BitmapSource::AddRef()
{
    return ++refCount_;
}

ULONG STDMETHODCALLTYPE CR3BitmapSource::Release()
{
    const ULONG remaining = --refCount_;
    if (remaining == 0)
        delete this;
    return remaining;
}

HRESULT STDMETHODCALLTYPE CR3BitmapSource::GetSize(UINT* puiWidth, UINT* puiHeight)
{
    if (!puiWidth || !puiHeight)
        return E_INVALIDARG;

    *puiWidth = image_->width;
    *puiHeight = image_->height;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3BitmapSource::GetPixelFormat(WICPixelFormatGUID* pPixelFormat)
{
    if (!pPixelFormat)
        return E_INVALIDARG;

    *pPixelFormat = image_->pixelFormat;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3BitmapSource::GetResolution(double* pDpiX, double* pDpiY)
{
    if (!pDpiX || !pDpiY)
        return E_INVALIDARG;

    *pDpiX = 96.0;
    *pDpiY = 96.0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3BitmapSource::CopyPalette(IWICPalette* /*pIPalette*/)
{
    return WINCODEC_ERR_PALETTEUNAVAILABLE;
}

HRESULT STDMETHODCALLTYPE CR3BitmapSource::CopyPixels(const WICRect* prc, UINT cbStride, UINT cbBufferSize, BYTE* pbBuffer)
{
    return CopyImageRect(*image_, prc, cbStride, cbBufferSize, pbBuffer);
}

// ============================================================================
// CR3FrameDecode
// ============================================================================

CR3FrameDecode::CR3FrameDecode(std::shared_ptr<CR3DecoderState> state)
    : state_(std::move(state))
{
    ModuleAddRef();
}

CR3FrameDecode::~CR3FrameDecode()
{
    ModuleRelease();
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::QueryInterface(REFIID riid, void** ppvObject)
{
    if (!ppvObject)
        return E_INVALIDARG;

    *ppvObject = nullptr;
    if (riid == IID_IUnknown || riid == IID_IWICBitmapSource || riid == IID_IWICBitmapFrameDecode)
    {
        *ppvObject = static_cast<IWICBitmapFrameDecode*>(this);
        AddRef();
        return S_OK;
    }
    if (riid == IID_IWICBitmapSourceTransform)
    {
        *ppvObject = static_cast<IWICBitmapSourceTransform*>(this);
        AddRef();
        return S_OK;
    }
    if (riid == IID_IWICMetadataBlockReader)
    {
        *ppvObject = static_cast<IWICMetadataBlockReader*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CR3FrameDecode::AddRef()
{
    return ++refCount_;
}

ULONG STDMETHODCALLTYPE CR3FrameDecode::Release()
{
    const ULONG remaining = --refCount_;
    if (remaining == 0)
        delete this;
    return remaining;
}

HRESULT CR3FrameDecode::getOrDecodeFullImage(std::shared_ptr<const DecodedImageBGR>& outImage)
{
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->initialized || !state_->stream)
        return WINCODEC_ERR_NOTINITIALIZED;

    if (!state_->fullImage)
    {
        auto decoded = std::make_shared<DecodedImageBGR>();
        const HRESULT hr = LibRawProcessor::DecodeToImage(state_->stream, CR3DecodeMode::FullRaw, *decoded);
        if (FAILED(hr))
            return hr;

        state_->info.width = decoded->width;
        state_->info.height = decoded->height;
        state_->info.halfWidth = (decoded->width >= 2) ? (decoded->width / 2u) : decoded->width;
        state_->info.halfHeight = (decoded->height >= 2) ? (decoded->height / 2u) : decoded->height;
        state_->fullImage = std::move(decoded);
    }

    outImage = state_->fullImage;
    return S_OK;
}

HRESULT CR3FrameDecode::getOrDecodeHalfImage(std::shared_ptr<const DecodedImageBGR>& outImage)
{
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->initialized || !state_->stream)
        return WINCODEC_ERR_NOTINITIALIZED;

    if (!state_->thumbnail)
    {
        if (state_->fullImage)
            state_->thumbnail = DownsampleHalf2x2To24bpp(*state_->fullImage);

        if (!state_->thumbnail && state_->info.fastEmbeddedThumb && state_->info.embeddedJpegLength > 0)
        {
            auto fastThumb = std::make_shared<DecodedImageBGR>();
            if (SUCCEEDED(LibRawProcessor::DecodeEmbeddedJpegFromStream(
                    state_->stream,
                    state_->info.embeddedJpegOffset,
                    state_->info.embeddedJpegLength,
                    state_->info.embeddedJpegFlip,
                    *fastThumb)))
            {
                state_->thumbnail = std::move(fastThumb);
            }
        }

        if (!state_->thumbnail)
        {
            auto decodedThumb = std::make_shared<DecodedImageBGR>();
            const HRESULT hr = LibRawProcessor::DecodeToImage(state_->stream, CR3DecodeMode::HalfSizeRaw, *decodedThumb);
            if (FAILED(hr))
                return hr;
            state_->thumbnail = std::move(decodedThumb);
        }
    }

    outImage = state_->thumbnail;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetSize(UINT* puiWidth, UINT* puiHeight)
{
    if (!puiWidth || !puiHeight)
        return E_INVALIDARG;

    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->initialized)
        return WINCODEC_ERR_NOTINITIALIZED;

    *puiWidth = state_->info.width;
    *puiHeight = state_->info.height;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetPixelFormat(WICPixelFormatGUID* pPixelFormat)
{
    if (!pPixelFormat)
        return E_INVALIDARG;

    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->initialized)
        return WINCODEC_ERR_NOTINITIALIZED;

    *pPixelFormat = (state_->info.outputBps == 16) ? GUID_WICPixelFormat48bppBGR : GUID_WICPixelFormat24bppBGR;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetResolution(double* pDpiX, double* pDpiY)
{
    if (!pDpiX || !pDpiY)
        return E_INVALIDARG;

    *pDpiX = 96.0;
    *pDpiY = 96.0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::CopyPalette(IWICPalette* /*pIPalette*/)
{
    return WINCODEC_ERR_PALETTEUNAVAILABLE;
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::CopyPixels(const WICRect* prc, UINT cbStride, UINT cbBufferSize, BYTE* pbBuffer)
{
    if (!pbBuffer)
        return E_INVALIDARG;

    std::shared_ptr<const DecodedImageBGR> fullImage;
    const HRESULT hr = getOrDecodeFullImage(fullImage);
    if (FAILED(hr) || !fullImage)
        return hr;

    return CopyImageRect(*fullImage, prc, cbStride, cbBufferSize, pbBuffer);
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetMetadataQueryReader(IWICMetadataQueryReader** ppIMetadataQueryReader)
{
    if (!ppIMetadataQueryReader)
        return E_INVALIDARG;

    *ppIMetadataQueryReader = nullptr;

    std::lock_guard<std::mutex> lock(state_->mutex);
    const HRESULT hr = EnsureMetadataBuiltLocked(*state_);
    if (FAILED(hr))
        return hr;

    state_->metaQueryReader->AddRef();
    *ppIMetadataQueryReader = state_->metaQueryReader;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetColorContexts(UINT cCount, IWICColorContext** ppIColorContexts, UINT* pcActualCount)
{
    int cs = 1;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        cs = state_->info.metadata.exifColorSpace;
    }
    return ProvideColorContext(cs, cCount, ppIColorContexts, pcActualCount);
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetThumbnail(IWICBitmapSource** ppIThumbnail)
{
    if (!ppIThumbnail)
        return E_INVALIDARG;

    *ppIThumbnail = nullptr;

    std::shared_ptr<const DecodedImageBGR> thumbPtr;
    const HRESULT hr = getOrDecodeHalfImage(thumbPtr);
    if (FAILED(hr) || !thumbPtr)
        return hr;

    CR3BitmapSource* source = new (std::nothrow) CR3BitmapSource(std::move(thumbPtr));
    if (!source)
        return E_OUTOFMEMORY;

    *ppIThumbnail = source;
    return S_OK;
}

// ============================================================================
// CR3FrameDecode — IWICBitmapSourceTransform
// ============================================================================

HRESULT STDMETHODCALLTYPE CR3FrameDecode::CopyPixels(
    const WICRect* prc,
    UINT uiWidth,
    UINT uiHeight,
    WICPixelFormatGUID* pguidDstFormat,
    WICBitmapTransformOptions dstTransform,
    UINT nStride,
    UINT cbBufferSize,
    BYTE* pbBuffer)
{
    if (!pbBuffer || uiWidth == 0 || uiHeight == 0)
        return E_INVALIDARG;

    constexpr DWORD kValidTransformMask =
        WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal | WICBitmapTransformFlipVertical;
    if ((static_cast<DWORD>(dstTransform) & ~kValidTransformMask) != 0)
        return WINCODEC_ERR_UNSUPPORTEDOPERATION;

    UINT fullW = 0, fullH = 0, halfW = 0, halfH = 0;
    WICPixelFormatGUID nativeFmt = GUID_WICPixelFormat24bppBGR;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (!state_->initialized)
            return WINCODEC_ERR_NOTINITIALIZED;
        fullW = state_->info.width;
        fullH = state_->info.height;
        halfW = state_->info.halfWidth;
        halfH = state_->info.halfHeight;
        nativeFmt = (state_->info.outputBps == 16) ? GUID_WICPixelFormat48bppBGR : GUID_WICPixelFormat24bppBGR;
    }

    const WICPixelFormatGUID targetFmt = pguidDstFormat ? *pguidDstFormat : nativeFmt;
    if (targetFmt != GUID_WICPixelFormat24bppBGR && targetFmt != GUID_WICPixelFormat48bppBGR)
        return WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;

    std::shared_ptr<const DecodedImageBGR> srcImg;
    HRESULT hr = S_OK;
    if (halfW > 0 && halfH > 0 && uiWidth <= halfW && uiHeight <= halfH)
        hr = getOrDecodeHalfImage(srcImg);
    else
        hr = getOrDecodeFullImage(srcImg);

    if (FAILED(hr) || !srcImg)
        return hr;

    // Fast direct copy when size, format, and orientation already match
    if (dstTransform == WICBitmapTransformRotate0 &&
        srcImg->width == uiWidth && srcImg->height == uiHeight && srcImg->pixelFormat == targetFmt)
    {
        return CopyImageRect(*srcImg, prc, nStride, cbBufferSize, pbBuffer);
    }

    // Canonical WIC transform order: Scale (uiWidth x uiHeight) -> Crop (prc) -> Flip/Rotate (dstTransform)
    CR3BitmapSource* rawSource = new (std::nothrow) CR3BitmapSource(srcImg);
    if (!rawSource)
        return E_OUTOFMEMORY;

    IWICImagingFactory* factory = nullptr;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory)
    {
        rawSource->Release();
        return FAILED(hr) ? hr : E_FAIL;
    }

    IWICBitmapSource* pipeline = rawSource;
    IWICBitmapScaler* scaler = nullptr;
    if (srcImg->width != uiWidth || srcImg->height != uiHeight)
    {
        hr = factory->CreateBitmapScaler(&scaler);
        if (SUCCEEDED(hr) && scaler)
            hr = scaler->Initialize(pipeline, uiWidth, uiHeight, WICBitmapInterpolationModeFant);
        if (SUCCEEDED(hr) && scaler)
            pipeline = scaler;
    }

    IWICBitmapClipper* clipper = nullptr;
    const WICRect* finalCopyRect = prc;
    if (SUCCEEDED(hr) && prc && dstTransform != WICBitmapTransformRotate0)
    {
        hr = factory->CreateBitmapClipper(&clipper);
        if (SUCCEEDED(hr) && clipper)
            hr = clipper->Initialize(pipeline, prc);
        if (SUCCEEDED(hr) && clipper)
        {
            pipeline = clipper;
            finalCopyRect = nullptr;
        }
    }

    IWICBitmapFlipRotator* rotator = nullptr;
    if (SUCCEEDED(hr) && dstTransform != WICBitmapTransformRotate0)
    {
        hr = factory->CreateBitmapFlipRotator(&rotator);
        if (SUCCEEDED(hr) && rotator)
            hr = rotator->Initialize(pipeline, dstTransform);
        if (SUCCEEDED(hr) && rotator)
            pipeline = rotator;
    }

    IWICFormatConverter* conv = nullptr;
    WICPixelFormatGUID pipelineFmt = srcImg->pixelFormat;
    if (SUCCEEDED(hr) && pipelineFmt != targetFmt)
    {
        hr = factory->CreateFormatConverter(&conv);
        if (SUCCEEDED(hr) && conv)
        {
            hr = conv->Initialize(
                pipeline, targetFmt, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
        }
        if (SUCCEEDED(hr) && conv)
            pipeline = conv;
    }

    if (SUCCEEDED(hr))
        hr = pipeline->CopyPixels(finalCopyRect, nStride, cbBufferSize, pbBuffer);

    if (conv)
        conv->Release();
    if (rotator)
        rotator->Release();
    if (clipper)
        clipper->Release();
    if (scaler)
        scaler->Release();
    rawSource->Release();
    factory->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetClosestSize(UINT* puiWidth, UINT* puiHeight)
{
    if (!puiWidth || !puiHeight)
        return E_INVALIDARG;

    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->initialized)
        return WINCODEC_ERR_NOTINITIALIZED;

    if (*puiWidth == 0 || *puiHeight == 0)
        return E_INVALIDARG;

    // Report exact full or half-size native resolution when target fits within half-size;
    // CopyPixels() can also satisfy arbitrary intermediate sizes via WICBitmapScaler.
    if (state_->info.halfWidth > 0 && state_->info.halfHeight > 0 &&
        *puiWidth <= state_->info.halfWidth && *puiHeight <= state_->info.halfHeight)
    {
        *puiWidth = state_->info.halfWidth;
        *puiHeight = state_->info.halfHeight;
    }
    else
    {
        *puiWidth = state_->info.width;
        *puiHeight = state_->info.height;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetClosestPixelFormat(WICPixelFormatGUID* pguidDstFormat)
{
    if (!pguidDstFormat)
        return E_INVALIDARG;

    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->initialized)
        return WINCODEC_ERR_NOTINITIALIZED;

    if (*pguidDstFormat == GUID_WICPixelFormat48bppBGR && state_->info.outputBps == 16)
        *pguidDstFormat = GUID_WICPixelFormat48bppBGR;
    else
        *pguidDstFormat = GUID_WICPixelFormat24bppBGR;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::DoesSupportTransform(WICBitmapTransformOptions dstTransform, BOOL* pfIsSupported)
{
    if (!pfIsSupported)
        return E_INVALIDARG;

    constexpr DWORD kValidTransformMask =
        WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal | WICBitmapTransformFlipVertical;
    *pfIsSupported = ((static_cast<DWORD>(dstTransform) & ~kValidTransformMask) == 0) ? TRUE : FALSE;
    return S_OK;
}

// ============================================================================
// CR3FrameDecode — IWICMetadataBlockReader
// ============================================================================

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetContainerFormat(GUID* pguidContainerFormat)
{
    if (!pguidContainerFormat)
        return E_INVALIDARG;

    *pguidContainerFormat = GUID_ContainerFormatCR3;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetCount(UINT* pcCount)
{
    if (!pcCount)
        return E_INVALIDARG;

    *pcCount = 0;

    std::lock_guard<std::mutex> lock(state_->mutex);
    const HRESULT hr = EnsureMetadataBuiltLocked(*state_);
    if (FAILED(hr))
        return hr;

    return state_->metaBlockReader->GetCount(pcCount);
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetReaderByIndex(UINT nIndex, IWICMetadataReader** ppIMetadataReader)
{
    if (!ppIMetadataReader)
        return E_INVALIDARG;

    *ppIMetadataReader = nullptr;

    std::lock_guard<std::mutex> lock(state_->mutex);
    const HRESULT hr = EnsureMetadataBuiltLocked(*state_);
    if (FAILED(hr))
        return hr;

    return state_->metaBlockReader->GetReaderByIndex(nIndex, ppIMetadataReader);
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetEnumerator(IEnumUnknown** ppIEnumMetadata)
{
    if (!ppIEnumMetadata)
        return E_INVALIDARG;

    *ppIEnumMetadata = nullptr;

    std::lock_guard<std::mutex> lock(state_->mutex);
    const HRESULT hr = EnsureMetadataBuiltLocked(*state_);
    if (FAILED(hr))
        return hr;

    return state_->metaBlockReader->GetEnumerator(ppIEnumMetadata);
}

// ============================================================================
// CR3Decoder
// ============================================================================

CR3Decoder::CR3Decoder()
    : state_(std::make_shared<CR3DecoderState>())
{
    ModuleAddRef();
}

CR3Decoder::~CR3Decoder()
{
    ModuleRelease();
}

HRESULT STDMETHODCALLTYPE CR3Decoder::QueryInterface(REFIID riid, void** ppvObject)
{
    if (!ppvObject)
        return E_INVALIDARG;

    *ppvObject = nullptr;
    if (riid == IID_IUnknown || riid == IID_IWICBitmapDecoder)
    {
        *ppvObject = static_cast<IWICBitmapDecoder*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CR3Decoder::AddRef()
{
    return ++refCount_;
}

ULONG STDMETHODCALLTYPE CR3Decoder::Release()
{
    const ULONG remaining = --refCount_;
    if (remaining == 0)
        delete this;
    return remaining;
}

HRESULT STDMETHODCALLTYPE CR3Decoder::QueryCapability(IStream* pIStream, DWORD* pdwCapability)
{
    if (!pIStream || !pdwCapability)
        return E_INVALIDARG;

    *pdwCapability = 0;
    if (!LibRawProcessor::ProbeStream(pIStream))
        return WINCODEC_ERR_COMPONENTNOTFOUND;

    *pdwCapability = WICBitmapDecoderCapabilityCanDecodeAllImages |
                     WICBitmapDecoderCapabilityCanDecodeThumbnail;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3Decoder::Initialize(IStream* pIStream, WICDecodeOptions /*cacheOptions*/)
{
    if (!pIStream)
        return E_INVALIDARG;

    std::lock_guard<std::mutex> lock(state_->mutex);

    if (state_->stream)
    {
        state_->stream->Release();
        state_->stream = nullptr;
    }
    state_->initialized = false;
    state_->thumbnail.reset();
    state_->preview.reset();
    state_->fullImage.reset();
    state_->releaseMetadataLocked();
    state_->info = {};

    RawImageInfo info{};
    const HRESULT hr = LibRawProcessor::ReadHeader(pIStream, info);
    if (FAILED(hr))
        return hr;

    pIStream->AddRef();
    state_->stream = pIStream;
    state_->info = info;
    state_->initialized = true;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3Decoder::GetContainerFormat(GUID* pguidContainerFormat)
{
    if (!pguidContainerFormat)
        return E_INVALIDARG;

    *pguidContainerFormat = GUID_ContainerFormatCR3;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3Decoder::GetDecoderInfo(IWICBitmapDecoderInfo** ppIDecoderInfo)
{
    if (!ppIDecoderInfo)
        return E_INVALIDARG;

    *ppIDecoderInfo = nullptr;

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
        return hr;

    IWICComponentInfo* compInfo = nullptr;
    hr = factory->CreateComponentInfo(CLSID_CR3WICDecoder, &compInfo);
    if (SUCCEEDED(hr))
    {
        hr = compInfo->QueryInterface(IID_PPV_ARGS(ppIDecoderInfo));
        compInfo->Release();
    }

    factory->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE CR3Decoder::CopyPalette(IWICPalette* /*pIPalette*/)
{
    return WINCODEC_ERR_PALETTEUNAVAILABLE;
}

HRESULT STDMETHODCALLTYPE CR3Decoder::GetMetadataQueryReader(IWICMetadataQueryReader** ppIMetadataQueryReader)
{
    if (!ppIMetadataQueryReader)
        return E_INVALIDARG;

    *ppIMetadataQueryReader = nullptr;

    std::lock_guard<std::mutex> lock(state_->mutex);
    const HRESULT hr = EnsureMetadataBuiltLocked(*state_);
    if (FAILED(hr))
        return hr;

    state_->metaQueryReader->AddRef();
    *ppIMetadataQueryReader = state_->metaQueryReader;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3Decoder::GetPreview(IWICBitmapSource** ppIBitmapSource)
{
    if (!ppIBitmapSource)
        return E_INVALIDARG;

    *ppIBitmapSource = nullptr;

    std::shared_ptr<const DecodedImageBGR> previewPtr;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (!state_->initialized || !state_->stream)
            return WINCODEC_ERR_NOTINITIALIZED;

        if (!state_->preview)
        {
            if (state_->fullImage)
                state_->preview = DownsampleHalf2x2To24bpp(*state_->fullImage);

            if (!state_->preview && state_->info.embeddedJpegLength > 0)
            {
                auto fastPreview = std::make_shared<DecodedImageBGR>();
                if (SUCCEEDED(LibRawProcessor::DecodeEmbeddedJpegFromStream(
                        state_->stream,
                        state_->info.embeddedJpegOffset,
                        state_->info.embeddedJpegLength,
                        state_->info.embeddedJpegFlip,
                        *fastPreview)))
                {
                    state_->preview = std::move(fastPreview);
                }
            }

            if (!state_->preview)
            {
                auto decodedPreview = std::make_shared<DecodedImageBGR>();
                const HRESULT hr = LibRawProcessor::DecodeToImage(
                    state_->stream, CR3DecodeMode::PreviewJpeg, *decodedPreview);
                if (FAILED(hr))
                    return hr;
                state_->preview = std::move(decodedPreview);
            }
        }
        previewPtr = state_->preview;
    }

    CR3BitmapSource* source = new (std::nothrow) CR3BitmapSource(std::move(previewPtr));
    if (!source)
        return E_OUTOFMEMORY;

    *ppIBitmapSource = source;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3Decoder::GetColorContexts(UINT cCount, IWICColorContext** ppIColorContexts, UINT* pcActualCount)
{
    int cs = 1;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        cs = state_->info.metadata.exifColorSpace;
    }
    return ProvideColorContext(cs, cCount, ppIColorContexts, pcActualCount);
}

HRESULT STDMETHODCALLTYPE CR3Decoder::GetThumbnail(IWICBitmapSource** ppIThumbnail)
{
    if (!ppIThumbnail)
        return E_INVALIDARG;

    *ppIThumbnail = nullptr;

    IWICBitmapFrameDecode* frame = nullptr;
    HRESULT hr = GetFrame(0, &frame);
    if (FAILED(hr))
        return hr;

    hr = frame->GetThumbnail(ppIThumbnail);
    frame->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE CR3Decoder::GetFrameCount(UINT* pCount)
{
    if (!pCount)
        return E_INVALIDARG;

    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->initialized)
        return WINCODEC_ERR_NOTINITIALIZED;

    *pCount = 1;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CR3Decoder::GetFrame(UINT index, IWICBitmapFrameDecode** ppIBitmapFrame)
{
    if (!ppIBitmapFrame)
        return E_INVALIDARG;

    *ppIBitmapFrame = nullptr;

    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->initialized)
        return WINCODEC_ERR_NOTINITIALIZED;

    if (index != 0)
        return WINCODEC_ERR_FRAMEMISSING;

    CR3FrameDecode* frame = new (std::nothrow) CR3FrameDecode(state_);
    if (!frame)
        return E_OUTOFMEMORY;

    *ppIBitmapFrame = frame;
    return S_OK;
}
