#pragma once

#include <unknwn.h>
#include <atomic>

// COM IClassFactory implementation for CR3WICDecoder.
class CR3ClassFactory final : public IClassFactory
{
public:
    CR3ClassFactory();
    ~CR3ClassFactory();

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IClassFactory
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override;
    HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock) override;

private:
    std::atomic<ULONG> refCount_{1};
};

// Global DLL reference counter used by DllCanUnloadNow.
void ModuleAddRef();
void ModuleRelease();
LONG GetModuleRefCount();
