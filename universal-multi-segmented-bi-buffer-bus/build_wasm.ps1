#!/usr/bin/env powershell

# WebAssembly B    if (Test-Path "universal_multi_segmented_bi_buffer_bus.js" -and Test-Path "universal_multi_segmented_bi_buffer_bus.wasm") {
        Write-Host "✅ Build successful!" -ForegroundColor Green
        Write-Host "Generated files:" -ForegroundColor Cyan
        Write-Host "  - universal_multi_segmented_bi_buffer_bus.js ($(((Get-Item universal_multi_segmented_bi_buffer_bus.js).Length / 1KB).ToString('F1')) KB)" -ForegroundColor Gray
        Write-Host "  - universal_multi_segmented_bi_buffer_bus.wasm ($(((Get-Item universal_multi_segmented_bi_buffer_bus.wasm).Length / 1KB).ToString('F1')) KB)" -ForegroundColor GrayScript for Universal Multi-Segmented Bi-Buffer Bus
# Requires Emscripten SDK to be in PATH

Write-Host "🚀 Building Universal Multi-Segmented Bi-Buffer Bus for WebAssembly..." -ForegroundColor Green

# Check if emcc is available
if (!(Get-Command emcc -ErrorAction SilentlyContinue)) {
    Write-Host "❌ emcc not found! Please install and activate Emscripten SDK" -ForegroundColor Red
    Write-Host "Download from: https://emscripten.org/docs/getting_started/downloads.html" -ForegroundColor Yellow
    exit 1
}

# Get all source files
$sourceFiles = Get-ChildItem -Path "./src" -Filter "*.c" | ForEach-Object { $_.FullName }
$sourceList = $sourceFiles -join " "

Write-Host "📁 Source files found: $($sourceFiles.Count)" -ForegroundColor Cyan
$sourceFiles | ForEach-Object { Write-Host "  - $(Split-Path $_ -Leaf)" -ForegroundColor Gray }

# Build command
$buildCmd = @(
    "emcc"
    $sourceList
    "-Iinclude"
    "-o universal_multi_segmented_bi_buffer_bus.js"
    "-s EXPORTED_FUNCTIONS=`"['_umsbb_init','_umsbb_submit_to','_umsbb_drain_from','_umsbb_get_feedback','_malloc','_free']`""
    "-s EXPORTED_RUNTIME_METHODS=`"['HEAPU8','HEAPU32']`""
    "-s ALLOW_MEMORY_GROWTH=1"
    "-s MODULARIZE=1" 
    "-s EXPORT_NAME=`"'UniversalMultiSegmentedBiBufferBusModule'`""
    "-s TOTAL_STACK=1MB"
    "-s INITIAL_MEMORY=16MB"
    "-O2"
    "--no-entry"
)

Write-Host "🔨 Building..." -ForegroundColor Yellow
Write-Host "Command: $($buildCmd -join ' ')" -ForegroundColor Gray

try {
    & $buildCmd[0] $buildCmd[1..($buildCmd.Length-1)]
    
    if (Test-Path "somakernel.js" -and Test-Path "somakernel.wasm") {
        Write-Host "✅ Build successful!" -ForegroundColor Green
        Write-Host "📦 Generated files:" -ForegroundColor Cyan
        Write-Host "  - somakernel.js ($(((Get-Item somakernel.js).Length / 1KB).ToString('F1')) KB)" -ForegroundColor Gray
        Write-Host "  - somakernel.wasm ($(((Get-Item somakernel.wasm).Length / 1KB).ToString('F1')) KB)" -ForegroundColor Gray
        
        # Create simple test
        Write-Host "🧪 Creating test file..." -ForegroundColor Yellow
        @'
// Test Universal Multi-Segmented Bi-Buffer Bus WASM Module
const UniversalMultiSegmentedBiBufferBusModule = require('./universal_multi_segmented_bi_buffer_bus.js');

UniversalMultiSegmentedBiBufferBusModule().then(Module => {
    console.log('🚀 Universal Multi-Segmented Bi-Buffer Bus WASM Module Loaded');
    
    // Test basic functions
    const bus = Module._umsbb_init(1024, 2048);
    console.log('✅ Bus initialized:', bus);
    
    // Submit test message
    const testMsg = "Hello WASM!";
    const msgPtr = Module._malloc(testMsg.length + 1);
    Module.writeStringToMemory(testMsg, msgPtr);
    
    Module._umsbb_submit_to(bus, 0, msgPtr, testMsg.length);
    console.log('📤 Message submitted');
    
    // Drain message
    Module._umsbb_drain_from(bus, 0);
    console.log('📥 Message drained');
    
    // Get feedback
    const countPtr = Module._malloc(4);
    const feedbackPtr = Module._umsbb_get_feedback(bus, countPtr);
    const count = Module.getValue(countPtr, 'i32');
    console.log('🧾 Feedback entries:', count);
    
    // Cleanup
    Module._free(msgPtr);
    Module._free(countPtr);
    Module._umsbb_free(bus);
    
    console.log('✅ Test completed successfully!');
}).catch(err => {
    console.error('❌ Error loading module:', err);
});
'@ | Out-File -FilePath "wasm_test.js" -Encoding UTF8
        
        Write-Host "📄 Test file created: wasm_test.js" -ForegroundColor Green
        Write-Host "🔍 Run with: node wasm_test.js" -ForegroundColor Cyan
        
    } else {
        Write-Host "❌ Build failed - output files not found" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "❌ Build failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

Write-Host "🎉 WebAssembly build complete!" -ForegroundColor Green