echo "Building UMSBB v4.0 Adaptive Test System..."
echo "============================================"
echo ""

echo "🔍 Checking for GPU libraries..."

REM Check for CUDA
if exist "%CUDA_PATH%\include\cuda_runtime.h" (
    echo "   ✅ CUDA found at %CUDA_PATH%"
    set GPU_AVAILABLE=1
    set GPU_FLAGS=-DHAS_CUDA -I"%CUDA_PATH%\include" -L"%CUDA_PATH%\lib\x64" -lcudart
) else (
    echo "   ❌ CUDA not found"
    set GPU_AVAILABLE=0
    set GPU_FLAGS=
)

REM Check for basic GPU support (simplified)
if %GPU_AVAILABLE%==1 (
    echo ""
    echo "🎮 GPU ACCELERATION DETECTED!"
    echo "   Building with GPU support..."
    set BUILD_MODE=GPU-Accelerated
) else (
    echo ""
    echo "🖥️  CPU-ONLY MODE"
    echo "   Building optimized CPU version..."
    set BUILD_MODE=CPU-Optimized
)

echo ""
echo "🔨 Compiling adaptive test..."
echo "   Source: ultimate_adaptive_test.c"
echo "   Output: ultimate_adaptive_test.exe"
echo "   Mode: %BUILD_MODE%"

REM Build command
gcc -std=c11 -O3 -DNDEBUG -Wall %GPU_FLAGS% -o ultimate_adaptive_test.exe ultimate_adaptive_test.c

if %ERRORLEVEL%==0 (
    echo ""
    echo "✅ BUILD SUCCESSFUL!"
    echo ""
    echo "╔═══════════════════════════════════════════════════════╗"
    echo "║              ADAPTIVE BUILD COMPLETE                 ║"
    echo "╠═══════════════════════════════════════════════════════╣"
    echo "║ Output: ultimate_adaptive_test.exe                   ║"
    echo "║ Mode:   %BUILD_MODE%                        ║"
    echo "║ Ready:  GPU detection with CPU fallback              ║"
    echo "╚═══════════════════════════════════════════════════════╝"
    echo ""
    echo "🚀 Ready to run! Try: .\ultimate_adaptive_test.exe --help"
) else (
    echo ""
    echo "❌ BUILD FAILED!"
    echo "   Falling back to simple CPU-only build..."
    echo ""
    gcc -std=c11 -O3 -DNDEBUG -o ultimate_adaptive_test.exe ultimate_adaptive_test.c
    if %ERRORLEVEL%==0 (
        echo "✅ CPU-only build successful!"
    ) else (
        echo "❌ Build failed completely. Check GCC installation."
    )
)

echo ""
echo "🎯 Adaptive build process complete!"