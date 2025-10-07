#!/bin/bash
echo "======================================="
echo "UMSBB WebAssembly Builder v4.0"
echo "======================================="
echo

# Check if Emscripten is available
if ! command -v emcc &> /dev/null; then
    echo "Error: Emscripten not found in PATH."
    echo
    echo "Please install Emscripten SDK:"
    echo "1. Download from: https://emscripten.org/"
    echo "2. Or use the included installer:"
    echo "   cd emsdk"
    echo "   ./emsdk install latest"
    echo "   ./emsdk activate latest"
    echo "   source ./emsdk_env.sh"
    echo
    exit 1
fi

echo "✅ Emscripten found, compiling WebAssembly core..."
echo

# Create build directory
mkdir -p build

# Compile the WebAssembly core
emcc -O3 -s WASM=1 \
    -s "EXPORTED_FUNCTIONS=['_umsbb_init_system','_umsbb_shutdown_system','_umsbb_create_buffer','_umsbb_write_message','_umsbb_read_message','_umsbb_destroy_buffer','_umsbb_get_total_messages','_umsbb_get_total_bytes','_umsbb_get_pending_messages','_umsbb_get_version','_umsbb_get_error_string','_umsbb_run_performance_test','_umsbb_malloc','_umsbb_free']" \
    -s "EXPORTED_RUNTIME_METHODS=['ccall', 'cwrap']" \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s INITIAL_MEMORY=33554432 \
    -s MAXIMUM_MEMORY=134217728 \
    -s MODULARIZE=1 \
    -s EXPORT_NAME=UMSBBCore \
    -s ENVIRONMENT=web,node \
    umsbb_wasm_core.c -o umsbb_core.js

if [ $? -eq 0 ]; then
    echo
    echo "✅ WebAssembly compilation successful!"
    echo
    echo "Generated files:"
    echo "  🔹 umsbb_core.js   - JavaScript loader/wrapper"
    echo "  🔹 umsbb_core.wasm - WebAssembly binary"
    echo
    echo "📁 Files are ready for web deployment!"
    echo
    echo "Usage:"
    echo "  1. Include umsbb_core.js in your webpage"
    echo "  2. Load with: const core = await UMSBBCore()"
    echo "  3. Use: core.ccall('umsbb_init_system', 'number', [], [])"
    echo
    echo "🌐 Open web_demo.html to see it in action!"
else
    echo
    echo "❌ Compilation failed! Check error messages above."
    echo
    echo "Common issues:"
    echo "  - Missing header files"
    echo "  - Syntax errors in source code"
    echo "  - Memory configuration problems"
    echo
    echo "💡 Try editing umsbb_wasm_core.c and rebuild"
fi

echo