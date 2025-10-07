# UMSBB Core Customization Guide

Welcome to the **Universal Multi-Segmented Bi-Buffer Bus (UMSBB) v4.0** core customization system! This guide will help you understand how to customize and build your own WebAssembly core.

## 🚀 Quick Start

### Option 1: Use Pre-built Core
```bash
# Use the pre-built WebAssembly core (fastest option)
# Files are ready in web/ and connectors/javascript/
```

### Option 2: Build with Python Builder
```bash
# Build with default configuration
python build_core.py build

# Build with custom features
python build_core.py build --no-performance-testing --enable-memory-debugging

# Build for different platforms
python build_core.py build --target native-linux
python build_core.py build --target native-windows
```

### Option 3: Manual Build
```bash
# Windows (PowerShell)
.\build_wasm.bat

# Linux/macOS
./build_wasm.sh
```

## 🛠️ Customization Options

### Core Features (can be toggled)

| Feature | Default | Description |
|---------|---------|-------------|
| `performance_testing` | ✅ | Includes benchmarking and performance testing utilities |
| `detailed_statistics` | ✅ | Comprehensive statistics tracking and reporting |
| `error_checking` | ✅ | Extensive error checking and validation |
| `memory_debugging` | ❌ | Memory allocation tracking and debugging |
| `custom_allocators` | ❌ | Support for custom memory allocators |
| `network_support` | ❌ | Network communication features |
| `compression` | ❌ | Message compression capabilities |
| `encryption` | ❌ | Message encryption support |

### Build Targets

- **wasm**: WebAssembly (browser/Node.js compatible)
- **native-linux**: Linux shared library
- **native-windows**: Windows DLL
- **native-macos**: macOS dynamic library

### Optimization Levels

- **O0**: No optimization (fastest compile, largest size)
- **O1**: Basic optimization
- **O2**: Standard optimization (good balance)
- **O3**: Maximum optimization (smallest size, best performance) ⭐ **Default**

## 📝 Core File Structure

The unified core is contained in **`umsbb_complete_core.c`** (~1000 lines):

```c
// Core Components:
// 1. Multi-segment lock-free buffer architecture
// 2. Atomic operations for thread safety  
// 3. Message handling with headers and checksums
// 4. Comprehensive statistics tracking
// 5. Error handling and validation
// 6. Memory management utilities
// 7. WebAssembly export functions
// 8. Performance testing framework
```

## 🔧 Advanced Customization

### 1. Edit Source Code
```c
// Edit umsbb_complete_core.c directly
// Example: Change buffer segment count
#define UMSBB_NUM_SEGMENTS 16  // Default: 8

// Example: Modify message size limits
#define UMSBB_MAX_MESSAGE_SIZE (2 * 1024 * 1024)  // Default: 1MB
```

### 2. Use Python Builder
```python
from build_core import UMSBBCoreBuilder

builder = UMSBBCoreBuilder()

# Configure features
builder.configure_features(
    performance_testing=True,
    memory_debugging=True,
    compression=True
)

# Set optimization and target
builder.set_optimization(2)
builder.set_target('wasm')

# Build with custom source
builder.build(custom_source='my_custom_core.c')
```

### 3. Custom Build Configuration
```bash
# Generate build config header
python build_core.py configure --enable-memory-debugging

# Edit umsbb_build_config.h manually
# Then build
python build_core.py build
```

## 🌐 WebAssembly Integration

### Exported Functions
The core exports these functions for WebAssembly:

```javascript
// Core Management
umsbb_init_system()
umsbb_shutdown_system()

// Buffer Operations  
umsbb_create_buffer(segment_size, num_segments)
umsbb_destroy_buffer(buffer_id)

// Message Operations
umsbb_write_message(buffer_id, data, size)
umsbb_read_message(buffer_id, output_buffer, max_size)

// Statistics and Info
umsbb_get_total_messages(buffer_id)
umsbb_get_total_bytes(buffer_id) 
umsbb_get_comprehensive_stats(buffer_id)
umsbb_get_system_info()

// Performance Testing (if enabled)
umsbb_run_performance_test(buffer_id, message_count, message_size)
```

### Usage Example
```javascript
// Load the core
const UMSBBCore = require('./umsbb_core.js');

UMSBBCore().then(core => {
    // Initialize system
    core.ccall('umsbb_init_system', 'number', [], []);
    
    // Create buffer
    const bufferId = core.ccall('umsbb_create_buffer', 'number', 
        ['number', 'number'], [1024 * 1024, 8]);
    
    // Write message
    const message = "Hello UMSBB!";
    const messagePtr = core._malloc(message.length + 1);
    core.stringToUTF8(message, messagePtr, message.length + 1);
    
    const result = core.ccall('umsbb_write_message', 'number',
        ['number', 'number', 'number'], [bufferId, messagePtr, message.length]);
    
    core._free(messagePtr);
});
```

## 🔍 Performance Tuning

### For Maximum Throughput
```bash
python build_core.py build --target wasm -O 3 --no-detailed-statistics
```

### For Debugging
```bash
python build_core.py build --target wasm -O 0 --enable-memory-debugging
```

### For Minimal Size
```bash
python build_core.py build --target wasm -O 3 --no-performance-testing --no-detailed-statistics
```

## 📊 Performance Characteristics

The unified core achieves:
- **1+ million messages/second** throughput
- **Lock-free** multi-segment architecture
- **Thread-safe** atomic operations
- **Memory efficient** circular buffers
- **Cross-platform** compatibility

## 🔄 Integration with Language Connectors

After building, the core automatically deploys to:
- `connectors/javascript/` - For Node.js integration
- `connectors/python/` - For Python ctypes binding
- `connectors/cpp/` - For C++ integration
- `connectors/rust/` - For Rust FFI
- `web/` - For browser usage

## 🎯 Use Cases

### Real-time Applications
```c
// High-frequency trading, gaming, IoT
#define UMSBB_NUM_SEGMENTS 16
#define UMSBB_MAX_MESSAGE_SIZE 4096
```

### Data Processing Pipelines
```c
// ETL, stream processing, analytics
#define UMSBB_NUM_SEGMENTS 32
#define UMSBB_MAX_MESSAGE_SIZE (1024 * 1024)
```

### Embedded Systems
```c
// IoT devices, microcontrollers
#define UMSBB_NUM_SEGMENTS 4
#define UMSBB_MAX_MESSAGE_SIZE 1024
```

## 🛡️ Error Handling

The core provides comprehensive error handling:
- **Return codes** for all operations
- **Error strings** for detailed diagnostics
- **Validation** of all parameters
- **Memory safety** checks

## 📦 Distribution

After customization and building:
1. **Single .wasm file** contains all functionality
2. **Language connectors** provide easy integration
3. **Web dashboard** offers monitoring interface
4. **Documentation** and examples included

## 🔧 Troubleshooting

### Build Errors
```bash
# Check compiler installation
emcc --version  # For WebAssembly
gcc --version   # For native

# Clean and rebuild
python build_core.py clean
python build_core.py build
```

### Runtime Issues
```c
// Enable debugging in source
#define UMSBB_DEBUG_MEMORY 1
#define UMSBB_ENABLE_LOGGING 1
```

## 📚 Further Reading

- **API Reference**: See `umsbb_api.h` for complete function documentation
- **Examples**: Check `examples/` directory for usage patterns
- **Performance**: Review `comprehensive_test.c` for benchmarking
- **Web Interface**: Explore `web/dashboard.html` for monitoring

---

**Ready to customize your UMSBB core?** Start with the Python builder for easy configuration, or dive into the source code for maximum control!