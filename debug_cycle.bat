@echo off
:: Check for Administrator privileges
net session >nul 2>&1
if %errorLevel% == 0 (
    echo [OK] Running as Administrator.
) else (
    echo [ERROR] Current permissions are inadequate.
    echo Please right-click this script and select "Run as Administrator".
    pause
    exit /b
)

set "DLL_PATH=%~dp0bin\x64\Debug\UTIME.dll"

echo.
echo [1/4] Closing test applications (Notepad)...
taskkill /f /im notepad.exe 2>nul

echo.
echo [2/4] Attempting to unregister (to release lock if possible)...
regsvr32 /u /s "%DLL_PATH%"
if %errorlevel% neq 0 echo [WARNING] Unregister failed (maybe not registered yet).

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

:: Run regsvr32 without /s to show errors if any
echo Registering %DLL_PATH%...
regsvr32 "%DLL_PATH%"

if %errorlevel% neq 0 (
    echo [ERROR] Registration failed!
    pause
    exit /b
) else (
    echo [SUCCESS] Registration succeeded.
)

echo.
echo [4/4] Launching Notepad...
start notepad.exe

echo.
echo Done! Switch input method (Win+Space) to test.
pause
