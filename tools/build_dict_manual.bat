@echo off
setlocal EnableDelayedExpansion

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

echo Building DictBuilder...
"%MSBUILD%" tools\DictBuilder\DictBuilder.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
if %errorlevel% neq 0 (
    echo [ERROR] Failed to build DictBuilder.
    exit /b 1
)

echo.
echo Running DictBuilder...
if exist "tools\DictBuilder\x64\Debug\DictBuilder.exe" (
    "tools\DictBuilder\x64\Debug\DictBuilder.exe" src\cedict_ts.u8 src\utime.db
) else if exist "tools\DictBuilder\Debug\DictBuilder.exe" (
    "tools\DictBuilder\Debug\DictBuilder.exe" src\cedict_ts.u8 src\utime.db
) else (
    echo [ERROR] DictBuilder.exe not found.
    exit /b 1
)

if %errorlevel% neq 0 (
    echo [ERROR] DictBuilder failed.
    exit /b 1
)

echo.
echo [SUCCESS] Database generated at src\utime.db
