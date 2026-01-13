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
echo [Step 1/5] Unregistering DLL first...
if exist "%DLL_PATH%" (
    regsvr32 /u /s "%DLL_PATH%" >nul 2>&1
    if !errorlevel! equ 0 (
        echo   [OK] Unregistered successfully.
    ) else (
        echo   [INFO] Unregister skipped (not registered or failed).
    )
) else (
    echo   [INFO] DLL not found, skipping unregister.
)

echo.
echo [Step 2/5] Stopping Text Services to prevent auto-restart...
sc stop "TabletInputService" >nul 2>&1
if !errorlevel! equ 0 (
    echo   [OK] TabletInputService stopped.
) else (
    echo   [INFO] Service already stopped or not running.
)

echo.
echo [Step 3/5] Killing processes that might lock the DLL...
echo   - Killing ctfmon.exe...
taskkill /f /im ctfmon.exe >nul 2>&1
echo   - Killing notepad.exe...
taskkill /f /im notepad.exe >nul 2>&1

:: Wait longer for handles to release
echo   - Waiting for handles to release (3 seconds)...
timeout /t 3 /nobreak >nul

echo.
echo [Step 4/5] Attempting to delete or rename the DLL...
if exist "%DLL_PATH%" (
    del /f /q "%DLL_PATH%" >nul 2>&1
    if exist "%DLL_PATH%" (
        echo   [INFO] Delete failed. File is still locked.
        echo   [ACTION] Attempting to RENAME the locked file...
        
        :: Generate a random suffix for the trash file
        set "TRASH_NAME=%DLL_NAME%.trash.%RANDOM%"
        ren "%DLL_PATH%" "!TRASH_NAME!" >nul 2>&1
        
        if exist "%DLL_PATH%" (
            echo   [WARNING] Rename failed! Trying extended cleanup...
            echo   - Restarting Windows Explorer...
            taskkill /f /im explorer.exe >nul 2>&1
            timeout /t 2 /nobreak >nul
            del /f /q "%DLL_PATH%" >nul 2>&1
            start explorer.exe
            
            if exist "%DLL_PATH%" (
                ren "%DLL_PATH%" "!TRASH_NAME!" >nul 2>&1
            )
            
            if exist "%DLL_PATH%" (
                echo   [ERROR] All attempts failed! The file is locked by a critical process.
                echo.
                echo   SUGGESTION:
                echo   1. Restart your computer.
                echo   2. Try this script again immediately after reboot.
            ) else (
                echo   [SUCCESS] File removed after Explorer restart.
            )
        ) else (
            echo   [SUCCESS] Locked file renamed to "!TRASH_NAME!".
            echo   (The trash file will be deleted on next reboot)
        )
    ) else (
        echo   [SUCCESS] File deleted successfully.
    )
) else (
    echo   [INFO] File does not exist. Ready for build.
)

echo.
echo [Step 5/5] Restarting Text Services...
sc start "TabletInputService" >nul 2>&1
start ctfmon.exe

echo.
echo ========================================================
echo  READY TO BUILD
echo ========================================================
echo  Go to Visual Studio and build now.
echo.
pause