#include "Globals.h"
#include <olectl.h>

const TCHAR c_szDescription[] = TEXT("UTIME Sample IME");
const TCHAR c_szModelName[] = TEXT("UTIME");

// Register the COM server
STDAPI DllRegisterServer()
{
    HKEY hKey;
    TCHAR szFileName[MAX_PATH];
    TCHAR szKey[MAX_PATH];

    if (GetModuleFileName(g_hInst, szFileName, ARRAYSIZE(szFileName)) == 0)
    {
        return E_FAIL;
    }

    // Register CLSID
    wsprintf(szKey, TEXT("CLSID\\{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"), 
        CLSID_UTIME.Data1, CLSID_UTIME.Data2, CLSID_UTIME.Data3, 
        CLSID_UTIME.Data4[0], CLSID_UTIME.Data4[1], CLSID_UTIME.Data4[2], CLSID_UTIME.Data4[3], 
        CLSID_UTIME.Data4[4], CLSID_UTIME.Data4[5], CLSID_UTIME.Data4[6], CLSID_UTIME.Data4[7]);

    if (RegCreateKeyEx(HKEY_CLASSES_ROOT, szKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
    {
        return E_FAIL;
    }
    RegSetValueEx(hKey, NULL, 0, REG_SZ, (BYTE *)c_szDescription, (lstrlen(c_szDescription) + 1) * sizeof(TCHAR));
    RegCloseKey(hKey);

    wsprintf(szKey, TEXT("CLSID\\{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\\InProcServer32"), 
        CLSID_UTIME.Data1, CLSID_UTIME.Data2, CLSID_UTIME.Data3, 
        CLSID_UTIME.Data4[0], CLSID_UTIME.Data4[1], CLSID_UTIME.Data4[2], CLSID_UTIME.Data4[3], 
        CLSID_UTIME.Data4[4], CLSID_UTIME.Data4[5], CLSID_UTIME.Data4[6], CLSID_UTIME.Data4[7]);

    if (RegCreateKeyEx(HKEY_CLASSES_ROOT, szKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
    {
        return E_FAIL;
    }
    RegSetValueEx(hKey, NULL, 0, REG_SZ, (BYTE *)szFileName, (lstrlen(szFileName) + 1) * sizeof(TCHAR));
    RegSetValueEx(hKey, TEXT("ThreadingModel"), 0, REG_SZ, (BYTE *)TEXT("Apartment"), 20);
    RegCloseKey(hKey);

    // Register TSF Profile
    ITfInputProcessorProfiles *pProfiles;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (void**)&pProfiles);
    if (SUCCEEDED(hr))
    {
        hr = pProfiles->Register(CLSID_UTIME);
        if (SUCCEEDED(hr))
        {
            hr = pProfiles->AddLanguageProfile(CLSID_UTIME, 0x0804, GUID_PROFILE, c_szDescription, (ULONG)wcslen(c_szDescription), szFileName, (ULONG)wcslen(szFileName), 0);
            
            // Explicitly enable the profile
            if (SUCCEEDED(hr))
            {
                hr = pProfiles->EnableLanguageProfile(CLSID_UTIME, 0x0804, GUID_PROFILE, TRUE);
            }
        }
        pProfiles->Release();
    }

    if (FAILED(hr))
    {
        return hr;
    }

    // Register TSF Category
    ITfCategoryMgr *pCategoryMgr;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (void**)&pCategoryMgr);
    if (SUCCEEDED(hr))
    {
        hr = pCategoryMgr->RegisterCategory(CLSID_UTIME, GUID_TFCAT_TIP_KEYBOARD, CLSID_UTIME);
        // We are NOT implementing Display Attributes yet, so do not register it.
        // if (SUCCEEDED(hr))
        // {
        //     hr = pCategoryMgr->RegisterCategory(CLSID_UTIME, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_UTIME);
        // }
        pCategoryMgr->Release();
    }
    
    return hr;
}

// Unregister the COM server
STDAPI DllUnregisterServer()
{
    TCHAR szKey[MAX_PATH];
    
    // Unregister TSF Profile
    ITfInputProcessorProfiles *pProfiles;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (void**)&pProfiles);
    if (SUCCEEDED(hr))
    {
        pProfiles->Unregister(CLSID_UTIME);
        pProfiles->Release();
    }

    // Unregister TSF Category
    ITfCategoryMgr *pCategoryMgr;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (void**)&pCategoryMgr);
    if (SUCCEEDED(hr))
    {
        pCategoryMgr->UnregisterCategory(CLSID_UTIME, GUID_TFCAT_TIP_KEYBOARD, CLSID_UTIME);
        // pCategoryMgr->UnregisterCategory(CLSID_UTIME, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_UTIME);
        pCategoryMgr->Release();
    }

    // Delete CLSID keys
    wsprintf(szKey, TEXT("CLSID\\{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\\InProcServer32"), 
        CLSID_UTIME.Data1, CLSID_UTIME.Data2, CLSID_UTIME.Data3, 
        CLSID_UTIME.Data4[0], CLSID_UTIME.Data4[1], CLSID_UTIME.Data4[2], CLSID_UTIME.Data4[3], 
        CLSID_UTIME.Data4[4], CLSID_UTIME.Data4[5], CLSID_UTIME.Data4[6], CLSID_UTIME.Data4[7]);
    RegDeleteKey(HKEY_CLASSES_ROOT, szKey);

    wsprintf(szKey, TEXT("CLSID\\{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"), 
        CLSID_UTIME.Data1, CLSID_UTIME.Data2, CLSID_UTIME.Data3, 
        CLSID_UTIME.Data4[0], CLSID_UTIME.Data4[1], CLSID_UTIME.Data4[2], CLSID_UTIME.Data4[3], 
        CLSID_UTIME.Data4[4], CLSID_UTIME.Data4[5], CLSID_UTIME.Data4[6], CLSID_UTIME.Data4[7]);
    RegDeleteKey(HKEY_CLASSES_ROOT, szKey);

    return S_OK;
}
