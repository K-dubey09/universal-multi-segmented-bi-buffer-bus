/**/**

 * JavaScript Connector for UMSBB WebAssembly Core * JavaScript Connector for UMSBB WebAssembly Core

 * Direct memory binding without API overhead * Direct memory binding without API overhead

 */ */



class UMSBBError extends Error {class UMSBBError extends Error {

    constructor(message) {    constructor(message) {

        super(message);        super(message);

        this.name = 'UMSBBError';        this.name = 'UMSBBError';

    }    }

}}



class UMSBBBuffer {class UMSBBBuffer {

    // Error code constants    // Error code constants

    static get SUCCESS() { return 0; }    static get SUCCESS() { return 0; }

    static get ERROR_INVALID_PARAMS() { return -1; }    static get ERROR_INVALID_PARAMS() { return -1; }

    static get ERROR_BUFFER_FULL() { return -2; }    static get ERROR_BUFFER_FULL() { return -2; }

    static get ERROR_BUFFER_EMPTY() { return -3; }    static get ERROR_BUFFER_EMPTY() { return -3; }

    static get ERROR_INVALID_HANDLE() { return -4; }    static get ERROR_INVALID_HANDLE() { return -4; }

    static get ERROR_MEMORY_ALLOCATION() { return -5; }    static get ERROR_MEMORY_ALLOCATION() { return -5; }

    static get ERROR_CORRUPTED_DATA() { return -6; }    static get ERROR_CORRUPTED_DATA() { return -6; }



    static get errorMessages() {    static get errorMessages() {

        return {        return {

            [UMSBBBuffer.ERROR_INVALID_PARAMS]: 'Invalid parameters',            [UMSBBBuffer.ERROR_INVALID_PARAMS]: 'Invalid parameters',

            [UMSBBBuffer.ERROR_BUFFER_FULL]: 'Buffer is full',            [UMSBBBuffer.ERROR_BUFFER_FULL]: 'Buffer is full',

            [UMSBBBuffer.ERROR_BUFFER_EMPTY]: 'Buffer is empty',            [UMSBBBuffer.ERROR_BUFFER_EMPTY]: 'Buffer is empty',

            [UMSBBBuffer.ERROR_INVALID_HANDLE]: 'Invalid buffer handle',            [UMSBBBuffer.ERROR_INVALID_HANDLE]: 'Invalid buffer handle',

            [UMSBBBuffer.ERROR_MEMORY_ALLOCATION]: 'Memory allocation failed',            [UMSBBBuffer.ERROR_MEMORY_ALLOCATION]: 'Memory allocation failed',

            [UMSBBBuffer.ERROR_CORRUPTED_DATA]: 'Corrupted data detected'            [UMSBBBuffer.ERROR_CORRUPTED_DATA]: 'Corrupted data detected'

        };        };

    }    }



    constructor(sizeMB = 16) {    constructor(sizeMB = 16) {

        this.sizeMB = sizeMB;        this.sizeMB = sizeMB;

        this.wasmModule = null;        this.wasmModule = null;

        this.handle = 0;        this.handle = 0;

        this.isInitialized = false;        this.isInitialized = false;

        this.lastReadMessage = null;        this.lastReadMessage = null;

                

        if (sizeMB < 1 || sizeMB > 64) {        if (sizeMB < 1 || sizeMB > 64) {

            throw new Error('Buffer size must be between 1 and 64 MB');            throw new Error('Buffer size must be between 1 and 64 MB');

        }        }

    }    }



