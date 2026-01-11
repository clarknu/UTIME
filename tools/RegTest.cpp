#include <windows.h>
#include <iostream>

typedef HRESULT (STDAPICALLTYPE *PfnDllRegisterServer)(void);

int main()
{
    const wchar_t* dllPath = L"D:\\Code\\UTIME\\bin\\x64\\Debug\\UTIME.dll";
    
    std::wcout << L"Loading DLL: " << dllPath << std::endl;
    HMODULE hDll = LoadLibraryW(dllPath);
    if (!hDll)
    {
        std::cerr << "Failed to load DLL. Error: " << GetLastError() << std::endl;
        return 1;
    }

    PfnDllRegisterServer pfnDllRegisterServer = (PfnDllRegisterServer)GetProcAddress(hDll, "DllRegisterServer");
    if (!pfnDllRegisterServer)
    {
        std::cerr << "Failed to find DllRegisterServer. Error: " << GetLastError() << std::endl;
        FreeLibrary(hDll);
        return 1;
    }

    std::cout << "Calling DllRegisterServer..." << std::endl;
    HRESULT hr = pfnDllRegisterServer();
    if (FAILED(hr))
    {
        std::cerr << "DllRegisterServer failed with HRESULT: 0x" << std::hex << hr << std::endl;
    }
    else
    {
        std::cout << "DllRegisterServer succeeded!" << std::endl;
    }

    FreeLibrary(hDll);
    return 0;
}
