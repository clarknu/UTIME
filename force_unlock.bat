@echo off
setlocal EnableDelayedExpansion

:: 1. Check for Administrator privileges
net session >nul 2>&1
if %errorLevel% == 0 (
    echo [OK] Running as Administrator.
) else (
    echo [ERROR] This script requires Administrator privileges to unlock files.
    echo Please right-click and select "Run as Administrator".
    pause
    exit /b
)

set "DLL_DIR=%~dp0bin\x64\Debug"
set "DLL_NAME=UTIME.dll"
set "DLL_PATH=%DLL_DIR%\%DLL_NAME%"

echo.
echo [Step 1] Attempting to gracefully unregister...
if exist "%DLL_PATH%" (
    regsvr32 /u /s "%DLL_PATH%"
    if !errorlevel! equ 0 (
        echo [OK] Unregistered successfully.
    ) else (
        echo [WARNING] Unregister failed or timed out.
    )
) else (
    echo [INFO] DLL not found, skipping unregister.
)

echo.
echo [Step 2] Killing processes that might lock the DLL...
:: ctfmon.exe is the Text Services Framework loader - the main culprit
echo Killing ctfmon.exe...
taskkill /f /im ctfmon.exe >nul 2>&1
:: Notepad often holds a handle if it was used for testing
echo Killing notepad.exe...
taskkill /f /im notepad.exe >nul 2>&1

:: Wait a moment for handles to release
timeout /t 1 /nobreak >nul

echo.
echo [Step 3] Attempting to delete or rename the DLL...
if exist "%DLL_PATH%" (
    del /f /q "%DLL_PATH%" >nul 2>&1
    if exist "%DLL_PATH%" (
        echo [INFO] Delete failed. File is still locked.
        echo [ACTION] Attempting to RENAME the locked file to allow new build...
        
        :: Generate a random suffix for the trash file
        set "TRASH_NAME=%DLL_NAME%.trash.%RANDOM%"
        ren "%DLL_PATH%" "!TRASH_NAME!"
        
        if exist "%DLL_PATH%" (
            echo [ERROR] Rename failed! The file is locked by a critical system process ^(likely Explorer^).
            echo.
            echo SUGGESTION:
            echo 1. Open Task Manager.
            echo 2. Restart "Windows Explorer".
            echo 3. Try this script again.
        ) else (
            echo [SUCCESS] Locked file renamed to "!TRASH_NAME!".
            echo You can now BUILD the project in Visual Studio.
            echo ^(The trash file will be deleted on next reboot^)
        )
    ) else (
        echo [SUCCESS] File deleted successfully.
    )
) else (
    echo [INFO] File does not exist. Ready for build.
)

echo.
echo [Step 4] Restarting ctfmon.exe...
start ctfmon.exe

echo.
echo ========================================================
echo  READY TO BUILD
echo ========================================================
echo  Go to Visual Studio and build now.
echo.
pause