@echo off
REM Build script for UMSBB Comprehensive Test Suite

echo Building UMSBB Comprehensive Test Suite...
echo ==========================================

REM Check for compiler
where gcc >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: GCC not found. Please install MinGW or similar.
    exit /b 1
)

echo Compiling comprehensive test...

REM Compile the comprehensive test
gcc -std=c11 -O2 -Wall -Wextra ^
    -I. ^
    comprehensive_test.c ^
    -o comprehensive_test.exe ^
    -lpthread

if %ERRORLEVEL% equ 0 (
    echo ✅ Build successful!
    echo.
    echo Running comprehensive test suite...
    echo.
    comprehensive_test.exe
) else (
    echo ❌ Build failed!
    exit /b 1
)

echo.
echo Test completed! Check the results above.
echo Open web\index.html in a browser for the interactive dashboard.