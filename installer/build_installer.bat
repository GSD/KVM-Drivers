@echo off
REM build_installer.bat - Build WiX MSI installer for KVM-Drivers
REM Usage: build_installer.bat [Release|Debug]

setlocal enabledelayedexpansion

set CONFIGURATION=%1
if "%CONFIGURATION%"=="" set CONFIGURATION=Release

echo ============================================
echo KVM-Drivers MSI Installer Builder
echo Configuration: %CONFIGURATION%
echo ============================================
echo.

REM Check for WiX Toolset
set WIX_PATH=C:\Program Files (x86)\WiX Toolset v3.11\bin
if not exist "%WIX_PATH%\candle.exe" (
    echo Error: WiX Toolset not found at %WIX_PATH%
    echo Please install WiX Toolset v3.11 or later from:
    echo https://wixtoolset.org/releases/
    exit /b 1
)

set PATH=%PATH%;%WIX_PATH%

REM Check for built files
set BUILD_DIR=..\build\%CONFIGURATION%
if not exist "%BUILD_DIR%\drivers\vhidkb.sys" (
    echo Error: Driver binaries not found. Please build drivers first.
    echo Run: ..\scripts\build.bat %CONFIGURATION%
    exit /b 1
)

set PROJECT_DIR=%CD%

REM Create output directory
if not exist "output" mkdir output

REM Copy assets if they don't exist
if not exist "assets" mkdir assets
if not exist "assets\installer_banner.bmp" (
    echo Creating placeholder banner...
    copy "%WIX_PATH%\wixui\WixUI_Bmp_Banner.bmp" "assets\installer_banner.bmp" >nul 2>&1
)
if not exist "assets\installer_dialog.bmp" (
    echo Creating placeholder dialog image...
    copy "%WIX_PATH%\wixui\WixUI_Bmp_Dialog.bmp" "assets\installer_dialog.bmp" >nul 2>&1
)

REM Convert LICENSE to RTF if needed
if not exist "LICENSE.rtf" (
    if exist "..\LICENSE" (
        echo Converting LICENSE to RTF format...
        powershell -Command "(Get-Content '..\LICENSE') | Set-Content 'LICENSE.rtf'"
    ) else (
        echo Creating placeholder license...
        echo KVM-Drivers License > LICENSE.rtf
    )
)

echo.
echo Step 1: Compiling WiX source...
candle.exe -arch x64 -dBuildDir=%BUILD_DIR% -dProjectDir=%PROJECT_DIR% -out output\ KVM-Drivers.wxs
if errorlevel 1 (
    echo Error: WiX compilation failed
    exit /b 1
)

echo.
echo Step 2: Linking MSI package...
light.exe -ext WixUIExtension -ext WixUtilExtension -out output\KVM-Drivers-1.0.0-x64.msi output\KVM-Drivers.wixobj
if errorlevel 1 (
    echo Error: MSI linking failed
    exit /b 1
)

echo.
echo ============================================
echo Installer build complete!
echo Output: output\KVM-Drivers-1.0.0-x64.msi
echo ============================================
echo.

REM Sign MSI if certificate is available
if exist "%SIGNING_CERT%" (
    echo Signing installer...
    signtool.exe sign /f "%SIGNING_CERT%" /p "%SIGNING_PASSWORD%" /tr http://timestamp.digicert.com /td sha256 /fd sha256 "output\KVM-Drivers-1.0.0-x64.msi"
    if errorlevel 1 (
        echo Warning: Installer signing failed
    ) else (
        echo Installer signed successfully
    )
) else (
    echo Note: Installer not signed. Set SIGNING_CERT and SIGNING_PASSWORD environment variables to sign.
)

exit /b 0
