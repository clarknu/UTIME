@echo off
setlocal EnableDelayedExpansion

:: 1. Find vcvarsall.bat
set "VCVARS="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
)

if not defined VCVARS (
    echo [ERROR] vcvars64.bat not found.
    exit /b 1
)

echo Using Environment: "!VCVARS!"
call "!VCVARS!"

echo.
echo Compiling DictBuilder...
cl /nologo /EHsc /DUNICODE /D_UNICODE tools\DictBuilder\main.cpp src\sqlite\sqlite3.c /Fo:tools\DictBuilder\ /Fe:tools\DictBuilder\DictBuilder.exe /I src\sqlite /I include\sqlite

if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed.
    exit /b 1
)

echo.
echo Running DictBuilder...
:: Check if cedict_ts.u8 exists
if not exist "src\cedict_ts.u8" (
    echo [ERROR] src\cedict_ts.u8 not found!
    exit /b 1
)

tools\DictBuilder\DictBuilder.exe src\cedict_ts.u8 src\utime.db

if %errorlevel% neq 0 (
    echo [ERROR] DictBuilder failed.
    exit /b 1
)

echo.
echo [SUCCESS] Database generated at src\utime.db
pause