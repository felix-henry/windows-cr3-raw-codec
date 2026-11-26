#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
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

    std::shared_ptr<const DecodedImageBGR24> thumbnail;
    std::shared_ptr<const DecodedImageBGR24> fullImage;

    ~CR3DecoderState()
    {
        if (stream)
        {
            stream->Release();
            stream = nullptr;
        }
    }
};

// Lightweight IWICBitmapSource backed by a shared 24bppBGR buffer (zero-copy thumbnails).
class CR3BitmapSource final : public IWICBitmapSource
{
public:
    explicit CR3BitmapSource(std::shared_ptr<const DecodedImageBGR24> image);
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
    std::shared_ptr<const DecodedImageBGR24> image_;
};

// IWICBitmapFrameDecode implementation for frame 0 of a Canon CR3 file.
class CR3FrameDecode final : public IWICBitmapFrameDecode
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

private:
    HRESULT getOrDecodeFullImage(std::shared_ptr<const DecodedImageBGR24>& outImage);

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
