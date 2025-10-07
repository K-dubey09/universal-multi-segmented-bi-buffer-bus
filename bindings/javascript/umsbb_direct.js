/**
 * Universal Multi-Segmented Bi-Buffer Bus - JavaScript/Node.js Direct Binding
 * No API wrapper - Direct FFI connection with auto-scaling and GPU support
 */

const ffi = require('ffi-napi');
const ref = require('ref-napi');
const Struct = require('ref-struct-di')(ref);
const path = require('path');
const os = require('os');

// Platform-specific library loading
function getLibraryPath() {
    const platform = os.platform();
    let libName;
    
    if (platform === 'win32') {
        libName = 'universal_multi_segmented_bi_buffer_bus.dll';
    } else if (platform === 'darwin') {
        libName = 'libuniversal_multi_segmented_bi_buffer_bus.dylib';
    } else {
        libName = 'libuniversal_multi_segmented_bi_buffer_bus.so';
    }
    
    // Try common paths
    const paths = [
        path.join(__dirname, '..', '..', 'lib', libName),
        path.join(__dirname, libName),
        libName // System path
    ];
    
    for (const libPath of paths) {
        try {
            require('fs').accessSync(libPath);
            return libPath;
        } catch (e) {
            // Continue to next path
        }
    }
    
    return libName; // Fallback to system search
}

// Language types
const LanguageType = {
    C: 0,
    CPP: 1,
    PYTHON: 2,
    JAVASCRIPT: 3,
    RUST: 4,
    GO: 5,
    JAVA: 6,
    CSHARP: 7,
    KOTLIN: 8,
    SWIFT: 9
};

// Type definitions
const voidPtr = ref.refType(ref.types.void);
const uint32 = ref.types.uint32;
const size_t = ref.types.size_t;
const bool = ref.types.bool;

const UniversalData = Struct({
    data: voidPtr,
    size: size_t,
    type_id: uint32,
    source_lang: ref.types.int
});

const ScalingConfig = Struct({
    min_producers: uint32,
    max_producers: uint32,
    min_consumers: uint32,
    max_consumers: uint32,
    scale_threshold_percent: uint32,
    scale_cooldown_ms: uint32,
    gpu_preferred: bool,
    auto_balance_load: bool
});

const GPUCapabilities = Struct({
    has_cuda: bool,
    has_opencl: bool,
    has_compute: bool,
    has_memory_pool: bool,
    memory_size: size_t,
    compute_capability: ref.types.int,
    max_threads: size_t
});

// Load the native library
const lib = ffi.Library(getLibraryPath(), {
    // Core functions
    'umsbb_create_direct': [voidPtr, [size_t, uint32, ref.types.int]],
    'umsbb_submit_direct': [bool, [voidPtr, ref.refType(UniversalData)]],
    'umsbb_drain_direct': [ref.refType(UniversalData), [voidPtr, ref.types.int]],
    'umsbb_destroy_direct': [ref.types.void, [voidPtr]],
    
    // GPU functions
    'initialize_gpu': [bool, []],
    'gpu_available': [bool, []],
    'get_gpu_capabilities': [GPUCapabilities, []],
    
    // Scaling functions
    'configure_auto_scaling': [bool, [ref.refType(ScalingConfig)]],
    'get_optimal_producer_count': [uint32, []],
    'get_optimal_consumer_count': [uint32, []],
    'trigger_scale_evaluation': [ref.types.void, []],
    
    // Memory management
    'create_universal_data': [ref.refType(UniversalData), [voidPtr, size_t, uint32, ref.types.int]],
    'free_universal_data': [ref.types.void, [ref.refType(UniversalData)]]
});

/**
 * Direct Universal Bus - JavaScript Interface
 * Provides direct access to the Universal Multi-Segmented Bi-Buffer Bus
 * with auto-scaling and GPU acceleration support
 */
