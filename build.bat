@echo off

echo ================================
echo       Homebax Browser
echo ================================
echo.

if not exist build mkdir build

echo Compiling...

cl.exe ^
    /std:c++20 ^
    /EHsc ^
    /W4 ^
    /Fe:build\HomebaxBrowser.exe ^
    src\main.cpp ^
    src\ui\Window.cpp ^
    user32.lib ^
    gdi32.lib

if errorlevel 1 (
    echo.
    echo ================================
    echo          BUILD FAILED
    echo ================================
    echo.
    pause
    exit /b 1
)

echo.
echo ================================
echo           BUILD OK
echo ================================
echo.

start "" "build\HomebaxBrowser.exe"

pause