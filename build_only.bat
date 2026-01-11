@echo off
setlocal EnableDelayedExpansion

echo [1/3] Killing processes...
taskkill /f /im notepad.exe >nul 2>&1
taskkill /f /im ctfmon.exe >nul 2>&1

echo.
echo [2/3] Building Project...
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
echo [3/3] Copying Database...
copy /y src\utime.db bin\x64\Debug\utime.db >nul

echo.
echo [SUCCESS] Build complete.
echo.
echo NOTE: To register the DLL, you MUST run 'debug_cycle.bat' or 'rebuild_all.bat' AS ADMINISTRATOR from File Explorer.
echo Trae cannot perform the registration step due to permission limits.