class DirectUniversalBus {
    /**
     * Create a new Direct Universal Bus
     * @param {number} bufferSize - Size of each buffer segment (default: 1MB)
     * @param {number} segmentCount - Number of segments (0 = auto-determine)
     * @param {boolean} gpuPreferred - Prefer GPU processing for large operations
     * @param {boolean} autoScale - Enable automatic scaling
     */
    constructor(bufferSize = 1024 * 1024, segmentCount = 0, gpuPreferred = true, autoScale = true) {
        this.bufferSize = bufferSize;
        this.segmentCount = segmentCount;
        this.closed = false;
        
        if (autoScale) {
            this._configureAutoScaling(gpuPreferred);
        }
        
        // Create bus handle
        this.handle = lib.umsbb_create_direct(bufferSize, segmentCount, LanguageType.JAVASCRIPT);
        
        if (this.handle.isNull()) {
            throw new Error('Failed to create Universal Bus');
        }
        
        console.log(`[JS Direct] Bus created with ${bufferSize} byte segments`);
        
        // Setup cleanup on process exit
        process.on('exit', () => this.close());
        process.on('SIGINT', () => this.close());
        process.on('SIGTERM', () => this.close());
    }
    
    /**
     * Configure automatic scaling parameters
     * @private
     */
    _configureAutoScaling(gpuPreferred) {
        const config = new ScalingConfig({
            min_producers: 1,
            max_producers: 16,
            min_consumers: 1,
            max_consumers: 8,
            scale_threshold_percent: 75,
            scale_cooldown_ms: 1000,
            gpu_preferred: gpuPreferred,
            auto_balance_load: true
        });
        
        lib.configure_auto_scaling(config.ref());
    }
    
    /**
     * Send data to the bus
     * @param {Buffer|string|object} data - Data to send
     * @param {number} typeId - Type identifier for routing
     * @returns {boolean} Success status
     */
    send(data, typeId = 0) {
        if (this.closed) {
            throw new Error('Bus is closed');
        }
        
        let buffer;
        
        if (typeof data === 'string') {
            buffer = Buffer.from(data, 'utf8');
        } else if (Buffer.isBuffer(data)) {
            buffer = data;
        } else {
            // Serialize objects to JSON
            buffer = Buffer.from(JSON.stringify(data), 'utf8');
        }
        
        // Create universal data structure
        const udata = lib.create_universal_data(
            buffer, 
            buffer.length, 
            typeId, 
            LanguageType.JAVASCRIPT
        );
        
        if (udata.isNull()) {
            return false;
        }
        
        const result = lib.umsbb_submit_direct(this.handle, udata);
        lib.free_universal_data(udata);
        
        return result;
    }
    
    /**
     * Receive data from the bus
     * @param {number} timeoutMs - Timeout in milliseconds (0 = non-blocking)
     * @returns {Buffer|null} Received data or null if nothing available
     */
    receive(timeoutMs = 0) {
        if (this.closed) {
            throw new Error('Bus is closed');
        }
        
        const udataPtr = lib.umsbb_drain_direct(this.handle, LanguageType.JAVASCRIPT);
        
        if (udataPtr.isNull()) {
            return null;
        }
        
        const udata = udataPtr.deref();
        
        // Copy data from C memory to JavaScript Buffer
        const result = Buffer.alloc(udata.size);
        ref.reinterpret(udata.data, udata.size).copy(result);
        
        // Free the universal data structure
        lib.free_universal_data(udataPtr);
        
        return result;
    }
    
    /**
     * Send and receive in one operation (useful for request-response patterns)
     * @param {Buffer|string|object} data - Data to send
     * @param {number} typeId - Type identifier
     * @param {number} timeoutMs - Timeout for response
     * @returns {Promise<Buffer|null>} Response data
     */
    async sendAndReceive(data, typeId = 0, timeoutMs = 5000) {
        if (!this.send(data, typeId)) {
            return null;
        }
        
        return new Promise((resolve) => {
            const startTime = Date.now();
            
            const checkForResponse = () => {
                const response = this.receive();
                if (response) {
                    resolve(response);
                } else if (Date.now() - startTime >= timeoutMs) {
                    resolve(null);
                } else {
                    setImmediate(checkForResponse);
                }
            };
            
            checkForResponse();
        });
    }
    
    /**
     * Get GPU capabilities information
     * @returns {object} GPU information
     */
    getGpuInfo() {
        try {
            const caps = lib.get_gpu_capabilities();
            return {
                available: lib.gpu_available(),
                hasCuda: caps.has_cuda,
                hasOpenCL: caps.has_opencl,
                hasCompute: caps.has_compute,
                memorySize: caps.memory_size,
                computeCapability: caps.compute_capability,
                maxThreads: caps.max_threads
            };
        } catch (e) {
            return { available: false };
        }
    }
    
