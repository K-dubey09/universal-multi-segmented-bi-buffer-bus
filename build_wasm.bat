@echo off
REM WebAssembly Build Script for UMSBB Core (Windows)
REM Builds the core library for WebAssembly target

echo Building UMSBB Core for WebAssembly...
echo =======================================

REM Check if Emscripten is installed
where emcc >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: Emscripten not found. Please install Emscripten SDK.
    echo Visit: https://emscripten.org/docs/getting_started/downloads.html
    exit /b 1
)

REM Create build directory
if not exist build mkdir build
cd build

echo Compiling UMSBB complete core...

REM Compile with Emscripten
emcc ../umsbb_complete_core.c -o umsbb_core.js ^
    -O3 ^
    -s WASM=1 ^
    -s EXPORTED_FUNCTIONS="[\"_umsbb_create_buffer\",\"_umsbb_write_message\",\"_umsbb_read_message\",\"_umsbb_get_total_messages\",\"_umsbb_get_total_bytes\",\"_umsbb_get_pending_messages\",\"_umsbb_destroy_buffer\",\"_umsbb_get_max_message_size\",\"_umsbb_get_error_string\"]" ^
    -s EXPORTED_RUNTIME_METHODS="[\"ccall\", \"cwrap\"]" ^
    -s ALLOW_MEMORY_GROWTH=1 ^
    -s MAXIMUM_MEMORY=134217728 ^
    -s STACK_SIZE=1048576 ^
    -s INITIAL_MEMORY=16777216 ^
    -s MODULARIZE=1 ^
    -s EXPORT_NAME="UMSBBCore" ^
    -s USE_ES6_IMPORT_META=0 ^
    -s ENVIRONMENT="web,node"

if %ERRORLEVEL% equ 0 (
    echo ✅ WebAssembly compilation successful!
    echo Generated files:
    echo   - umsbb_core.js     (JavaScript loader^)
    echo   - umsbb_core.wasm   (WebAssembly binary^)
    
    REM Copy files to appropriate locations
    copy umsbb_core.wasm ..\connectors\javascript\ >nul
    copy umsbb_core.wasm ..\web\ >nul
    copy umsbb_core.js ..\connectors\javascript\ >nul
    copy umsbb_core.js ..\web\ >nul
    
    echo ✅ Files copied to connector directories
    
    REM Show file sizes
    echo.
    echo File sizes:
    dir umsbb_core.* /Q
) else (
    echo ❌ WebAssembly compilation failed!
    exit /b 1
)

echo.
echo Build complete! You can now use the WebAssembly module in:
echo   - JavaScript/TypeScript projects
echo   - Web browsers
echo   - Node.js applications

cd ..