    async initialize() {    /**

        if (this.isInitialized) {     * Initialize the WebAssembly module and create buffer

            return;     */

        }    async initialize(): Promise<void> {

        if (this.isInitialized) {

        try {            return;

            this.wasmModule = await this.loadWasmModule();        }

            this.handle = this.wasmModule._umsbb_create_buffer(this.sizeMB);

            if (this.handle === 0) {        try {

                throw new UMSBBError('Failed to create buffer');            // Load WebAssembly module

            }            this.wasmModule = await this.loadWasmModule();

            this.isInitialized = true;            

        } catch (error) {            // Create buffer

            this.createMockInterface();            this.handle = this.wasmModule._umsbb_create_buffer(this.sizeMB);

            this.isInitialized = true;            if (this.handle === 0) {

        }                throw new UMSBBError('Failed to create buffer');

    }            }



    async loadWasmModule() {            this.isInitialized = true;

        if (typeof window !== 'undefined') {        } catch (error) {

            const wasmPath = './umsbb_core.wasm';            // Fallback to mock implementation for development

            const wasmModule = await fetch(wasmPath)            this.createMockInterface();

                .then(response => response.arrayBuffer())            this.isInitialized = true;

                .then(bytes => WebAssembly.instantiate(bytes))        }

                .then(result => result.instance.exports);    }

            return wasmModule;

        } else {    /**

            const fs = require('fs');     * Load WebAssembly module

            const path = require('path');     */

            const wasmPath = path.join(__dirname, 'umsbb_core.wasm');    private async loadWasmModule(): Promise<any> {

            if (fs.existsSync(wasmPath)) {        if (typeof window !== 'undefined') {

                const wasmBuffer = fs.readFileSync(wasmPath);            // Browser environment

                const wasmModule = await WebAssembly.instantiate(wasmBuffer);            const wasmPath = './umsbb_core.wasm';

                return wasmModule.instance.exports;            const wasmModule = await fetch(wasmPath)

            }                .then(response => response.arrayBuffer())

            throw new Error('WebAssembly module not found');                .then(bytes => WebAssembly.instantiate(bytes))

        }                .then(result => result.instance.exports);

    }            

            return wasmModule;

    createMockInterface() {        } else {

        const mockData = {            // Node.js environment

            handles: new Map(),            const fs = require('fs');

            nextHandle: 1            const path = require('path');

        };            

            const wasmPath = path.join(__dirname, 'umsbb_core.wasm');

        this.wasmModule = {            if (fs.existsSync(wasmPath)) {

            _umsbb_create_buffer: (sizeMB) => {                const wasmBuffer = fs.readFileSync(wasmPath);

                const handle = mockData.nextHandle++;                const wasmModule = await WebAssembly.instantiate(wasmBuffer);

                mockData.handles.set(handle, {                return wasmModule.instance.exports;

                    sizeMB,            }

                    messages: [],            

                    totalMessages: 0,            throw new Error('WebAssembly module not found');

                    totalBytes: 0        }

                });    }

                return handle;

            },    /**

     * Create mock interface for development

            _umsbb_write_message: (handle, dataPtr, size) => {     */

                const buffer = mockData.handles.get(handle);    private createMockInterface(): void {

                if (!buffer) return UMSBBBuffer.ERROR_INVALID_HANDLE;        const mockData = {

                if (buffer.messages.length > 1000) return UMSBBBuffer.ERROR_BUFFER_FULL;            handles: new Map<number, any>(),

            nextHandle: 1

                const message = new Array(size).fill(0).map(() => Math.floor(Math.random() * 256));        };

                buffer.messages.push(message);

                buffer.totalMessages++;        this.wasmModule = {

                buffer.totalBytes += size;            _umsbb_create_buffer: (sizeMB: number): number => {

                return UMSBBBuffer.SUCCESS;                const handle = mockData.nextHandle++;

            },                mockData.handles.set(handle, {

                    sizeMB,

            _umsbb_read_message: (handle, bufferPtr, bufferSize, actualSizePtr) => {                    messages: [],

                const buffer = mockData.handles.get(handle);                    totalMessages: 0,

                if (!buffer) return UMSBBBuffer.ERROR_INVALID_HANDLE;                    totalBytes: 0

                if (buffer.messages.length === 0) return UMSBBBuffer.ERROR_BUFFER_EMPTY;                });

                return handle;

                const message = buffer.messages.shift();            },

                if (message.length > bufferSize) return UMSBBBuffer.ERROR_INVALID_PARAMS;

            _umsbb_write_message: (handle: number, dataPtr: number, size: number): number => {

                this.lastReadMessage = new Uint8Array(message);                const buffer = mockData.handles.get(handle);

                return UMSBBBuffer.SUCCESS;                if (!buffer) {

            },                    return UMSBBBuffer.ERROR_INVALID_HANDLE;

                }

            _umsbb_get_total_messages: (handle) => {

                const buffer = mockData.handles.get(handle);                if (buffer.messages.length > 1000) {

                return buffer ? buffer.totalMessages : 0;                    return UMSBBBuffer.ERROR_BUFFER_FULL;

            },                }



            _umsbb_get_total_bytes: (handle) => {                // Simulate data storage

                const buffer = mockData.handles.get(handle);                const message = new Array(size).fill(0).map(() => Math.floor(Math.random() * 256));

                return buffer ? buffer.totalBytes : 0;                buffer.messages.push(message);

            },                buffer.totalMessages++;

                buffer.totalBytes += size;

            _umsbb_get_pending_messages: (handle) => {                

                const buffer = mockData.handles.get(handle);                return UMSBBBuffer.SUCCESS;

                return buffer ? buffer.messages.length : 0;            },

            },

            _umsbb_read_message: (handle: number, bufferPtr: number, bufferSize: number, actualSizePtr: number): number => {

            _umsbb_destroy_buffer: (handle) => {                const buffer = mockData.handles.get(handle);

                if (mockData.handles.has(handle)) {                if (!buffer) {

                    mockData.handles.delete(handle);                    return UMSBBBuffer.ERROR_INVALID_HANDLE;

                    return UMSBBBuffer.SUCCESS;                }

                }

                return UMSBBBuffer.ERROR_INVALID_HANDLE;                if (buffer.messages.length === 0) {

            }                    return UMSBBBuffer.ERROR_BUFFER_EMPTY;

        };                }



        this.handle = this.wasmModule._umsbb_create_buffer(this.sizeMB);                const message = buffer.messages.shift();

    }                if (message.length > bufferSize) {

                    return UMSBBBuffer.ERROR_INVALID_PARAMS;

    async write(data) {                }

        if (!this.isInitialized) await this.initialize();

                // In real implementation, would copy to memory

        let buffer;                // For mock, we'll store the result

        if (typeof data === 'string') {                this.lastReadMessage = new Uint8Array(message);

            buffer = new TextEncoder().encode(data);                

        } else if (data instanceof ArrayBuffer) {                return UMSBBBuffer.SUCCESS;

            buffer = new Uint8Array(data);            },

        } else {

            buffer = data;            _umsbb_get_total_messages: (handle: number): number => {

        }                const buffer = mockData.handles.get(handle);

                return buffer ? buffer.totalMessages : 0;

        if (buffer.length > 65536) {            },

            throw new UMSBBError('Message too large (max 64KB)');

        }            _umsbb_get_total_bytes: (handle: number): number => {

                const buffer = mockData.handles.get(handle);

        const result = this.wasmModule._umsbb_write_message(this.handle, 0, buffer.length);                return buffer ? buffer.totalBytes : 0;

        if (result !== UMSBBBuffer.SUCCESS) {            },

            const errorMsg = UMSBBBuffer.errorMessages[result] || `Unknown error: ${result}`;

            throw new UMSBBError(`Write failed: ${errorMsg}`);            _umsbb_get_pending_messages: (handle: number): number => {

        }                const buffer = mockData.handles.get(handle);

    }                return buffer ? buffer.messages.length : 0;

            },

