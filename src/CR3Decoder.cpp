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
} // namespace

// ============================================================================
// CR3BitmapSource
// ============================================================================

CR3BitmapSource::CR3BitmapSource(DecodedImageBGR24 image)
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

    *puiWidth = image_.width;
    *puiHeight = image_.height;
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
    return CopyBGR24Rect(image_, prc, cbStride, cbBufferSize, pbBuffer);
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

HRESULT CR3FrameDecode::ensureFullDecodedLocked()
{
    if (!state_->initialized || !state_->stream)
        return WINCODEC_ERR_NOTINITIALIZED;

    if (state_->hasFullImage)
        return S_OK;

    const HRESULT hr = LibRawProcessor::DecodeToBGR24(state_->stream, false /* full-res */, state_->fullImage);
    if (FAILED(hr))
        return hr;

    state_->info.width = state_->fullImage.width;
    state_->info.height = state_->fullImage.height;
    state_->hasFullImage = true;
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

    std::lock_guard<std::mutex> lock(state_->mutex);
    const HRESULT hr = ensureFullDecodedLocked();
    if (FAILED(hr))
        return hr;

    return CopyBGR24Rect(state_->fullImage, prc, cbStride, cbBufferSize, pbBuffer);
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

    DecodedImageBGR24 thumbCopy;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (!state_->initialized || !state_->stream)
            return WINCODEC_ERR_NOTINITIALIZED;

        if (!state_->hasThumbnail)
        {
            const HRESULT hr = LibRawProcessor::DecodeToBGR24(state_->stream, true /* half_size = 1 */, state_->thumbnail);
            if (FAILED(hr))
                return hr;
            state_->hasThumbnail = true;
        }
        thumbCopy = state_->thumbnail;
    }

    CR3BitmapSource* source = new (std::nothrow) CR3BitmapSource(std::move(thumbCopy));
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
    state_->hasThumbnail = false;
    state_->hasFullImage = false;
    state_->thumbnail = {};
    state_->fullImage = {};
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
