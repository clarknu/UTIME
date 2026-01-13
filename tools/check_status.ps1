# Check UTIME Status Script
Write-Host "=== UTIME Status Check ===" -ForegroundColor Cyan

# 1. Check DLL
$dllPath = "bin\x64\Debug\UTIME.dll"
if (Test-Path $dllPath) {
    $item = Get-Item $dllPath
    Write-Host "[OK] DLL found at $dllPath" -ForegroundColor Green
    Write-Host "     LastWriteTime: $($item.LastWriteTime)"
} else {
    Write-Host "[ERROR] DLL not found at $dllPath" -ForegroundColor Red
}

# 2. Check Database
$dbPath = "bin\x64\Debug\utime.db"
if (Test-Path $dbPath) {
    $item = Get-Item $dbPath
    $sizeMB = $item.Length / 1MB
    if ($sizeMB -gt 1) {
        Write-Host "[OK] Database found at $dbPath" -ForegroundColor Green
        Write-Host "     Size: $($sizeMB.ToString('F2')) MB"
    } else {
        Write-Host "[WARNING] Database found but seems too small ($($sizeMB.ToString('F2')) MB)" -ForegroundColor Yellow
    }
} else {
    Write-Host "[ERROR] Database not found at $dbPath" -ForegroundColor Red
}

# 3. Check Registry (Requires Admin for full check, but we can try HKCU if applicable, though TSF usually uses HKLM)
Write-Host "`nChecking Registry (HKLM\SOFTWARE\Microsoft\CTF\TIP\{D4696144-8C2D-4581-9653-7313622445E8})..."
try {
    $path = "HKLM:\SOFTWARE\Microsoft\CTF\TIP\{D4696144-8C2D-4581-9653-7313622445E8}"
    if (Test-Path $path) {
        $desc = Get-ItemProperty -Path $path -Name "Description" -ErrorAction SilentlyContinue
        Write-Host "[OK] Registry key exists." -ForegroundColor Green
        if ($desc) {
            Write-Host "     Description: $($desc.Description)"
        }
    } else {
        Write-Host "[WARNING] Registry key not found. You may need to run 'rebuild_all.bat' as Admin." -ForegroundColor Yellow
    }
} catch {
    Write-Host "[INFO] Cannot access Registry (Permission denied?)." -ForegroundColor Gray
}

Write-Host "`n=== End Check ==="