    async read() {

        if (!this.isInitialized) await this.initialize();            _umsbb_destroy_buffer: (handle: number): number => {

                if (mockData.handles.has(handle)) {

        const result = this.wasmModule._umsbb_read_message(this.handle, 0, 65536, 0);                    mockData.handles.delete(handle);

        if (result === UMSBBBuffer.ERROR_BUFFER_EMPTY) {                    return UMSBBBuffer.SUCCESS;

            return null;                }

        } else if (result !== UMSBBBuffer.SUCCESS) {                return UMSBBBuffer.ERROR_INVALID_HANDLE;

            const errorMsg = UMSBBBuffer.errorMessages[result] || `Unknown error: ${result}`;            }

            throw new UMSBBError(`Read failed: ${errorMsg}`);        };

        }

        return this.lastReadMessage;        this.handle = this.wasmModule._umsbb_create_buffer(this.sizeMB);

    }    }



    async getStats() {    private lastReadMessage: Uint8Array | null = null;

        if (!this.isInitialized) await this.initialize();

    /**

        return {     * Write message to buffer

            totalMessages: this.wasmModule._umsbb_get_total_messages(this.handle),     */

            totalBytes: this.wasmModule._umsbb_get_total_bytes(this.handle),    async write(data: ArrayBuffer | Uint8Array | string): Promise<void> {

            pendingMessages: this.wasmModule._umsbb_get_pending_messages(this.handle),        if (!this.isInitialized) {

            activeSegments: 0            await this.initialize();

        };        }

    }

        let buffer: Uint8Array;

    async close() {        if (typeof data === 'string') {

        if (this.isInitialized && this.handle && this.wasmModule) {            buffer = new TextEncoder().encode(data);

            this.wasmModule._umsbb_destroy_buffer(this.handle);        } else if (data instanceof ArrayBuffer) {

            this.handle = 0;            buffer = new Uint8Array(data);

            this.isInitialized = false;        } else {

        }            buffer = data;

    }        }

}

        if (buffer.length > 65536) { // 64KB max

async function createBuffer(sizeMB = 16) {            throw new UMSBBError('Message too large (max 64KB)');

    const buffer = new UMSBBBuffer(sizeMB);        }

    await buffer.initialize();

    return buffer;        // In real WebAssembly, we'd allocate memory and copy data

}        // For mock, we simulate the call

        const result = this.wasmModule._umsbb_write_message(this.handle, 0, buffer.length);

