#include "ClassFactory.h"
#include "CR3Decoder.h"
#include <new>

CR3ClassFactory::CR3ClassFactory()
{
    ModuleAddRef();
}

CR3ClassFactory::~CR3ClassFactory()
{
    ModuleRelease();
}

HRESULT STDMETHODCALLTYPE CR3ClassFactory::QueryInterface(REFIID riid, void** ppvObject)
{
    if (!ppvObject)
        return E_INVALIDARG;

    *ppvObject = nullptr;
    if (riid == IID_IUnknown || riid == IID_IClassFactory)
    {
        *ppvObject = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CR3ClassFactory::AddRef()
{
    return ++refCount_;
}

ULONG STDMETHODCALLTYPE CR3ClassFactory::Release()
{
    const ULONG remaining = --refCount_;
    if (remaining == 0)
        delete this;
    return remaining;
}

HRESULT STDMETHODCALLTYPE CR3ClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject)
{
    if (!ppvObject)
        return E_INVALIDARG;

    *ppvObject = nullptr;
    if (pUnkOuter != nullptr)
        return CLASS_E_NOAGGREGATION;

    CR3Decoder* decoder = new (std::nothrow) CR3Decoder();
    if (!decoder)
        return E_OUTOFMEMORY;

    const HRESULT hr = decoder->QueryInterface(riid, ppvObject);
    decoder->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE CR3ClassFactory::LockServer(BOOL fLock)
{
    if (fLock)
        ModuleAddRef();
    else
        ModuleRelease();
    return S_OK;
}
