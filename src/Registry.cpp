#include "Registry.h"
#include "Guid.h"
#include "Version.h"
#include <shlobj.h>
#include <wincodec.h>
#include <cstring>
#include <string>

namespace
{
HRESULT SetRegString(HKEY root, const wchar_t* subKey, const wchar_t* valueName, const wchar_t* data)
{
    HKEY key = nullptr;
    const LONG res = RegCreateKeyExW(
        root, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, nullptr, &key, nullptr);
    if (res != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(res);

    const DWORD bytes = static_cast<DWORD>((std::wcslen(data) + 1) * sizeof(wchar_t));
    const LONG setRes = RegSetValueExW(
        key, valueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(data), bytes);
    RegCloseKey(key);
    return setRes == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(setRes);
}

HRESULT SetRegDword(HKEY root, const wchar_t* subKey, const wchar_t* valueName, DWORD value)
{
    HKEY key = nullptr;
    const LONG res = RegCreateKeyExW(
        root, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, nullptr, &key, nullptr);
    if (res != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(res);

    const LONG setRes = RegSetValueExW(
        key, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
    return setRes == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(setRes);
}

HRESULT SetRegBinary(HKEY root, const wchar_t* subKey, const wchar_t* valueName, const BYTE* data, DWORD size)
{
    HKEY key = nullptr;
    const LONG res = RegCreateKeyExW(
        root, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, nullptr, &key, nullptr);
    if (res != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(res);

    const LONG setRes = RegSetValueExW(key, valueName, 0, REG_BINARY, data, size);
    RegCloseKey(key);
    return setRes == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(setRes);
}

HRESULT CreateEmptyRegKey(HKEY root, const wchar_t* subKey)
{
    HKEY key = nullptr;
    const LONG res = RegCreateKeyExW(
        root, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, nullptr, &key, nullptr);
    if (res != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(res);
    RegCloseKey(key);
    return S_OK;
}

HRESULT DeleteRegTreeSafe(HKEY root, const wchar_t* subKey)
{
    const LONG res = RegDeleteTreeW(root, subKey);
    if (res == ERROR_SUCCESS || res == ERROR_FILE_NOT_FOUND)
        return S_OK;
    return HRESULT_FROM_WIN32(res);
}

std::wstring GetModulePath(HMODULE moduleHandle)
{
    std::wstring path(MAX_PATH, L'\0');
    while (true)
    {
        const DWORD len = GetModuleFileNameW(moduleHandle, path.data(), static_cast<DWORD>(path.size()));
        if (len == 0)
            return {};
        if (len < path.size() - 1)
        {
            path.resize(len);
            return path;
        }
        path.resize(path.size() * 2);
    }
}
} // namespace

HRESULT RegisterCR3Codec(HMODULE moduleHandle)
{
    const std::wstring dllPath = GetModulePath(moduleHandle);
    if (dllPath.empty())
        return HRESULT_FROM_WIN32(GetLastError());

    const std::wstring clsidBase = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kClsidCR3WICDecoderStr;
    const std::wstring inprocKey = clsidBase + L"\\InprocServer32";
    const std::wstring pattern0Key = clsidBase + L"\\Patterns\\0";
    const std::wstring pattern1Key = clsidBase + L"\\Patterns\\1";

    wchar_t bgr24Str[64]{};
    if (StringFromGUID2(GUID_WICPixelFormat24bppBGR, bgr24Str, _countof(bgr24Str)) == 0)
        return E_FAIL;
    const std::wstring formatKey = clsidBase + L"\\Formats\\" + bgr24Str;

    const std::wstring wicCategoryInstanceKey =
        std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kWicBitmapDecodersCategoryStr +
        L"\\Instance\\" + kClsidCR3WICDecoderStr;

    HRESULT hr = S_OK;

    // 1. Register COM server & WIC decoder metadata under HKLM\SOFTWARE\Classes\CLSID\{...}
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, clsidBase.c_str(), nullptr, L"Canon CR3 WIC RAW Decoder")))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"FriendlyName", L"Canon CR3 WIC RAW Decoder")))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, inprocKey.c_str(), nullptr, dllPath.c_str())))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, inprocKey.c_str(), L"ThreadingModel", L"Both")))
        return hr;

    if (FAILED(hr = SetRegDword(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"ArbitrationPriority", 20)))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"Author", L"CR3 WIC Codec Project")))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"ColorManagementVersion", L"1.0.0.0")))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"Description", L"Native 14-bit Canon CR3 RAW Decoder (LibRaw)")))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"Version", CR3_VER_STR_W)))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"SpecVersion", L"1.0.0.0")))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"Vendor", kVendorCanonCR3CodecStr)))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"ContainerFormat", kContainerFormatCR3Str)))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"FileExtensions", L".cr3")))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"MimeTypes", L"image/x-canon-cr3")))
        return hr;
    if (FAILED(hr = SetRegDword(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"Flags", WICComponentUnsigned)))
        return hr;
    if (FAILED(hr = SetRegDword(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"SupportAnimation", 0)))
        return hr;
    if (FAILED(hr = SetRegDword(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"SupportChromaKey", 0)))
        return hr;
    if (FAILED(hr = SetRegDword(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"SupportLossless", 1)))
        return hr;
    if (FAILED(hr = SetRegDword(HKEY_LOCAL_MACHINE, clsidBase.c_str(), L"SupportMultiframe", 0)))
        return hr;

    // Supported pixel format
    if (FAILED(hr = CreateEmptyRegKey(HKEY_LOCAL_MACHINE, formatKey.c_str())))
        return hr;

    // Pattern 0: Extended ISOBMFF Canon CR3 signature (19 matched bytes -> outscores Microsoft's 8-byte pattern)
    // Bytes 0..2: 00 00 00, Byte 3: box size (masked 00), Bytes 4..11: "ftypcrx ", Bytes 16..23: "crx isom"
    static constexpr BYTE kCr3ExtPattern[24] = {
        0x00, 0x00, 0x00, 0x18, 'f', 't', 'y', 'p', 'c', 'r', 'x', ' ',
        0x00, 0x00, 0x00, 0x01, 'c', 'r', 'x', ' ', 'i', 's', 'o', 'm'
    };
    static constexpr BYTE kCr3ExtMask[24] = {
        0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };
    if (FAILED(hr = SetRegDword(HKEY_LOCAL_MACHINE, pattern0Key.c_str(), L"Position", 0)))
        return hr;
    if (FAILED(hr = SetRegDword(HKEY_LOCAL_MACHINE, pattern0Key.c_str(), L"Length", sizeof(kCr3ExtPattern))))
        return hr;
    if (FAILED(hr = SetRegBinary(HKEY_LOCAL_MACHINE, pattern0Key.c_str(), L"Pattern", kCr3ExtPattern, sizeof(kCr3ExtPattern))))
        return hr;
    if (FAILED(hr = SetRegBinary(HKEY_LOCAL_MACHINE, pattern0Key.c_str(), L"Mask", kCr3ExtMask, sizeof(kCr3ExtMask))))
        return hr;

    // Pattern 1: Fallback ISOBMFF 'ftypcrx ' signature at offset 4 (8 bytes)
    static constexpr BYTE kCr3Pattern[8] = {'f', 't', 'y', 'p', 'c', 'r', 'x', ' '};
    static constexpr BYTE kCr3Mask[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (FAILED(hr = SetRegDword(HKEY_LOCAL_MACHINE, pattern1Key.c_str(), L"Position", 4)))
        return hr;
    if (FAILED(hr = SetRegDword(HKEY_LOCAL_MACHINE, pattern1Key.c_str(), L"Length", sizeof(kCr3Pattern))))
        return hr;
    if (FAILED(hr = SetRegBinary(HKEY_LOCAL_MACHINE, pattern1Key.c_str(), L"Pattern", kCr3Pattern, sizeof(kCr3Pattern))))
        return hr;
    if (FAILED(hr = SetRegBinary(HKEY_LOCAL_MACHINE, pattern1Key.c_str(), L"Mask", kCr3Mask, sizeof(kCr3Mask))))
        return hr;

    // 2. Register in WIC Bitmap Decoders category instance list
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, wicCategoryInstanceKey.c_str(), L"CLSID", kClsidCR3WICDecoderStr)))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, wicCategoryInstanceKey.c_str(), L"FriendlyName", L"Canon CR3 WIC RAW Decoder")))
        return hr;

    // 3. Associate .cr3 with Windows Photo Thumbnail Handler (uses WIC)
    static constexpr wchar_t kPhotoThumbnailProvider[] = L"{C7657C4A-9F68-40fa-A4DF-96BC08EB3551}";
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes\\.cr3", L"PerceivedType", L"image")))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes\\.cr3", L"Content Type", L"image/x-canon-cr3")))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes\\.cr3\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}", nullptr, kPhotoThumbnailProvider)))
        return hr;

    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes\\SystemFileAssociations\\.cr3", L"PerceivedType", L"image")))
        return hr;
    if (FAILED(hr = SetRegString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes\\SystemFileAssociations\\.cr3\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}", nullptr, kPhotoThumbnailProvider)))
        return hr;

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}

HRESULT UnregisterCR3Codec()
{
    const std::wstring clsidBase = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kClsidCR3WICDecoderStr;
    const std::wstring wicCategoryInstanceKey =
        std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + kWicBitmapDecodersCategoryStr +
        L"\\Instance\\" + kClsidCR3WICDecoderStr;

    HRESULT hr1 = DeleteRegTreeSafe(HKEY_LOCAL_MACHINE, wicCategoryInstanceKey.c_str());
    HRESULT hr2 = DeleteRegTreeSafe(HKEY_LOCAL_MACHINE, clsidBase.c_str());

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    if (FAILED(hr1))
        return hr1;
    return hr2;
}
