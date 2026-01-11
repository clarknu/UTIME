@echo off
set "DLL_PATH=%~dp0bin\x64\Debug\UTIME.dll"

echo [1/4] Closing test applications (Notepad)...
taskkill /f /im notepad.exe 2>nul

echo.
echo [2/4] Attempting to unregister (to release lock if possible)...
:: Sometimes helps, sometimes fails if truly locked by system
regsvr32 /u /s "%DLL_PATH%"

echo.
echo ========================================================
echo  NOW: Go to Visual Studio and click "Build" (F7).
echo  If Build fails with "File in use", switch your system 
echo  IME to "English" or "Microsoft Pinyin" temporarily.
echo ========================================================
pause

echo.
echo [3/4] Registering new DLL...
if not exist "%DLL_PATH%" (
    echo [ERROR] DLL not found at: %DLL_PATH%
    echo Did the build succeed?
    pause
    exit /b
)
regsvr32 /s "%DLL_PATH%"

echo.
echo [4/4] Launching Notepad...
start notepad.exe

echo.
echo Done! Switch input method (Win+Space) to test.
