#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <initguid.h>
#include <wincodec.h>
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

HRESULT SaveSourceAsPng(IWICImagingFactory* factory, IWICBitmapSource* source, const wchar_t* outPath)
{
    ComPtr<IWICStream> stream;
    HRESULT hr = factory->CreateStream(stream.Put());
    if (FAILED(hr))
        return hr;

    hr = stream->InitializeFromFilename(outPath, GENERIC_WRITE);
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.Put());
    if (FAILED(hr))
        return hr;

    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameEncode> frameEncode;
    hr = encoder->CreateNewFrame(frameEncode.Put(), nullptr);
    if (FAILED(hr))
        return hr;

    hr = frameEncode->Initialize(nullptr);
    if (FAILED(hr))
        return hr;

    UINT width = 0, height = 0;
    hr = source->GetSize(&width, &height);
    if (FAILED(hr))
        return hr;

    hr = frameEncode->SetSize(width, height);
    if (FAILED(hr))
        return hr;

    WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
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
        std::fwprintf(stderr, L"Usage: test_decoder.exe <input.cr3> [output.png] [--thumb] [--dll <CR3WICCodec.dll>]\n");
        return 1;
    }

    const std::wstring inputPath = argv[1];
    std::wstring outputPath;
    std::wstring directDllPath;
    bool testThumb = false;

    for (int i = 2; i < argc; ++i)
    {
        if (std::wcscmp(argv[i], L"--thumb") == 0)
            testThumb = true;
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
                            hr = SaveSourceAsPng(factory.Get(), thumb.Get(), thumbOut.c_str());
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
                    hr = SaveSourceAsPng(factory.Get(), frame.Get(), outputPath.c_str());
                    const ULONGLONG elapsedMs = GetTickCount64() - t0;
                    if (FAILED(hr))
                    {
                        std::fwprintf(stderr, L"SaveSourceAsPng failed: 0x%08X\n", hr);
                        exitCode = 1;
                    }
                    else
                    {
                        std::wprintf(L"Decoded & saved PNG  : %s (%llu ms)\n", outputPath.c_str(), elapsedMs);
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
