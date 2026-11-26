#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <atomic>
#include <memory>
#include <mutex>
#include "LibRawProcessor.h"

struct CR3DecoderState
{
    std::mutex mutex;
    IStream* stream = nullptr;
    RawImageInfo info{};
    bool initialized = false;

    std::shared_ptr<const DecodedImageBGR> thumbnail;
    std::shared_ptr<const DecodedImageBGR> preview;
    std::shared_ptr<const DecodedImageBGR> fullImage;

    bool metadataAttempted = false;
    IWICBitmapDecoder* metaTiffDecoder = nullptr;
    IWICBitmapFrameDecode* metaTiffFrame = nullptr;
    IWICMetadataQueryReader* metaQueryReader = nullptr;
    IWICMetadataBlockReader* metaBlockReader = nullptr;

    void releaseMetadataLocked()
    {
        if (metaBlockReader)
        {
            metaBlockReader->Release();
            metaBlockReader = nullptr;
        }
        if (metaQueryReader)
        {
            metaQueryReader->Release();
            metaQueryReader = nullptr;
        }
        if (metaTiffFrame)
        {
            metaTiffFrame->Release();
            metaTiffFrame = nullptr;
        }
        if (metaTiffDecoder)
        {
            metaTiffDecoder->Release();
            metaTiffDecoder = nullptr;
        }
        metadataAttempted = false;
    }

    ~CR3DecoderState()
    {
        releaseMetadataLocked();
        if (stream)
        {
            stream->Release();
            stream = nullptr;
        }
    }
};

// Lightweight IWICBitmapSource backed by a shared 24bppBGR or 48bppBGR buffer (zero-copy).
class CR3BitmapSource final : public IWICBitmapSource
{
public:
    explicit CR3BitmapSource(std::shared_ptr<const DecodedImageBGR> image);
    ~CR3BitmapSource();

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IWICBitmapSource
    HRESULT STDMETHODCALLTYPE GetSize(UINT* puiWidth, UINT* puiHeight) override;
    HRESULT STDMETHODCALLTYPE GetPixelFormat(WICPixelFormatGUID* pPixelFormat) override;
    HRESULT STDMETHODCALLTYPE GetResolution(double* pDpiX, double* pDpiY) override;
    HRESULT STDMETHODCALLTYPE CopyPalette(IWICPalette* pIPalette) override;
    HRESULT STDMETHODCALLTYPE CopyPixels(const WICRect* prc, UINT cbStride, UINT cbBufferSize, BYTE* pbBuffer) override;

private:
    std::atomic<ULONG> refCount_{1};
    std::shared_ptr<const DecodedImageBGR> image_;
};

// IWICBitmapFrameDecode + IWICBitmapSourceTransform + IWICMetadataBlockReader for frame 0 of a Canon CR3 file.
class CR3FrameDecode final
    : public IWICBitmapFrameDecode
    , public IWICBitmapSourceTransform
    , public IWICMetadataBlockReader
{
public:
    explicit CR3FrameDecode(std::shared_ptr<CR3DecoderState> state);
    ~CR3FrameDecode();

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IWICBitmapSource
    HRESULT STDMETHODCALLTYPE GetSize(UINT* puiWidth, UINT* puiHeight) override;
    HRESULT STDMETHODCALLTYPE GetPixelFormat(WICPixelFormatGUID* pPixelFormat) override;
    HRESULT STDMETHODCALLTYPE GetResolution(double* pDpiX, double* pDpiY) override;
    HRESULT STDMETHODCALLTYPE CopyPalette(IWICPalette* pIPalette) override;
    HRESULT STDMETHODCALLTYPE CopyPixels(const WICRect* prc, UINT cbStride, UINT cbBufferSize, BYTE* pbBuffer) override;

    // IWICBitmapFrameDecode
    HRESULT STDMETHODCALLTYPE GetMetadataQueryReader(IWICMetadataQueryReader** ppIMetadataQueryReader) override;
    HRESULT STDMETHODCALLTYPE GetColorContexts(UINT cCount, IWICColorContext** ppIColorContexts, UINT* pcActualCount) override;
    HRESULT STDMETHODCALLTYPE GetThumbnail(IWICBitmapSource** ppIThumbnail) override;

    // IWICBitmapSourceTransform
    HRESULT STDMETHODCALLTYPE CopyPixels(
        const WICRect* prc,
        UINT uiWidth,
        UINT uiHeight,
        WICPixelFormatGUID* pguidDstFormat,
        WICBitmapTransformOptions dstTransform,
        UINT nStride,
        UINT cbBufferSize,
        BYTE* pbBuffer) override;
    HRESULT STDMETHODCALLTYPE GetClosestSize(UINT* puiWidth, UINT* puiHeight) override;
    HRESULT STDMETHODCALLTYPE GetClosestPixelFormat(WICPixelFormatGUID* pguidDstFormat) override;
    HRESULT STDMETHODCALLTYPE DoesSupportTransform(WICBitmapTransformOptions dstTransform, BOOL* pfIsSupported) override;

    // IWICMetadataBlockReader
    HRESULT STDMETHODCALLTYPE GetContainerFormat(GUID* pguidContainerFormat) override;
    HRESULT STDMETHODCALLTYPE GetCount(UINT* pcCount) override;
    HRESULT STDMETHODCALLTYPE GetReaderByIndex(UINT nIndex, IWICMetadataReader** ppIMetadataReader) override;
    HRESULT STDMETHODCALLTYPE GetEnumerator(IEnumUnknown** ppIEnumMetadata) override;

private:
    HRESULT getOrDecodeFullImage(std::shared_ptr<const DecodedImageBGR>& outImage);
    HRESULT getOrDecodeHalfImage(std::shared_ptr<const DecodedImageBGR>& outImage);

    std::atomic<ULONG> refCount_{1};
    std::shared_ptr<CR3DecoderState> state_;
};

// IWICBitmapDecoder implementation for Canon CR3 RAW files.
class CR3Decoder final : public IWICBitmapDecoder
{
public:
    CR3Decoder();
    ~CR3Decoder();

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IWICBitmapDecoder
    HRESULT STDMETHODCALLTYPE QueryCapability(IStream* pIStream, DWORD* pdwCapability) override;
    HRESULT STDMETHODCALLTYPE Initialize(IStream* pIStream, WICDecodeOptions cacheOptions) override;
    HRESULT STDMETHODCALLTYPE GetContainerFormat(GUID* pguidContainerFormat) override;
    HRESULT STDMETHODCALLTYPE GetDecoderInfo(IWICBitmapDecoderInfo** ppIDecoderInfo) override;
    HRESULT STDMETHODCALLTYPE CopyPalette(IWICPalette* pIPalette) override;
    HRESULT STDMETHODCALLTYPE GetMetadataQueryReader(IWICMetadataQueryReader** ppIMetadataQueryReader) override;
    HRESULT STDMETHODCALLTYPE GetPreview(IWICBitmapSource** ppIBitmapSource) override;
    HRESULT STDMETHODCALLTYPE GetColorContexts(UINT cCount, IWICColorContext** ppIColorContexts, UINT* pcActualCount) override;
    HRESULT STDMETHODCALLTYPE GetThumbnail(IWICBitmapSource** ppIThumbnail) override;
    HRESULT STDMETHODCALLTYPE GetFrameCount(UINT* pCount) override;
    HRESULT STDMETHODCALLTYPE GetFrame(UINT index, IWICBitmapFrameDecode** ppIBitmapFrame) override;

private:
    std::atomic<ULONG> refCount_{1};
    std::shared_ptr<CR3DecoderState> state_;
};
