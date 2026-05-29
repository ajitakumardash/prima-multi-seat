@echo off
:: ============================================================
:: Prima Multi Seat - Start Script
:: Run as Administrator
:: ============================================================
title Prima Multi Seat - Starting...
echo.
echo  ████████████████████████████████████████
echo      PRIMA MULTI SEAT v1.0.0
echo      Dual-Seat School Lab Software
echo  ████████████████████████████████████████
echo.

:: Check for admin rights
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo  [ERROR] Administrator privileges required.
    echo  Please right-click and select "Run as administrator"
    pause
    exit /b 1
)

:: Start Windows Service (if installed)
sc query PrimaMultiSeatService >nul 2>&1
if %errorLevel% equ 0 (
    echo  [INFO] Starting Windows Service...
    net start PrimaMultiSeatService >nul 2>&1
    echo  [OK] Service started.
) else (
    :: Start core engine directly
    echo  [INFO] Starting Prima Core Engine...
    start "" /B "%~dp0PrimaMultiSeat.exe"
    timeout /t 2 /nobreak >nul
)

:: Start dashboard UI
echo  [INFO] Starting Dashboard UI...
start "" "%~dp0PrimaUI.exe"

echo.
echo  [OK] Prima Multi Seat started successfully!
echo  [INFO] Press Ctrl+Alt+P at any time for emergency recovery.
echo.
exit /b 0