async function performanceTest(messageCount = 10000) {

    console.log('UMSBB JavaScript Connector Test');        if (result !== UMSBBBuffer.SUCCESS) {

    console.log('='.repeat(40));            const errorMsg = UMSBBBuffer.errorMessages[result] || `Unknown error: ${result}`;

            throw new UMSBBError(`Write failed: ${errorMsg}`);

    const buffer = await createBuffer(32);        }

    const startTime = Date.now();    }



    const produceMessages = async () => {    /**

        for (let i = 0; i < messageCount; i++) {     * Read message from buffer

            await buffer.write(`Message ${i}`);     */

            if (i % 1000 === 0) console.log(`Produced ${i} messages`);    async read(): Promise<Uint8Array | null> {

        }        if (!this.isInitialized) {

    };            await this.initialize();

        }

    const consumeMessages = async () => {

        let received = 0;        const result = this.wasmModule._umsbb_read_message(this.handle, 0, 65536, 0);

        while (received < messageCount) {

            const message = await buffer.read();        if (result === UMSBBBuffer.ERROR_BUFFER_EMPTY) {

            if (message) {            return null;

                received++;        } else if (result !== UMSBBBuffer.SUCCESS) {

                if (received % 1000 === 0) console.log(`Consumed ${received} messages`);            const errorMsg = UMSBBBuffer.errorMessages[result] || `Unknown error: ${result}`;

            } else {            throw new UMSBBError(`Read failed: ${errorMsg}`);

                await new Promise(resolve => setTimeout(resolve, 1));        }

            }

        }        // In real implementation, would read from WebAssembly memory

    };        // For mock, return the stored result

        return this.lastReadMessage;

    await Promise.all([produceMessages(), consumeMessages()]);    }



    const duration = (Date.now() - startTime) / 1000;    /**

    const stats = await buffer.getStats();     * Get buffer statistics

     */

    console.log('\nTest Results:');    async getStats(): Promise<UMSBBStats> {

    console.log(`Duration: ${duration.toFixed(3)} seconds`);        if (!this.isInitialized) {

    console.log(`Messages: ${stats.totalMessages}`);            await this.initialize();

    console.log(`Bytes: ${stats.totalBytes}`);        }

    console.log(`Messages/sec: ${Math.round(stats.totalMessages / duration)}`);

    console.log(`MB/sec: ${(stats.totalBytes / (1024 * 1024) / duration).toFixed(2)}`);        const totalMessages = this.wasmModule._umsbb_get_total_messages(this.handle);

        const totalBytes = this.wasmModule._umsbb_get_total_bytes(this.handle);

    await buffer.close();        const pendingMessages = this.wasmModule._umsbb_get_pending_messages(this.handle);

}

        return {

// Module exports            totalMessages,

if (typeof module !== 'undefined' && module.exports) {            totalBytes,

    module.exports = { UMSBBBuffer, UMSBBError, createBuffer, performanceTest };            pendingMessages,

} else if (typeof window !== 'undefined') {            activeSegments: 0 // TODO: Implement in core

    window.UMSBBBuffer = UMSBBBuffer;        };

    window.UMSBBError = UMSBBError;    }

    window.createBuffer = createBuffer;

    window.performanceTest = performanceTest;    /**

}     * Close buffer and free resources

     */

