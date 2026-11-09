
@echo off
setlocal EnableExtensions
title HB Browser - Windows Build

cd /d "%~dp0"

echo.
echo ==========================================
echo       HOMEBAX STUDIOS - HB BROWSER
echo             WINDOWS BUILDER
echo ==========================================
echo.

:: Configuration
set "BUILD_DIR=build"
set "BUILD_TYPE=Release"
set "GENERATOR=Visual Studio 17 2022"
set "ARCH=x64"
set "BUILD_JOBS=%NUMBER_OF_PROCESSORS%"

:: Check CMake
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake was not found!
    pause
    exit /b 1
)

:: Check CMakeLists
if not exist "CMakeLists.txt" (
    echo [ERROR] CMakeLists.txt not found!
    pause
    exit /b 1
)

echo [INFO] Project: HB Browser
echo [INFO] Generator: %GENERATOR%
echo [INFO] Architecture: %ARCH%
echo [INFO] Configuration: %BUILD_TYPE%
echo [INFO] CPU threads: %BUILD_JOBS%
echo.

:: Configure
echo [1/2] Configuring project...

cmake -S . -B "%BUILD_DIR%" -G "%GENERATOR%" -A %ARCH%

if errorlevel 1 (
    echo.
    echo [ERROR] CMake configuration failed!
    pause
    exit /b 1
)

:: Build
echo.
echo [2/2] Building project...

cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel %BUILD_JOBS%

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo ==========================================
echo          BUILD SUCCESSFUL!
echo ==========================================
echo.

pause
exit /b 0
