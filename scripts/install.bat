@echo off
REM Driver Installation Script for KVM-Drivers
REM Must be run as Administrator

echo ============================================
echo KVM-Drivers Installation
echo ============================================
echo.

REM Check for admin privileges
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: This script must be run as Administrator
    echo Right-click and select "Run as administrator"
    pause
    exit /b 1
)

set CONFIG=Release
if "%~1"=="Debug" set CONFIG=Debug

echo Installing %CONFIG% drivers...
echo.

set BUILD_DIR=%~dp0..\build\%CONFIG%

REM Install Virtual HID Keyboard
echo [1/4] Installing Virtual HID Keyboard Driver...
if exist "%BUILD_DIR%\vhidkb.sys" (
    pnputil /add-driver "%~dp0..\src\drivers\vhidkb\vhidkb.inf" /install
    if %errorlevel% neq 0 (
        echo WARNING: vhidkb installation may have failed
    ) else (
        echo vhidkb installed successfully
    )
) else (
    echo WARNING: vhidkb.sys not found - skipping
)

REM Install Virtual HID Mouse
echo [2/4] Installing Virtual HID Mouse Driver...
if exist "%BUILD_DIR%\vhidmouse.sys" (
    pnputil /add-driver "%~dp0..\src\drivers\vhidmouse\vhidmouse.inf" /install
    if %errorlevel% neq 0 (
        echo WARNING: vhidmouse installation may have failed
    ) else (
        echo vhidmouse installed successfully
    )
) else (
    echo WARNING: vhidmouse.sys not found - skipping
)

REM Install Virtual Xbox Controller
echo [3/4] Installing Virtual Xbox Controller Driver...
if exist "%BUILD_DIR%\vxinput.sys" (
    pnputil /add-driver "%~dp0..\src\drivers\vxinput\vxinput.inf" /install
    if %errorlevel% neq 0 (
        echo WARNING: vxinput installation may have failed
    ) else (
        echo vxinput installed successfully
    )
) else (
    echo WARNING: vxinput.sys not found - skipping
)

REM Install Virtual Display
echo [4/4] Installing Virtual Display Driver...
if exist "%BUILD_DIR%\vdisplay.dll" (
    pnputil /add-driver "%~dp0..\src\drivers\vdisplay\vdisplay.inf" /install
    if %errorlevel% neq 0 (
        echo WARNING: vdisplay installation may have failed
    ) else (
        echo vdisplay installed successfully
    )
) else (
    echo WARNING: vdisplay.dll not found - skipping
)

echo.
echo ============================================
echo Installation complete
echo.
echo You may need to restart your computer.
echo.
pause
