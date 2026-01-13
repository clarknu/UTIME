@echo off
setlocal EnableDelayedExpansion

:: ========================================================
:: 1. Check for Administrator privileges
:: ========================================================
net session >nul 2>&1
if %errorLevel% == 0 (
    echo [OK] Running as Administrator.
) else (
    echo [ERROR] This script requires Administrator privileges.
    echo Please right-click and select "Run as Administrator".
    pause
    exit /b
)

set "DLL_DIR=%~dp0bin\x64\Debug"
set "DLL_NAME=UTIME.dll"
set "DLL_PATH=%DLL_DIR%\%DLL_NAME%"

:: ========================================================
:: 2. Force Unlock & Clean (Optimized Order)
:: ========================================================
echo.
echo [Step 1/4] Cleaning up previous instance...

:: Step 2.1: First unregister to release TSF hooks
echo   - Unregistering DLL...
if exist "%DLL_PATH%" (
    regsvr32 /u /s "%DLL_PATH%" >nul 2>&1
)

:: Step 2.2: Stop ctfmon service to prevent auto-restart
echo   - Stopping Text Services...
sc stop "TabletInputService" >nul 2>&1

:: Step 2.3: Kill processes AFTER unregister
echo   - Killing processes...
taskkill /f /im ctfmon.exe >nul 2>&1
taskkill /f /im notepad.exe >nul 2>&1

:: Step 2.4: Wait longer for handles to be released
echo   - Waiting for handles to release...
timeout /t 3 /nobreak >nul

:: Step 2.5: Force delete or rename
if exist "%DLL_PATH%" (
    del /f /q "%DLL_PATH%" >nul 2>&1
    if exist "%DLL_PATH%" (
        echo   - Delete failed, attempting rename...
        ren "%DLL_PATH%" "%DLL_NAME%.trash.%RANDOM%" >nul 2>&1
        if exist "%DLL_PATH%" (
            echo [WARNING] File still locked. Trying extended cleanup...
            :: Extra attempt: restart explorer (last resort)
            taskkill /f /im explorer.exe >nul 2>&1
            timeout /t 2 /nobreak >nul
            del /f /q "%DLL_PATH%" >nul 2>&1
            start explorer.exe
            if exist "%DLL_PATH%" (
                ren "%DLL_PATH%" "%DLL_NAME%.trash.%RANDOM%" >nul 2>&1
            )
            if exist "%DLL_PATH%" (
                echo [ERROR] Failed to remove locked file.
                echo Please manually restart your computer.
                sc start "TabletInputService" >nul 2>&1
                pause
                exit /b
            )
        )
    )
)

echo [OK] Environment cleaned.

:: ========================================================
:: 3. Build Project (using MSBuild)
:: ========================================================
echo.
echo [Step 2/4] Building project...

:: Find MSBuild
set "MSBUILD_EXE="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD_EXE=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD_EXE=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD_EXE=C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
)

:: If not found standard paths, try vswhere
if not defined MSBUILD_EXE (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
        set "MSBUILD_EXE=%%i"
    )
)

if not defined MSBUILD_EXE (
    echo [ERROR] MSBuild.exe not found. Please build in Visual Studio manually.
    pause
    exit /b
)

echo Using MSBuild: "!MSBUILD_EXE!"
"!MSBUILD_EXE!" "%~dp0UTIME.sln" /p:Configuration=Debug /p:Platform=x64 /v:minimal

if %errorlevel% neq 0 (
    echo [ERROR] Build failed!
    pause
    exit /b
)
echo [OK] Build successful.

:: Copy generated DB to output
copy /y "%~dp0src\utime.db" "%~dp0bin\x64\Debug\utime.db" >nul
echo [OK] Database updated in bin directory.

:: Force update User DB in AppData
if exist "%APPDATA%\UTIME\utime.db" (
    del /f /q "%APPDATA%\UTIME\utime.db" >nul
    echo [INFO] Deleted old database in AppData.
)

:: ========================================================
:: 4. Register DLL
:: ========================================================
echo.
echo [Step 3/4] Registering DLL...

:: Restart Text Services before registering
echo   - Restarting Text Services...
sc start "TabletInputService" >nul 2>&1
start ctfmon.exe
timeout /t 2 /nobreak >nul

if not exist "%DLL_PATH%" (
    echo [ERROR] DLL not found at: %DLL_PATH%
    pause
    exit /b
)

regsvr32 "%DLL_PATH%"

if %errorlevel% neq 0 (
    echo [ERROR] Registration failed!
    pause
    exit /b
)
echo [OK] Registration successful.

:: ========================================================
:: 5. Launch Test
:: ========================================================
echo.
echo [Step 4/4] Launching test (Notepad)...
start notepad.exe

echo.
echo ========================================================
echo  DONE! 
echo  1. In Notepad, press Win+Space to switch to "UTIME".
echo  2. If not in list, go to Settings ^> Time ^& Language ^> Language
echo     ^> Chinese ^> Options ^> Add a keyboard.
echo ========================================================
pause