@echo off
REM KVM-Drivers Build Script
REM Requires: Visual Studio 2022, Windows Driver Kit (WDK)

setlocal enabledelayedexpansion

echo ==========================================
echo KVM-Drivers Build System
echo ==========================================
echo.

REM Check for Visual Studio
if not exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    if not exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        if not exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
            echo ERROR: Visual Studio 2022 not found
            exit /b 1
        )
    )
)

REM Check for WDK
if not exist "%ProgramFiles(x86)%\Windows Kits\10" (
    echo ERROR: Windows Driver Kit not found
    exit /b 1
)

REM Parse arguments
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

set ARCH=%2
if "%ARCH%"=="" set ARCH=x64

echo Build Configuration: %CONFIG%
echo Architecture: %ARCH%
echo.

REM Setup VS environment
call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    call "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
)
if errorlevel 1 (
    call "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
)

REM Create build output directory
if not exist "build" mkdir build
if not exist "build\%CONFIG%" mkdir build\%CONFIG%

echo Building drivers...
echo.

REM Build Virtual HID Keyboard Driver
echo [1/4] Building vhidkb.sys (Virtual HID Keyboard)...
cd src\drivers\vhidkb
msbuild vhidkb.vcxproj /p:Configuration=%CONFIG% /p:Platform=%ARCH% /p:TargetVersion=Windows10 /m
if errorlevel 1 (
    echo ERROR: vhidkb build failed
    exit /b 1
)
cd ..\..\..

REM Build Virtual HID Mouse Driver
echo [2/4] Building vhidmouse.sys (Virtual HID Mouse)...
cd src\drivers\vhidmouse
msbuild vhidmouse.vcxproj /p:Configuration=%CONFIG% /p:Platform=%ARCH% /p:TargetVersion=Windows10 /m
if errorlevel 1 (
    echo ERROR: vhidmouse build failed
    exit /b 1
)
cd ..\..\..

REM Build Virtual Xbox Controller Driver
echo [3/4] Building vxinput.sys (Virtual Xbox Controller)...
cd src\drivers\vxinput
msbuild vxinput.vcxproj /p:Configuration=%CONFIG% /p:Platform=%ARCH% /p:TargetVersion=Windows10 /m
if errorlevel 1 (
    echo ERROR: vxinput build failed
    exit /b 1
)
cd ..\..\..

REM Build Virtual Display Driver (IDD)
echo [4/4] Building vdisplay.dll (Virtual Display Driver)...
cd src\drivers\vdisplay
msbuild vdisplay.vcxproj /p:Configuration=%CONFIG% /p:Platform=%ARCH% /p:TargetVersion=Windows10 /m
if errorlevel 1 (
    echo ERROR: vdisplay build failed
    exit /b 1
)
cd ..\..\..

echo.
echo Building user-mode components...
echo.

REM Build Core Service
echo [1/3] Building KVMService.exe...
cd src\usermode\core
msbuild core.vcxproj /p:Configuration=%CONFIG% /p:Platform=%ARCH% /m
if errorlevel 1 (
    echo ERROR: Core service build failed
    exit /b 1
)
cd ..\..\..

REM Build Remote Protocol Handlers
echo [2/3] Building protocol handlers...
cd src\usermode\remote
msbuild remote.vcxproj /p:Configuration=%CONFIG% /p:Platform=%ARCH% /m
if errorlevel 1 (
    echo ERROR: Remote protocol build failed
    exit /b 1
)
cd ..\..\..

echo.
echo Build complete!
echo Output: build\%CONFIG%\
echo.

endlocal
