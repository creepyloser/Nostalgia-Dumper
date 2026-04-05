@echo off
REM ==============================================================================
REM UE5 Offset Dumper - Build Script for Windows
REM Usage: build.bat [config] [generator]
REM   config: Debug or Release (default: Release)
REM   generator: Visual Studio version (default: Visual Studio 17 2022)
REM ==============================================================================

setlocal enabledelayedexpansion

set CONFIG=Release
set GENERATOR=Visual Studio 17 2022

if not "%1"=="" set CONFIG=%1
if not "%2"=="" set GENERATOR=%2

echo.
echo ========================================
echo UE5 Offset Dumper - Build Script
echo ========================================
echo Configuration: %CONFIG%
echo Generator: %GENERATOR%
echo.

REM Check if CMake is available
cmake --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake not found. Please install CMake 3.20 or later.
    pause
    exit /b 1
)

REM Check if ImGui is set up
if not exist "vendor\imgui" (
    echo.
    echo WARNING: ImGui not found in vendor directory!
    echo.
    echo Setting up ImGui...
    mkdir vendor
    cd vendor
    git clone https://github.com/ocornut/imgui.git
    if errorlevel 1 (
        echo ERROR: Failed to clone ImGui. Make sure Git is installed.
        cd ..
        pause
        exit /b 1
    )
    cd ..
    echo ImGui setup complete.
)

REM Create build directory if it doesn't exist
if not exist "build" (
    echo Creating build directory...
    mkdir build
)

REM Navigate to build directory
cd build

REM Run CMake
echo.
echo Generating project files...
cmake .. -G "%GENERATOR%" -A x64 -DCMAKE_BUILD_TYPE=%CONFIG%
if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

REM Build the project
echo.
echo Building project...
cmake --build . --config %CONFIG% --parallel
if errorlevel 1 (
    echo ERROR: Build failed!
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo ========================================
echo Build successful!
echo ========================================
echo Output: bin\%CONFIG%\UEDumper.exe
echo.
pause
