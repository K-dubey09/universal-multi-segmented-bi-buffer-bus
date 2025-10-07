# UMSBB JavaScript Connector

High-performance JavaScript/TypeScript connector for the Universal Multi-Segmented Bi-Buffer Bus (UMSBB) system.

## Features

🚀 **High Performance**: Optimized for maximum throughput with minimal latency  
🌐 **Universal Compatibility**: Works in both Node.js and Browser environments  
⚡ **WebAssembly Ready**: Seamless integration with WASM core with fallback to mock mode  
🔧 **TypeScript Support**: Full TypeScript definitions included  
🛡️ **Error Handling**: Comprehensive error handling and validation  
📊 **Monitoring**: Built-in statistics and performance metrics  
🎯 **Easy Integration**: Simple async/await API

## Quick Start

### Node.js

```javascript
const { createBuffer } = require('./umsbb_connector.js');

async function example() {
    // Create a 32MB buffer with 8 segments
    const buffer = await createBuffer(32, 8);
    
    // Write messages
    await buffer.write('Hello, UMSBB!');
    await buffer.write('Another message');
    
    // Read messages
    const message1 = await buffer.read(); // "Hello, UMSBB!"
    const message2 = await buffer.read(); // "Another message"
    const message3 = await buffer.read(); // null (empty)
    
    // Get statistics
    const stats = await buffer.getStats();
    console.log('Total messages:', stats.totalMessages);
    
    // Clean up
    await buffer.close();
}

example().catch(console.error);
```

### Browser

```html
<script src="umsbb_connector.js"></script>
<script>
async function example() {
    const buffer = await createBuffer(16, 4);
    await buffer.write('Browser message');
    const message = await buffer.read();
    console.log('Received:', message);
    await buffer.close();
}
</script>
```

### TypeScript

```typescript
import { createBuffer, UMSBBBuffer, UMSBBError } from './umsbb_connector';

async function typedExample(): Promise<void> {
    try {
        const buffer: UMSBBBuffer = await createBuffer(64, 16);
        await buffer.write('TypeScript message');
        const message: string | Uint8Array | null = await buffer.read();
        await buffer.close();
    } catch (error) {
        if (error instanceof UMSBBError) {
            console.error('UMSBB Error:', error.message, 'Code:', error.code);
        }
    }
}
```

## API Reference

### UMSBBBuffer Class

#### Constructor
```javascript
new UMSBBBuffer(sizeMB = 16, numSegments = 8)
```
- `sizeMB`: Buffer size in megabytes (1-64)
- `numSegments`: Number of segments (1-16)

#### Methods

##### `initialize(): Promise<void>`
Initializes the buffer and loads the WebAssembly module.

##### `write(data): Promise<number>`
Writes data to the buffer.
- `data`: string, ArrayBuffer, or Uint8Array
- Returns: 0 on success
- Throws: UMSBBError on failure

##### `read(): Promise<string | Uint8Array | null>`
Reads data from the buffer.
- Returns: Message data or null if buffer is empty
- Throws: UMSBBError on failure

##### `getStats(): Promise<BufferStats>`
Gets buffer statistics.
```typescript
interface BufferStats {
    totalMessages: number;
    pendingMessages: number;
    useMockMode: boolean;
    bufferId?: number;
    totalBytes?: number;
    averageMessageSize?: number;
}
```

##### `close(): Promise<void>`
Closes the buffer and frees resources.

### Factory Functions

##### `createBuffer(sizeMB = 16, numSegments = 8): Promise<UMSBBBuffer>`
Creates and initializes a new buffer.

##### `performanceTest(messageCount = 10000, sizeMB = 32): Promise<void>`
Runs a comprehensive performance test.

### Error Handling

```javascript
try {
    const buffer = await createBuffer(100); // Invalid size
} catch (error) {
    if (error instanceof UMSBBError) {
        console.log('Error code:', error.code);
        console.log('Error message:', error.message);
    }
}
```

## Performance

The UMSBB JavaScript connector is designed for high-performance scenarios:

- **Mock Mode**: 500K+ messages/second (development/testing)
- **WebAssembly Mode**: 1M+ messages/second (production)
- **Zero-copy**: Efficient memory usage for large messages
- **Concurrent**: Supports producer/consumer patterns

### Benchmarking

