@echo off
setlocal EnableDelayedExpansion

set "DLL_DIR=%~dp0bin\x64\Debug"
set "DLL_NAME=UTIME.dll"
set "DLL_PATH=%DLL_DIR%\%DLL_NAME%"

echo [1/4] Preparing environment...

:: Step 1: Unregister DLL first (reduces lock)
echo   - Attempting to unregister DLL...
if exist "%DLL_PATH%" (
    regsvr32 /u /s "%DLL_PATH%" >nul 2>&1
)

echo [2/4] Killing processes...
taskkill /f /im notepad.exe >nul 2>&1
taskkill /f /im ctfmon.exe >nul 2>&1

:: Wait for handles to release
echo   - Waiting for handles to release...
timeout /t 2 /nobreak >nul

:: Try to delete or rename old DLL
if exist "%DLL_PATH%" (
    del /f /q "%DLL_PATH%" >nul 2>&1
    if exist "%DLL_PATH%" (
        echo   - File locked, renaming old DLL...
        ren "%DLL_PATH%" "%DLL_NAME%.trash.%RANDOM%" >nul 2>&1
    )
)

echo.
echo [3/4] Building Project...
set "MSBUILD="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
) else if exist "C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD=C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
)

if not defined MSBUILD (
    echo [ERROR] MSBuild not found.
    exit /b 1
)

"%MSBUILD%" UTIME.sln /p:Configuration=Debug /p:Platform=x64 /v:minimal
if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo.
echo [4/4] Copying Database & Restarting services...
copy /y src\utime.db bin\x64\Debug\utime.db >nul

:: Restart ctfmon
start ctfmon.exe >nul 2>&1

echo.
echo [SUCCESS] Build complete.
echo.
echo NOTE: To register the DLL, you MUST run 'debug_cycle.bat' or 'rebuild_all.bat' AS ADMINISTRATOR from File Explorer.
