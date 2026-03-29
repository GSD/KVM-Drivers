@echo off
REM Driver Uninstallation Script for KVM-Drivers
REM Must be run as Administrator

echo ============================================
echo KVM-Drivers Uninstallation
echo ============================================
echo.

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: This script must be run as Administrator
    pause
    exit /b 1
)

echo Removing drivers...

REM Find and remove published names from pnputil
for /f "tokens=2 delims=: " %%a in ('pnputil /enum-drivers ^| findstr /i "vhidkb.inf vhidmouse.inf vxinput.inf vdisplay.inf"') do (
    echo Removing driver: %%a
    pnputil /delete-driver %%a /uninstall /force
)

echo.
echo Uninstallation complete
echo.
pause
