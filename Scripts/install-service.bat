@echo off
:: ============================================================
:: Prima Multi Seat - Service Installation Script
:: ============================================================
title Prima Multi Seat - Service Installer

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ERROR] Run as Administrator required.
    pause & exit /b 1
)

set INSTALL_DIR=%~dp0
echo [INFO] Installing service from: %INSTALL_DIR%

:: Install service
"%INSTALL_DIR%PrimaService.exe" install
if %errorLevel% equ 0 (
    echo [OK] Service installed.
    net start PrimaMultiSeatService
    echo [OK] Service started.
) else (
    echo [ERROR] Service installation failed.
)
pause