// Example usage    async close(): Promise<void> {

if (typeof require !== 'undefined' && require.main === module) {        if (this.isInitialized && this.handle && this.wasmModule) {

    performanceTest(5000).catch(console.error);            this.wasmModule._umsbb_destroy_buffer(this.handle);

}            this.handle = 0;
            this.isInitialized = false;
        }
    }
}

/**
 * Convenience function to create a new buffer
 */
export async function createBuffer(sizeMB: number = 16): Promise<UMSBBBuffer> {
    const buffer = new UMSBBBuffer(sizeMB);
    await buffer.initialize();
    return buffer;
}

/**
 * Performance test utility
 */
export async function performanceTest(messageCount: number = 10000): Promise<void> {
    console.log('UMSBB JavaScript Connector Test');
    console.log('='.repeat(40));

    const buffer = await createBuffer(32); // 32MB buffer

    const startTime = Date.now();

    // Producer
    const produceMessages = async (): Promise<void> => {
        for (let i = 0; i < messageCount; i++) {
            const message = `Message ${i}`;
            await buffer.write(message);
            
            if (i % 1000 === 0) {
                console.log(`Produced ${i} messages`);
            }
        }
    };

    // Consumer
    const consumeMessages = async (): Promise<void> => {
        let received = 0;
        while (received < messageCount) {
            const message = await buffer.read();
            if (message) {
                received++;
                if (received % 1000 === 0) {
                    console.log(`Consumed ${received} messages`);
                }
            } else {
                await new Promise(resolve => setTimeout(resolve, 1)); // Small delay
            }
        }
    };

    // Run producer and consumer concurrently
    await Promise.all([
        produceMessages(),
        consumeMessages()
    ]);

    const endTime = Date.now();
    const duration = (endTime - startTime) / 1000;

    const stats = await buffer.getStats();

    console.log('\nTest Results:');
    console.log(`Duration: ${duration.toFixed(3)} seconds`);
    console.log(`Messages: ${stats.totalMessages}`);
    console.log(`Bytes: ${stats.totalBytes}`);
    console.log(`Messages/sec: ${Math.round(stats.totalMessages / duration)}`);
    console.log(`MB/sec: ${(stats.totalBytes / (1024 * 1024) / duration).toFixed(2)}`);

    await buffer.close();
}

// Export everything
export { UMSBBBuffer, UMSBBError };
export type { UMSBBStats };

// Node.js module exports for CommonJS compatibility
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        UMSBBBuffer,
        UMSBBError,
        createBuffer,
        performanceTest
    };
}