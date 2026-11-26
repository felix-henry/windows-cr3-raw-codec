#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <initguid.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <shlwapi.h>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <string>
#include <vector>
#include "../src/Guid.h"

namespace
{
template <typename T>
class ComPtr
{
public:
    ComPtr() = default;
    ~ComPtr() { Reset(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    T** Put()
    {
        Reset();
        return &ptr_;
    }

    T* Get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }

    void Reset()
    {
        if (ptr_)
        {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }

private:
    T* ptr_ = nullptr;
};

HRESULT SaveSourceAsImage(IWICImagingFactory* factory, IWICBitmapSource* source, const wchar_t* outPath)
{
    ComPtr<IWICStream> stream;
    HRESULT hr = factory->CreateStream(stream.Put());
    if (FAILED(hr))
        return hr;

    hr = stream->InitializeFromFilename(outPath, GENERIC_WRITE);
    if (FAILED(hr))
        return hr;

    const std::wstring ext = std::filesystem::path(outPath).extension().wstring();
    const bool isJpeg = (_wcsicmp(ext.c_str(), L".jpg") == 0 || _wcsicmp(ext.c_str(), L".jpeg") == 0);
    const GUID containerFmt = isJpeg ? GUID_ContainerFormatJpeg : GUID_ContainerFormatPng;

    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(containerFmt, nullptr, encoder.Put());
    if (FAILED(hr))
        return hr;

    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameEncode> frameEncode;
    ComPtr<IPropertyBag2> props;
    hr = encoder->CreateNewFrame(frameEncode.Put(), isJpeg ? props.Put() : nullptr);
    if (FAILED(hr))
        return hr;

    hr = frameEncode->Initialize(isJpeg ? props.Get() : nullptr);
    if (FAILED(hr))
        return hr;

    UINT width = 0, height = 0;
    hr = source->GetSize(&width, &height);
    if (FAILED(hr))
        return hr;

    hr = frameEncode->SetSize(width, height);
    if (FAILED(hr))
        return hr;

    WICPixelFormatGUID srcFmt{};
    source->GetPixelFormat(&srcFmt);
    WICPixelFormatGUID format = (!isJpeg && srcFmt == GUID_WICPixelFormat48bppBGR)
                                    ? GUID_WICPixelFormat48bppBGR
                                    : GUID_WICPixelFormat24bppBGR;
    hr = frameEncode->SetPixelFormat(&format);
    if (FAILED(hr))
        return hr;

    hr = frameEncode->WriteSource(source, nullptr);
    if (FAILED(hr))
        return hr;

    hr = frameEncode->Commit();
    if (FAILED(hr))
        return hr;

    return encoder->Commit();
}

void PrintMetadataSummary(IWICBitmapFrameDecode* frame)
{
    ComPtr<IWICMetadataBlockReader> blockReader;
    if (SUCCEEDED(frame->QueryInterface(IID_PPV_ARGS(blockReader.Put()))) && blockReader)
    {
        UINT blockCount = 0;
        blockReader->GetCount(&blockCount);
        std::wprintf(L"Metadata Blocks      : %u\n", blockCount);
    }

    ComPtr<IWICMetadataQueryReader> qr;
    HRESULT hr = frame->GetMetadataQueryReader(qr.Put());
    if (FAILED(hr) || !qr)
    {
        std::fwprintf(stderr, L"GetMetadataQueryReader failed: 0x%08X\n", hr);
        return;
    }

    auto readStr = [&](const wchar_t* query, const wchar_t* label) {
        PROPVARIANT pv;
        PropVariantInit(&pv);
        if (SUCCEEDED(qr->GetMetadataByName(query, &pv)))
        {
            if (pv.vt == VT_LPSTR && pv.pszVal)
                std::printf("%-21ls: %s\n", label, pv.pszVal);
            else if (pv.vt == VT_LPWSTR && pv.pwszVal)
                std::wprintf(L"%-21ls: %s\n", label, pv.pwszVal);
        }
        PropVariantClear(&pv);
    };

    readStr(L"/ifd/{ushort=271}", L"Camera Make");
    readStr(L"/ifd/{ushort=272}", L"Camera Model");
    readStr(L"/ifd/exif/{ushort=42036}", L"Lens Model");
    readStr(L"/ifd/exif/{ushort=36867}", L"DateTimeOriginal");

    PROPVARIANT pv;
    PropVariantInit(&pv);
    if (SUCCEEDED(qr->GetMetadataByName(L"/ifd/exif/{ushort=34855}", &pv)) && pv.vt == VT_UI2)
        std::wprintf(L"%-21ls: %u\n", L"ISO", pv.uiVal);
    PropVariantClear(&pv);

    if (SUCCEEDED(qr->GetMetadataByName(L"/ifd/exif/{ushort=33437}", &pv)) && pv.vt == VT_UI8 && pv.uhVal.HighPart > 0)
        std::wprintf(L"%-21ls: f/%.1f\n", L"Aperture", static_cast<double>(pv.uhVal.LowPart) / pv.uhVal.HighPart);
    PropVariantClear(&pv);

    if (SUCCEEDED(qr->GetMetadataByName(L"/ifd/exif/{ushort=33434}", &pv)) && pv.vt == VT_UI8 && pv.uhVal.HighPart > 0)
    {
        const double sec = static_cast<double>(pv.uhVal.LowPart) / pv.uhVal.HighPart;
        if (sec > 0.0 && sec < 0.5)
            std::wprintf(L"%-21ls: 1/%.0f s\n", L"Shutter Speed", 1.0 / sec);
        else
            std::wprintf(L"%-21ls: %.2f s\n", L"Shutter Speed", sec);
    }
    PropVariantClear(&pv);

    if (SUCCEEDED(qr->GetMetadataByName(L"/ifd/exif/{ushort=37386}", &pv)) && pv.vt == VT_UI8 && pv.uhVal.HighPart > 0)
        std::wprintf(L"%-21ls: %.1f mm\n", L"Focal Length", static_cast<double>(pv.uhVal.LowPart) / pv.uhVal.HighPart);
    PropVariantClear(&pv);

    if (SUCCEEDED(qr->GetMetadataByName(L"/ifd/exif/{ushort=37380}", &pv)) && pv.vt == VT_I8 && pv.hVal.HighPart != 0)
    {
        const double ev = static_cast<double>(static_cast<int32_t>(pv.hVal.LowPart)) / pv.hVal.HighPart;
        std::wprintf(L"%-21ls: %+.2f EV\n", L"Exposure Bias", ev);
    }
    PropVariantClear(&pv);

    if (SUCCEEDED(qr->GetMetadataByName(L"/ifd/exif/{ushort=37385}", &pv)) && pv.vt == VT_UI2)
        std::wprintf(L"%-21ls: 0x%X\n", L"Flash", pv.uiVal);
    PropVariantClear(&pv);

    if (SUCCEEDED(qr->GetMetadataByName(L"/ifd/exif/{ushort=41987}", &pv)) && pv.vt == VT_UI2)
        std::wprintf(L"%-21ls: %s\n", L"White Balance", pv.uiVal == 0 ? L"Auto" : L"Manual");
    PropVariantClear(&pv);

    readStr(L"/ifd/exif/{ushort=37521}", L"SubSecTimeOriginal");
    readStr(L"/ifd/{ushort=33432}", L"Copyright");
    if (SUCCEEDED(qr->GetMetadataByName(L"/ifd/{ushort=18246}", &pv)) && pv.vt == VT_UI2)
        std::wprintf(L"%-21ls: %u/5\n", L"Rating", pv.uiVal);
    PropVariantClear(&pv);

    readStr(L"/ifd/gps/{ushort=1}", L"GPS Lat Ref");
    readStr(L"/ifd/gps/{ushort=3}", L"GPS Lon Ref");
}

void PrintColorStats(IWICBitmapSource* source)
{
    UINT width = 0, height = 0;
    if (FAILED(source->GetSize(&width, &height)) || width == 0 || height == 0)
        return;

    WICPixelFormatGUID fmt{};
    if (FAILED(source->GetPixelFormat(&fmt)) || fmt != GUID_WICPixelFormat24bppBGR)
        return;

    const UINT stride = width * 3u;
    std::vector<uint8_t> row(stride);

    uint64_t sumR = 0, sumG = 0, sumB = 0;
    uint64_t clippedR = 0, clippedG = 0, clippedB = 0;
    const uint64_t totalPixels = static_cast<uint64_t>(width) * height;

    WICRect rc{0, 0, static_cast<INT>(width), 1};
    for (UINT y = 0; y < height; ++y)
    {
        rc.Y = static_cast<INT>(y);
        if (FAILED(source->CopyPixels(&rc, stride, stride, row.data())))
            return;

        for (UINT x = 0; x < width; ++x)
        {
            const uint8_t b = row[x * 3 + 0];
            const uint8_t g = row[x * 3 + 1];
            const uint8_t r = row[x * 3 + 2];
            sumB += b;
            sumG += g;
            sumR += r;
            if (r == 255) ++clippedR;
            if (g == 255) ++clippedG;
            if (b == 255) ++clippedB;
        }
    }

    std::printf("Color Stats (mean R/G/B): %.1f / %.1f / %.1f\n",
        static_cast<double>(sumR) / totalPixels,
        static_cast<double>(sumG) / totalPixels,
        static_cast<double>(sumB) / totalPixels);
    std::printf("Clipped 255 pixels (R/G/B): %.3f%% / %.3f%% / %.3f%%\n",
        100.0 * clippedR / totalPixels,
        100.0 * clippedG / totalPixels,
        100.0 * clippedB / totalPixels);
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2)
    {
        std::fwprintf(stderr, L"Usage: test_decoder.exe <input.cr3> [output.png|.jpg] [--thumb] [--meta] [--preview] [--transform] [--dll <CR3WICCodec.dll>]\n");
        return 1;
    }

    const std::wstring inputPath = argv[1];
    std::wstring outputPath;
    std::wstring directDllPath;
    bool testThumb = false;
    bool testMeta = false;
    bool testPreview = false;
    bool testTransform = false;

    for (int i = 2; i < argc; ++i)
    {
        if (std::wcscmp(argv[i], L"--thumb") == 0)
            testThumb = true;
        else if (std::wcscmp(argv[i], L"--meta") == 0)
            testMeta = true;
        else if (std::wcscmp(argv[i], L"--preview") == 0)
            testPreview = true;
        else if (std::wcscmp(argv[i], L"--transform") == 0)
            testTransform = true;
        else if (std::wcscmp(argv[i], L"--dll") == 0 && i + 1 < argc)
            directDllPath = argv[++i];
        else if (outputPath.empty())
            outputPath = argv[i];
    }

    if (outputPath.empty())
    {
        std::filesystem::path p(inputPath);
        p.replace_extension(L".png");
        outputPath = p.wstring();
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        std::fwprintf(stderr, L"CoInitializeEx failed: 0x%08X\n", hr);
        return 1;
    }

    int exitCode = 0;
    HMODULE loadedCodecDll = nullptr;
    {
        ComPtr<IWICImagingFactory> factory;
        hr = CoCreateInstance(
            CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.Put()));
        if (FAILED(hr))
        {
            std::fwprintf(stderr, L"Failed to create IWICImagingFactory: 0x%08X\n", hr);
            exitCode = 1;
        }
        else
        {
            ComPtr<IWICBitmapDecoder> decoder;
            if (!directDllPath.empty())
            {
                loadedCodecDll = LoadLibraryW(directDllPath.c_str());
                if (!loadedCodecDll)
                {
                    std::fwprintf(stderr, L"LoadLibraryW failed for %s (err=%lu)\n", directDllPath.c_str(), GetLastError());
                    exitCode = 1;
                }
                else
                {
                    using DllGetClassObjectFn = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);
                    auto fn = reinterpret_cast<DllGetClassObjectFn>(GetProcAddress(loadedCodecDll, "DllGetClassObject"));
                    ComPtr<IClassFactory> cf;
                    ComPtr<IWICStream> inStream;
                    if (!fn || FAILED(hr = fn(CLSID_CR3WICDecoder, IID_PPV_ARGS(cf.Put()))) ||
                        FAILED(hr = cf->CreateInstance(nullptr, IID_PPV_ARGS(decoder.Put()))) ||
                        FAILED(hr = factory->CreateStream(inStream.Put())) ||
                        FAILED(hr = inStream->InitializeFromFilename(inputPath.c_str(), GENERIC_READ)))
                    {
                        std::fwprintf(stderr, L"Direct DLL setup failed: 0x%08X\n", hr);
                        exitCode = 1;
                    }
                    else
                    {
                        DWORD caps = 0;
                        hr = decoder->QueryCapability(inStream.Get(), &caps);
                        std::wprintf(L"Direct DLL QueryCapability: hr=0x%08X caps=0x%X\n", hr, caps);
                        if (SUCCEEDED(hr))
                            hr = decoder->Initialize(inStream.Get(), WICDecodeMetadataCacheOnDemand);
                        if (FAILED(hr))
                        {
                            std::fwprintf(stderr, L"Direct DLL Initialize failed: 0x%08X\n", hr);
                            exitCode = 1;
                        }
                    }
                }
            }
            else
            {
                hr = factory->CreateDecoderFromFilename(
                    inputPath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.Put());
                if (FAILED(hr))
                {
                    std::fwprintf(stderr, L"CreateDecoderFromFilename failed: 0x%08X\n", hr);
                    std::fwprintf(stderr, L"Ensure CR3WICCodec.dll is registered via install.ps1 / regsvr32.\n");
                    exitCode = 1;
                }
            }

            if (exitCode == 0 && decoder)
            {
                ComPtr<IWICBitmapDecoderInfo> decoderInfo;
                if (directDllPath.empty() && SUCCEEDED(decoder->GetDecoderInfo(decoderInfo.Put())))
                {
                    CLSID clsid{};
                    decoderInfo->GetCLSID(&clsid);
                    wchar_t clsidBuf[64]{};
                    StringFromGUID2(clsid, clsidBuf, _countof(clsidBuf));

                    UINT len = 0;
                    decoderInfo->GetFriendlyName(0, nullptr, &len);
                    std::wstring friendly(len, L'\0');
                    if (len > 0)
                        decoderInfo->GetFriendlyName(len, friendly.data(), &len);

                    std::wprintf(L"WIC Decoder selected : %s\n", friendly.c_str());
                    std::wprintf(L"WIC Decoder CLSID    : %s (%s)\n",
                        clsidBuf,
                        (clsid == CLSID_CR3WICDecoder) ? L"CR3WICCodec MATCH" : L"WARNING: different decoder!");
                }

                ComPtr<IWICBitmapFrameDecode> frame;
                hr = decoder->GetFrame(0, frame.Put());
                if (FAILED(hr))
                {
                    std::fwprintf(stderr, L"GetFrame(0) failed: 0x%08X\n", hr);
                    exitCode = 1;
                }
                else
                {
                    UINT w = 0, h = 0;
                    frame->GetSize(&w, &h);
                    std::wprintf(L"Full RAW Dimensions  : %u x %u\n", w, h);

                    if (testMeta)
                        PrintMetadataSummary(frame.Get());

                    if (testPreview)
                    {
                        ComPtr<IWICBitmapSource> preview;
                        hr = decoder->GetPreview(preview.Put());
                        if (SUCCEEDED(hr) && preview)
                        {
                            UINT pw = 0, ph = 0;
                            preview->GetSize(&pw, &ph);
                            std::wprintf(L"Preview Dimensions   : %u x %u\n", pw, ph);
                        }
                        else
                        {
                            std::fwprintf(stderr, L"GetPreview failed: 0x%08X\n", hr);
                            exitCode = 1;
                        }
                    }

                    if (testTransform)
                    {
                        ComPtr<IWICBitmapSourceTransform> transform;
                        hr = frame->QueryInterface(IID_PPV_ARGS(transform.Put()));
                        BOOL supported = FALSE;
                        UINT txW = 1200, txH = 801;
                        WICPixelFormatGUID txFmt = GUID_WICPixelFormat24bppBGR;
                        if (SUCCEEDED(hr) && transform &&
                            SUCCEEDED(transform->DoesSupportTransform(WICBitmapTransformRotate0, &supported)) && supported &&
                            SUCCEEDED(transform->GetClosestSize(&txW, &txH)) &&
                            SUCCEEDED(transform->GetClosestPixelFormat(&txFmt)))
                        {
                            const UINT reqW = 1200;
                            const UINT reqH = (w > 0) ? (1200u * h / w) : 801;
                            const UINT txStride = reqW * 3u;
                            std::vector<uint8_t> txBuf(static_cast<size_t>(txStride) * reqH);
                            hr = transform->CopyPixels(
                                nullptr, reqW, reqH, &txFmt, WICBitmapTransformRotate0,
                                txStride, static_cast<UINT>(txBuf.size()), txBuf.data());
                            if (SUCCEEDED(hr))
                                std::wprintf(L"Transform Scaled     : %u x %u (closest native %u x %u)\n", reqW, reqH, txW, txH);
                            else
                            {
                                std::fwprintf(stderr, L"Transform CopyPixels failed: 0x%08X\n", hr);
                                exitCode = 1;
                            }

                            BOOL supRot = FALSE;
                            const UINT rotStride = reqH * 3u;
                            std::vector<uint8_t> rotBuf(static_cast<size_t>(rotStride) * reqW);
                            if (SUCCEEDED(transform->DoesSupportTransform(WICBitmapTransformRotate90, &supRot)) && supRot &&
                                SUCCEEDED(transform->CopyPixels(
                                    nullptr, reqW, reqH, &txFmt, WICBitmapTransformRotate90,
                                    rotStride, static_cast<UINT>(rotBuf.size()), rotBuf.data())))
                            {
                                std::wprintf(L"Transform Rotate90   : %u x %u (OK)\n", reqH, reqW);
                            }
                            else
                            {
                                std::fwprintf(stderr, L"Transform Rotate90 failed\n");
                                exitCode = 1;
                            }
                        }
                        else
                        {
                            std::fwprintf(stderr, L"IWICBitmapSourceTransform check failed: 0x%08X\n", hr);
                            exitCode = 1;
                        }
                    }

                    if (testThumb)
                    {
                        ComPtr<IWICBitmapSource> thumb;
                        hr = frame->GetThumbnail(thumb.Put());
                        if (SUCCEEDED(hr) && thumb)
                        {
                            UINT tw = 0, th = 0;
                            thumb->GetSize(&tw, &th);
                            std::wprintf(L"Half-Size RAW Thumb  : %u x %u\n", tw, th);

                            std::filesystem::path tp(outputPath);
                            std::wstring thumbOut = (tp.parent_path() / (tp.stem().wstring() + L"_thumb.png")).wstring();
                            hr = SaveSourceAsImage(factory.Get(), thumb.Get(), thumbOut.c_str());
                            if (SUCCEEDED(hr))
                                std::wprintf(L"Saved thumbnail PNG  : %s\n", thumbOut.c_str());
                            else
                                std::fwprintf(stderr, L"Failed saving thumbnail PNG: 0x%08X\n", hr);
                        }
                        else
                        {
                            std::fwprintf(stderr, L"GetThumbnail failed: 0x%08X\n", hr);
                        }
                    }

                    const ULONGLONG t0 = GetTickCount64();
                    hr = SaveSourceAsImage(factory.Get(), frame.Get(), outputPath.c_str());
                    const ULONGLONG elapsedMs = GetTickCount64() - t0;
                    if (FAILED(hr))
                    {
                        std::fwprintf(stderr, L"SaveSourceAsImage failed: 0x%08X\n", hr);
                        exitCode = 1;
                    }
                    else
                    {
                        std::wprintf(L"Decoded & saved image: %s (%llu ms)\n", outputPath.c_str(), elapsedMs);
                        PrintColorStats(frame.Get());
                    }
                }
            }
        }
    }

    if (loadedCodecDll)
        FreeLibrary(loadedCodecDll);

    CoUninitialize();
    return exitCode;
}
