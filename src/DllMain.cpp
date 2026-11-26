#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <initguid.h>
#include <atomic>
#include <mutex>
#include <new>
#include <string>
#include "Guid.h"
#include "ClassFactory.h"
#include "Registry.h"

namespace
{
HMODULE g_moduleHandle = nullptr;
HMODULE g_vcompHandle = nullptr;
HMODULE g_librawHandle = nullptr;
std::once_flag g_librawLoadOnce;
std::atomic<LONG> g_moduleRefCount{0};
} // namespace

bool EnsureLibRawLoaded()
{
    std::call_once(g_librawLoadOnce, []() {
        if (!g_moduleHandle)
            return;

        std::wstring path(MAX_PATH, L'\0');
        DWORD len = GetModuleFileNameW(g_moduleHandle, path.data(), static_cast<DWORD>(path.size()));
        while (len >= path.size() - 1)
        {
            path.resize(path.size() * 2);
            len = GetModuleFileNameW(g_moduleHandle, path.data(), static_cast<DWORD>(path.size()));
        }
        if (len == 0)
            return;

        path.resize(len);
        const size_t slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
            return;

        const std::wstring dir = path.substr(0, slash + 1);

        // Preload sibling OpenMP runtime (if bundled) before loading libraw.dll
        const std::wstring vcompPath = dir + L"vcomp140.dll";
        g_vcompHandle = LoadLibraryExW(vcompPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);

        const std::wstring librawPath = dir + L"libraw.dll";
        g_librawHandle = LoadLibraryExW(librawPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!g_librawHandle)
            g_librawHandle = LoadLibraryW(L"libraw.dll");
    });
    return g_librawHandle != nullptr;
}

void ModuleAddRef()
{
    ++g_moduleRefCount;
}

void ModuleRelease()
{
    --g_moduleRefCount;
}

LONG GetModuleRefCount()
{
    return g_moduleRefCount.load();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        g_moduleHandle = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow()
{
    return GetModuleRefCount() == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    if (!ppv)
        return E_INVALIDARG;

    *ppv = nullptr;
    if (rclsid != CLSID_CR3WICDecoder)
        return CLASS_E_CLASSNOTAVAILABLE;

    CR3ClassFactory* factory = new (std::nothrow) CR3ClassFactory();
    if (!factory)
        return E_OUTOFMEMORY;

    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllRegisterServer()
{
    return RegisterCR3Codec(g_moduleHandle);
}

STDAPI DllUnregisterServer()
{
    return UnregisterCR3Codec();
}
