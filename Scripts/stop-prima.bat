@echo off
:: ============================================================
:: Prima Multi Seat - Stop Script
:: ============================================================
title Prima Multi Seat - Stopping...
echo.
echo  [INFO] Stopping Prima Multi Seat...

:: Stop service
sc query PrimaMultiSeatService >nul 2>&1
if %errorLevel% equ 0 (
    net stop PrimaMultiSeatService >nul 2>&1
    echo  [OK] Service stopped.
)

:: Kill processes
taskkill /IM PrimaMultiSeat.exe /F >nul 2>&1
taskkill /IM PrimaUI.exe /F >nul 2>&1
echo  [OK] Processes terminated.

:: Restore system cursor via registry
reg add "HKCU\Control Panel\Cursors" /v "" /t REG_SZ /d "%SystemRoot%\cursors\aero_arrow.cur" /f >nul 2>&1
rundll32.exe user32.dll, UpdatePerUserSystemParameters >nul 2>&1
echo  [OK] System cursor restored.

echo.
echo  Prima Multi Seat stopped successfully.
echo.
pause