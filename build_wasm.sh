#!/bin/bash

# WebAssembly Build Script for UMSBB Core
# Builds the core library for WebAssembly target

set -e

echo "Building UMSBB Core for WebAssembly..."
echo "======================================="

# Check if Emscripten is installed
if ! command -v emcc &> /dev/null; then
    echo "Error: Emscripten not found. Please install Emscripten SDK."
    echo "Visit: https://emscripten.org/docs/getting_started/downloads.html"
    exit 1
fi

# Create build directory
mkdir -p build
cd build

# Compilation flags
EMCC_FLAGS=(
    -O3                          # Optimize for performance
    -s WASM=1                    # Enable WebAssembly
    -s EXPORTED_FUNCTIONS='[
        "_umsbb_create_buffer",
        "_umsbb_write_message", 
        "_umsbb_read_message",
        "_umsbb_get_total_messages",
        "_umsbb_get_total_bytes",
        "_umsbb_get_pending_messages",
        "_umsbb_destroy_buffer",
        "_umsbb_get_max_message_size",
        "_umsbb_get_error_string"
    ]'                           # Export C functions
    -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]'  # Export runtime helpers
    -s ALLOW_MEMORY_GROWTH=1     # Allow memory to grow
    -s MAXIMUM_MEMORY=134217728  # 128MB max memory
    -s STACK_SIZE=1048576        # 1MB stack
    -s INITIAL_MEMORY=16777216   # 16MB initial memory
    -s MODULARIZE=1              # Create a module
    -s EXPORT_NAME='UMSBBCore'   # Module name
    -s USE_ES6_IMPORT_META=0     # Compatibility
    -s ENVIRONMENT='web,node'    # Target environments
    --pre-js ../wasm/pre.js     # Pre-execution JavaScript
    --post-js ../wasm/post.js   # Post-execution JavaScript
)

echo "Compiling UMSBB complete core..."
emcc ../umsbb_complete_core.c -o umsbb_core.js "${EMCC_FLAGS[@]}"

if [ $? -eq 0 ]; then
    echo "✅ WebAssembly compilation successful!"
    echo "Generated files:"
    echo "  - umsbb_core.js     (JavaScript loader)"
    echo "  - umsbb_core.wasm   (WebAssembly binary)"
    
    # Copy files to appropriate locations
    cp umsbb_core.wasm ../connectors/javascript/
    cp umsbb_core.wasm ../web/
    cp umsbb_core.js ../connectors/javascript/
    cp umsbb_core.js ../web/
    
    echo "✅ Files copied to connector directories"
    
    # Show file sizes
    echo ""
    echo "File sizes:"
    ls -lh umsbb_core.*
    
else
    echo "❌ WebAssembly compilation failed!"
    exit 1
fi

echo ""
echo "Build complete! You can now use the WebAssembly module in:"
echo "  - JavaScript/TypeScript projects"
echo "  - Web browsers"
echo "  - Node.js applications"