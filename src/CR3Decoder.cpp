#include "CR3Decoder.h"
#include "ClassFactory.h"
#include "Guid.h"
#include <algorithm>
#include <cstring>
#include <new>

namespace
{
HRESULT CopyBGR24Rect(const DecodedImageBGR24& img, const WICRect* prc, UINT cbStride, UINT cbBufferSize, BYTE* pbBuffer)
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

    const UINT rowBytes = static_cast<UINT>(rect.Width) * 3u;
    if (cbStride < rowBytes)
        return E_INVALIDARG;

    const uint64_t requiredBytes = static_cast<uint64_t>(rect.Height - 1) * cbStride + rowBytes;
    if (cbBufferSize < requiredBytes)
        return WINCODEC_ERR_INSUFFICIENTBUFFER;

    const size_t srcXOffset = static_cast<size_t>(rect.X) * 3u;
    for (INT row = 0; row < rect.Height; ++row)
    {
        const uint8_t* src = img.pixels.data() + static_cast<size_t>(rect.Y + row) * img.stride + srcXOffset;
        BYTE* dst = pbBuffer + static_cast<size_t>(row) * cbStride;
        std::memcpy(dst, src, rowBytes);
    }

    return S_OK;
}

HRESULT ProvideSrgbColorContext(UINT cCount, IWICColorContext** ppIColorContexts, UINT* pcActualCount)
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

    return ppIColorContexts[0]->InitializeFromExifColorSpace(1); // 1 = sRGB
}

std::shared_ptr<const DecodedImageBGR24> DownsampleHalf2x2(const DecodedImageBGR24& src)
{
    if (src.width < 2 || src.height < 2)
        return nullptr;

    auto dst = std::make_shared<DecodedImageBGR24>();
    dst->width = src.width / 2u;
    dst->height = src.height / 2u;
    dst->stride = dst->width * 3u;
    dst->pixels.resize(static_cast<size_t>(dst->stride) * dst->height);

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
} // namespace

// ============================================================================
// CR3BitmapSource
// ============================================================================

CR3BitmapSource::CR3BitmapSource(std::shared_ptr<const DecodedImageBGR24> image)
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

    *pPixelFormat = GUID_WICPixelFormat24bppBGR;
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
    return CopyBGR24Rect(*image_, prc, cbStride, cbBufferSize, pbBuffer);
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

HRESULT CR3FrameDecode::getOrDecodeFullImage(std::shared_ptr<const DecodedImageBGR24>& outImage)
{
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->initialized || !state_->stream)
        return WINCODEC_ERR_NOTINITIALIZED;

    if (!state_->fullImage)
    {
        auto decoded = std::make_shared<DecodedImageBGR24>();
        const HRESULT hr = LibRawProcessor::DecodeToBGR24(state_->stream, false /* full-res */, *decoded);
        if (FAILED(hr))
            return hr;

        state_->info.width = decoded->width;
        state_->info.height = decoded->height;
        state_->fullImage = std::move(decoded);
    }

    outImage = state_->fullImage;
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

    *pPixelFormat = GUID_WICPixelFormat24bppBGR;
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

    std::shared_ptr<const DecodedImageBGR24> fullImage;
    const HRESULT hr = getOrDecodeFullImage(fullImage);
    if (FAILED(hr) || !fullImage)
        return hr;

    return CopyBGR24Rect(*fullImage, prc, cbStride, cbBufferSize, pbBuffer);
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetMetadataQueryReader(IWICMetadataQueryReader** ppIMetadataQueryReader)
{
    if (ppIMetadataQueryReader)
        *ppIMetadataQueryReader = nullptr;
    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetColorContexts(UINT cCount, IWICColorContext** ppIColorContexts, UINT* pcActualCount)
{
    return ProvideSrgbColorContext(cCount, ppIColorContexts, pcActualCount);
}

HRESULT STDMETHODCALLTYPE CR3FrameDecode::GetThumbnail(IWICBitmapSource** ppIThumbnail)
{
    if (!ppIThumbnail)
        return E_INVALIDARG;

    *ppIThumbnail = nullptr;

    std::shared_ptr<const DecodedImageBGR24> thumbPtr;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (!state_->initialized || !state_->stream)
            return WINCODEC_ERR_NOTINITIALIZED;

        if (!state_->thumbnail)
        {
            if (state_->fullImage)
            {
                // Reuse already-decoded full RAW image via fast 2x2 downsample
                state_->thumbnail = DownsampleHalf2x2(*state_->fullImage);
            }

            if (!state_->thumbnail)
            {
                auto decodedThumb = std::make_shared<DecodedImageBGR24>();
                const HRESULT hr = LibRawProcessor::DecodeToBGR24(state_->stream, true /* half_size = 1 */, *decodedThumb);
                if (FAILED(hr))
                    return hr;
                state_->thumbnail = std::move(decodedThumb);
            }
        }
        thumbPtr = state_->thumbnail;
    }

    CR3BitmapSource* source = new (std::nothrow) CR3BitmapSource(std::move(thumbPtr));
    if (!source)
        return E_OUTOFMEMORY;

    *ppIThumbnail = source;
    return S_OK;
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
    state_->fullImage.reset();
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
    if (ppIMetadataQueryReader)
        *ppIMetadataQueryReader = nullptr;
    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

HRESULT STDMETHODCALLTYPE CR3Decoder::GetPreview(IWICBitmapSource** ppIBitmapSource)
{
    if (ppIBitmapSource)
        *ppIBitmapSource = nullptr;
    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

HRESULT STDMETHODCALLTYPE CR3Decoder::GetColorContexts(UINT cCount, IWICColorContext** ppIColorContexts, UINT* pcActualCount)
{
    return ProvideSrgbColorContext(cCount, ppIColorContexts, pcActualCount);
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
