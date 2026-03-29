@echo off
REM KVM-Drivers Build Script v2.0
REM Requires: Visual Studio 2022, Windows Driver Kit (WDK)
REM Usage: build.bat [Release|Debug] [all|drivers|usermode|tests|clean]

setlocal enabledelayedexpansion

REM Parse arguments
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release
if /I "%CONFIG%"=="clean" (
    set CLEAN=1
    set CONFIG=Release
)

set TARGET=%2
if "%TARGET%"=="" set TARGET=all

set ARCH=x64

set BUILDDIR=build\%CONFIG%
set LOGDIR=%BUILDDIR%\logs

echo ==========================================
echo KVM-Drivers Build System v2.0
echo ==========================================
echo Configuration: %CONFIG%
echo Architecture: %ARCH%
echo Target: %TARGET%
echo ==========================================
echo.

REM Check for Visual Studio via vswhere
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% (
    echo ERROR: Visual Studio Installer not found. Please install VS 2022.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set VSINSTALL=%%i
)

if not defined VSINSTALL (
    echo ERROR: Visual Studio 2022 not found
    exit /b 1
)

echo Found Visual Studio at: %VSINSTALL%

REM Setup MSBuild
set MSBUILD="%VSINSTALL%\MSBuild\Current\Bin\MSBuild.exe"
if not exist %MSBUILD% (
    set MSBUILD="%VSINSTALL%\MSBuild\15.0\Bin\MSBuild.exe"
)

if not exist %MSBUILD% (
    echo ERROR: MSBuild not found
    exit /b 1
)

REM Check for WDK
if not exist "%ProgramFiles(x86)%\Windows Kits\10\bin" (
    echo ERROR: Windows Driver Kit not found
    exit /b 1
)

echo Found Windows Driver Kit
echo.

REM Clean if requested
if "%CLEAN%"=="1" (
    echo Cleaning build directory...
    rmdir /s /q build 2>nul
    echo Clean complete.
    if "%TARGET%"=="clean" exit /b 0
    echo.
)

REM Create directories
if not exist %BUILDDIR% mkdir %BUILDDIR%
if not exist %LOGDIR% mkdir %LOGDIR%
if not exist %BUILDDIR%\drivers mkdir %BUILDDIR%\drivers
if not exist %BUILDDIR%\bin mkdir %BUILDDIR%\bin
if not exist %BUILDDIR%\tests mkdir %BUILDDIR%\tests

set BUILDFLAGS=/p:Configuration=%CONFIG% /p:Platform=%ARCH% /m /v:minimal /p:WarningLevel=0

REM Build
if "%TARGET%"=="all" goto :BuildAll
if "%TARGET%"=="drivers" goto :BuildDrivers
if "%TARGET%"=="usermode" goto :BuildUsermode
if "%TARGET%"=="tests" goto :BuildTests
goto :BuildSingle

:BuildAll
echo Building solution: KVM-Drivers.sln...
%MSBUILD% KVM-Drivers.sln %BUILDFLAGS% >%LOGDIR%\build_all.log 2>&1
if errorlevel 1 (
    echo FAILED! See %LOGDIR%\build_all.log
    exit /b 1
)
echo SUCCESS
goto :CopyArtifacts

:BuildDrivers
echo Building driver projects...
for %%p in (vhidkb vhidmouse vxinput vdisplay) do (
    echo   - Building %%p...
    %MSBUILD% src\drivers\%%p\%%p.vcxproj %BUILDFLAGS% >%LOGDIR%\%%p.log 2>&1
    if errorlevel 1 (
        echo FAILED! See %LOGDIR%\%%p.log
        exit /b 1
    )
)
echo SUCCESS
goto :CopyArtifacts

:BuildUsermode
echo Building user-mode projects...
for %%p in (core remote tray) do (
    echo   - Building %%p...
    %MSBUILD% src\usermode\%%p\%%p.vcxproj %BUILDFLAGS% >%LOGDIR%\%%p.log 2>&1
    if errorlevel 1 (
        echo FAILED! See %LOGDIR%\%%p.log
        exit /b 1
    )
)
echo SUCCESS
goto :CopyArtifacts

:BuildTests
echo Building test projects...
%MSBUILD% tests\test_harness.vcxproj %BUILDFLAGS% >%LOGDIR%\test_harness.log 2>&1
if errorlevel 1 (
    echo FAILED! See %LOGDIR%\test_harness.log
    exit /b 1
)
echo SUCCESS
goto :CopyArtifacts

:BuildSingle
echo Building single project: %TARGET%
%MSBUILD% %TARGET% %BUILDFLAGS% >%LOGDIR%\single.log 2>&1
if errorlevel 1 (
    echo FAILED! See %LOGDIR%\single.log
    exit /b 1
)
echo SUCCESS

:CopyArtifacts
echo.
echo Copying artifacts...

REM Copy drivers
copy /y %BUILDDIR%\*.sys %BUILDDIR%\drivers\ >nul 2>&1
copy /y %BUILDDIR%\*.inf %BUILDDIR%\drivers\ >nul 2>&1
copy /y %BUILDDIR%\*.cat %BUILDDIR%\drivers\ >nul 2>&1

REM Copy executables
copy /y %BUILDDIR%\*.exe %BUILDDIR%\bin\ >nul 2>&1
copy /y %BUILDDIR%\*.dll %BUILDDIR%\bin\ >nul 2>&1

REM Copy tests
for %%f in (test_*.exe) do (
    copy /y %%f %BUILDDIR%\tests\ >nul 2>&1
)

REM Copy scripts
copy /y scripts\*.bat %BUILDDIR%\ >nul 2>&1

echo SUCCESS
echo.

:Summary
echo ==========================================
echo BUILD COMPLETE
echo ==========================================
echo Output Directory: %BUILDDIR%\
echo   drivers\ - Kernel drivers (.sys, .inf)
echo   bin\     - Applications (.exe, .dll)
echo   tests\   - Test executables
echo   logs\   - Build logs
echo.
echo To install drivers, run: install.bat %CONFIG%
echo To run tests, run: tests\test_harness.exe
echo ==========================================
echo.

endlocal
