#include "Globals.h"
#include "TextService.h"
#include <olectl.h>

HINSTANCE g_hInst = NULL;
LONG g_cRefDll = 0;

void DllAddRef()
{
    InterlockedIncrement(&g_cRefDll);
}

void DllRelease()
{
    InterlockedDecrement(&g_cRefDll);
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID pvReserved)
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        g_hInst = hInstance;
        DisableThreadLibraryCalls(hInstance);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// ClassFactory implementation
class CClassFactory : public IClassFactory
{
public:
    CClassFactory() : _cRef(1) { DllAddRef(); }
    ~CClassFactory() { DllRelease(); }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj)
    {
        if (ppvObj == NULL) return E_INVALIDARG;
        *ppvObj = NULL;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory))
        {
            *ppvObj = (void *)this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&_cRef);
    }

    STDMETHODIMP_(ULONG) Release()
    {
        long cRef = InterlockedDecrement(&_cRef);
        if (cRef == 0) delete this;
        return cRef;
    }

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObj)
    {
        if (ppvObj == NULL) return E_INVALIDARG;
        *ppvObj = NULL;
        if (pUnkOuter != NULL) return CLASS_E_NOAGGREGATION;

        if (IsEqualIID(riid, IID_ITfTextInputProcessor))
        {
            CTextService *pTextService = new CTextService();
            if (!pTextService) return E_OUTOFMEMORY;
            HRESULT hr = pTextService->QueryInterface(riid, ppvObj);
            pTextService->Release();
            return hr;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP LockServer(BOOL fLock)
    {
        if (fLock) DllAddRef();
        else DllRelease();
        return S_OK;
    }

private:
    long _cRef;
};

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    if (ppv == NULL) return E_INVALIDARG;
    *ppv = NULL;

    if (IsEqualCLSID(rclsid, CLSID_UTIME))
    {
        CClassFactory *pFactory = new CClassFactory();
        if (pFactory == NULL) return E_OUTOFMEMORY;
        HRESULT hr = pFactory->QueryInterface(riid, ppv);
        pFactory->Release();
        return hr;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow()
{
    return (g_cRefDll == 0) ? S_OK : S_FALSE;
}
