@echo off
:: ============================================================
:: Prima Multi Seat - Service Uninstall Script
:: ============================================================
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ERROR] Run as Administrator required.
    pause & exit /b 1
)

net stop PrimaMultiSeatService >nul 2>&1
"%~dp0PrimaService.exe" uninstall
echo [OK] Service uninstalled.
pause