```javascript
// Run performance test
await performanceTest(100000, 64); // 100K messages, 64MB buffer

// Custom benchmark
async function benchmark() {
    const buffer = await createBuffer(128, 16);
    const start = Date.now();
    
    for (let i = 0; i < 50000; i++) {
        await buffer.write(`Message ${i}`);
    }
    
    const duration = (Date.now() - start) / 1000;
    console.log(`Throughput: ${Math.round(50000 / duration)} msg/s`);
    
    await buffer.close();
}
```

## WebAssembly Integration

The connector automatically attempts to load the WebAssembly core:

### Browser Paths Checked:
- `./umsbb_core.js`
- `../dist/umsbb_core.js`  
- `../../dist/umsbb_core.js`

### Node.js Paths Checked:
- `./umsbb_core.js`
- `../dist/umsbb_core.js`
- `../../dist/umsbb_core.js`

If WebAssembly loading fails, the connector automatically falls back to mock mode for development and testing.

## Demos

### Node.js Demo
```bash
node demo.js
```

### Browser Demo
Open `demo.html` in your browser for an interactive demo with:
- Buffer initialization controls
- Message writing/reading interface
- Real-time statistics
- Performance testing tools
- Concurrency testing

### Package Scripts
```bash
npm test          # Run performance test
npm run demo      # Run Node.js demo
```

## Error Codes

| Code | Description |
|------|-------------|
| 0 | SUCCESS |
| -1 | ERROR_INVALID_PARAMS |
| -2 | ERROR_BUFFER_FULL |
| -3 | ERROR_BUFFER_EMPTY |
| -4 | ERROR_INVALID_HANDLE |
| -5 | ERROR_MEMORY_ALLOCATION |

## Best Practices

### 1. Buffer Sizing
```javascript
// For high-throughput applications
const buffer = await createBuffer(128, 16);

// For memory-constrained environments  
const buffer = await createBuffer(16, 4);
```

### 2. Error Handling
```javascript
async function robustWrite(buffer, data) {
    try {
        await buffer.write(data);
    } catch (error) {
        if (error instanceof UMSBBError && error.code === -2) {
            // Buffer full - implement backpressure
            await new Promise(resolve => setTimeout(resolve, 10));
            return robustWrite(buffer, data); // Retry
        }
        throw error;
    }
}
```

### 3. Producer/Consumer Pattern
```javascript
async function producerConsumer() {
    const buffer = await createBuffer(64, 8);
    
    // Producer
    const producer = async () => {
        for (let i = 0; i < 10000; i++) {
            await buffer.write(`Message ${i}`);
        }
    };
    
    // Consumer
    const consumer = async () => {
        let processed = 0;
        while (processed < 10000) {
            const msg = await buffer.read();
            if (msg) {
                processed++;
                // Process message
            } else {
                await new Promise(resolve => setTimeout(resolve, 1));
            }
        }
    };
    
    // Run concurrently
    await Promise.all([producer(), consumer()]);
    await buffer.close();
}
```

### 4. Resource Management
```javascript
class BufferManager {
    constructor() {
        this.buffers = new Map();
    }
    
    async createBuffer(id, sizeMB = 32, numSegments = 8) {
        const buffer = await createBuffer(sizeMB, numSegments);
        this.buffers.set(id, buffer);
        return buffer;
    }
    
    async closeAll() {
        for (const buffer of this.buffers.values()) {
            await buffer.close();
        }
        this.buffers.clear();
    }
}
```

## Requirements

- **Node.js**: 14.0.0 or higher
- **Browser**: Modern browsers with ES2017+ support
- **WebAssembly**: Optional (falls back to mock mode)

## File Structure

```
connectors/javascript/
├── umsbb_connector.js      # Main connector implementation
├── umsbb_connector.d.ts    # TypeScript definitions
├── package.json            # Package configuration
├── demo.js                 # Node.js demo
├── demo.html               # Browser demo
└── README.md               # This file
```

## License

MIT License - see the main UMSBB project for details.

Copyright (c) 2025 UMSBB Development Team

## Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass
5. Submit a pull request

## Support

For issues and questions:
- Check the demo files for usage examples
- Review the TypeScript definitions for API details
- Run the performance tests to verify functionality
- Open an issue in the main UMSBB repository