    /**
     * Get current auto-scaling status
     * @returns {object} Scaling status
     */
    getScalingStatus() {
        return {
            optimalProducers: lib.get_optimal_producer_count(),
            optimalConsumers: lib.get_optimal_consumer_count(),
            gpuInfo: this.getGpuInfo()
        };
    }
    
    /**
     * Trigger manual scale evaluation
     */
    triggerScaleEvaluation() {
        lib.trigger_scale_evaluation();
    }
    
    /**
     * Close the bus and cleanup resources
     */
    close() {
        if (!this.closed && !this.handle.isNull()) {
            lib.umsbb_destroy_direct(this.handle);
            this.handle = ref.NULL;
            this.closed = true;
            console.log('[JS Direct] Bus closed');
        }
    }
}

/**
 * Auto-scaling producer-consumer system
 */
class AutoScalingBus extends DirectUniversalBus {
    constructor(options = {}) {
        super(
            options.bufferSize,
            options.segmentCount,
            options.gpuPreferred,
            options.autoScale
        );
        
        this.producers = [];
        this.consumers = [];
        this.running = false;
    }
    
    /**
     * Start auto-scaling producers
     * @param {function} producerFunc - Function that generates data
     * @param {number} count - Number of producers (null = auto-determine)
     */
    startAutoProducers(producerFunc, count = null) {
        if (count === null) {
            count = this.getScalingStatus().optimalProducers;
        }
        
        for (let i = 0; i < count; i++) {
            const producer = this._createProducer(producerFunc, i);
            this.producers.push(producer);
        }
        
        this.running = true;
        console.log(`Started ${count} auto-scaling producers`);
    }
    
    /**
     * Start auto-scaling consumers
     * @param {function} consumerFunc - Function that processes data
     * @param {number} count - Number of consumers (null = auto-determine)
     */
    startAutoConsumers(consumerFunc, count = null) {
        if (count === null) {
            count = this.getScalingStatus().optimalConsumers;
        }
        
        for (let i = 0; i < count; i++) {
            const consumer = this._createConsumer(consumerFunc, i);
            this.consumers.push(consumer);
        }
        
        this.running = true;
        console.log(`Started ${count} auto-scaling consumers`);
    }
    
    /**
     * Create a producer worker
     * @private
     */
    _createProducer(producerFunc, workerId) {
        const worker = setInterval(async () => {
            if (!this.running) return;
            
            try {
                const data = await producerFunc(workerId);
                if (data !== null && data !== undefined) {
                    this.send(data);
                }
            } catch (e) {
                console.error(`Producer ${workerId} error:`, e);
            }
        }, 1); // High frequency
        
        return worker;
    }
    
    /**
     * Create a consumer worker
     * @private
     */
    _createConsumer(consumerFunc, workerId) {
        const worker = setInterval(async () => {
            if (!this.running) return;
            
            try {
                const data = this.receive();
                if (data) {
                    await consumerFunc(data, workerId);
                }
            } catch (e) {
                console.error(`Consumer ${workerId} error:`, e);
            }
        }, 1); // High frequency
        
        return worker;
    }
    
    /**
     * Stop all producers and consumers
     */
    stop() {
        this.running = false;
        
        this.producers.forEach(clearInterval);
        this.consumers.forEach(clearInterval);
        
        this.producers = [];
        this.consumers = [];
        
        console.log('[JS AutoScale] Stopped all workers');
    }
    
    /**
     * Close the auto-scaling bus
     */
    close() {
        this.stop();
        super.close();
    }
}

// Export classes and utilities
module.exports = {
    DirectUniversalBus,
    AutoScalingBus,
    LanguageType
};

// Example usage
if (require.main === module) {
    // Direct usage example
    const bus = new DirectUniversalBus({ gpuPreferred: true });
    
    console.log('GPU Info:', bus.getGpuInfo());
    console.log('Scaling Status:', bus.getScalingStatus());
    
    // Send test data
    bus.send('Hello from JavaScript!', 1);
    bus.send(Buffer.from('Binary data'), 2);
    bus.send({ json: 'data', number: 42 }, 3);
    
    // Receive data
    let data;
    while ((data = bus.receive()) !== null) {
        console.log('Received:', data.toString());
    }
    
    bus.close();
}