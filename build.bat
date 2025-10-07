@echo off
title UMSBB v4.0 GPU Build System

echo =====================================================
echo  UMSBB v4.0 GPU-Accelerated Build System
echo  Building multi-GB/s system with web interface
echo =====================================================
echo.

REM Check if PowerShell is available
powershell -Command "exit 0" >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: PowerShell is not available
    echo Please install PowerShell or run build_gpu_system.sh in WSL
    pause
    exit /b 1
)

echo Running PowerShell build script...
powershell -ExecutionPolicy Bypass -File "build_simple.ps1"

echo.
echo Build process completed!
pause