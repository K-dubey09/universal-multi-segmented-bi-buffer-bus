/**
 * Universal Multi-Segmented Bi-Buffer Bus WebAssembly Harness v3.0
 * Multi-Language Priority Lane Selection Demo
 */

import UniversalMultiSegmentedBiBufferBusModule from './universal_multi_segmented_bi_buffer_bus.js';

// Language enumeration matching C header
const WASM_LANGUAGES = {
    JAVASCRIPT: 0,
    PYTHON: 1,
    RUST: 2,
    GO: 3,
    CSHARP: 4,
    CPP: 5
};

// Lane types for priority selection
const LANE_TYPES = {
    EXPRESS: 0,    // Ultra-low latency
    BULK: 1,       // High throughput  
    PRIORITY: 2,   // Critical messages
    STREAMING: 3   // Continuous flows
};

class UMSBBMultiLanguageConnector {
    constructor() {
        this.module = null;
        this.bus = null;
        this.initialized = false;
        this.performanceStats = new Map();
        this.languageRouting = new Map();
    }

    async initialize(bufferSize = 1024 * 1024, segmentCount = 4) {
        try {
            console.log('🚀 Initializing UMSBB v3.0 Multi-Language System...');
            
            this.module = await UniversalMultiSegmentedBiBufferBusModule();
            this.bus = this.module._umsbb_wasm_init(bufferSize, segmentCount);
            
            if (!this.bus) {
                throw new Error('Failed to initialize UMSBB bus');
            }

            // Register JavaScript runtime
            const jsRegistered = this.module._umsbb_wasm_register_js_runtime(this.bus);
            if (jsRegistered) {
                console.log('✅ JavaScript runtime registered successfully');
            }

            // Configure priority weights for optimal routing
            const weights = new Uint32Array([100, 80, 120, 90]); // EXPRESS, BULK, PRIORITY, STREAMING
            const weightsPtr = this.module._malloc(weights.length * 4);
            this.module.HEAPU32.set(weights, weightsPtr / 4);
            
            this.module._umsbb_wasm_configure_priority_weights(this.bus, weightsPtr, weights.length);
            this.module._free(weightsPtr);

            this.initialized = true;
            console.log('🎯 Multi-language priority system initialized');
            
            return true;
        } catch (error) {
            console.error('❌ Initialization failed:', error);
            return false;
        }
    }

    /**
     * Smart lane selection based on message characteristics and language pair
     */
    selectOptimalLane(messageSize, priority, sourceLang, targetLang, latencyCritical = false) {
        if (!this.initialized) {
            throw new Error('UMSBB not initialized');
        }

        const selectedLane = this.module._umsbb_wasm_select_lane(
            messageSize, 
            priority, 
            sourceLang, 
            targetLang
        );

        const laneNames = ['EXPRESS', 'BULK', 'PRIORITY', 'STREAMING'];
        console.log(`🎯 Selected ${laneNames[selectedLane]} lane for ${this.getLangName(sourceLang)} → ${this.getLangName(targetLang)}`);
        
        return selectedLane;
    }

    /**
     * Send message with automatic language-aware routing
     */
    async sendMultiLanguage(data, sourceLang, targetLang, priority = 50, requiresAck = false) {
        if (!this.initialized) {
            throw new Error('UMSBB not initialized');
        }

        const startTime = performance.now();
        
        // Convert data to bytes
        const encoder = new TextEncoder();
        const dataBytes = typeof data === 'string' ? encoder.encode(data) : new Uint8Array(data);
        
        // Allocate WASM memory
        const dataPtr = this.module._malloc(dataBytes.length);
        this.module.HEAPU8.set(dataBytes, dataPtr);
        
        try {
            // Automatic lane selection and submission
            const messageId = this.module._umsbb_wasm_submit_multilang(
                this.bus,
                dataPtr,
                dataBytes.length,
                sourceLang,
                targetLang,
                priority
            );
            
            const endTime = performance.now();
            const latency = endTime - startTime;
            
            // Update performance statistics
            this.updatePerformanceStats(sourceLang, targetLang, latency, dataBytes.length);
            
            console.log(`📤 Sent message (ID: ${messageId}) from ${this.getLangName(sourceLang)} to ${this.getLangName(targetLang)} in ${latency.toFixed(3)}ms`);
            
            return messageId;
        } finally {
            this.module._free(dataPtr);
        }
    }

    /**
     * Receive message for specific target language
     */
    async receiveMultiLanguage(targetLang) {
        if (!this.initialized) {
            throw new Error('UMSBB not initialized');
        }

        const messagePtr = this.module._umsbb_wasm_drain_multilang(this.bus, targetLang);
        
        if (messagePtr === 0) {
            return null; // No messages available
        }

        try {
            // Extract message data (implementation depends on message structure)
            const decoder = new TextDecoder();
            // Note: In real implementation, we'd need to read the message structure
            // This is a simplified example
            console.log(`📥 Received message for ${this.getLangName(targetLang)}`);
            
            return {
                targetLang: targetLang,
                timestamp: Date.now(),
                success: true
            };
        } finally {
            // Cleanup would be handled by the WASM module
        }
    }

    /**
     * Configure routing preferences between language pairs
     */
    configureLanguageRouting(sourceLang, targetLang, preferredLane) {
        const routeKey = `${sourceLang}->${targetLang}`;
        this.languageRouting.set(routeKey, preferredLane);
        
        console.log(`🔧 Configured ${this.getLangName(sourceLang)} → ${this.getLangName(targetLang)} to prefer ${Object.keys(LANE_TYPES)[preferredLane]} lane`);
    }

    /**
     * Get performance metrics for specific language
     */
    getLanguagePerformance(lang) {
        if (!this.initialized) {
            return 0;
        }

        const performance = this.module._umsbb_wasm_get_language_performance(this.bus, lang);
        return performance;
    }

    /**
     * Get comprehensive system metrics
     */
    getSystemMetrics() {
        if (!this.initialized) {
            return null;
        }

        // Allocate memory for metrics structure
        const metricsSize = 200; // Approximate size of system_metrics struct
        const metricsPtr = this.module._malloc(metricsSize);
        
        try {
            const success = this.module._umsbb_wasm_get_metrics(this.bus, metricsPtr);
            
            if (success) {
                // Extract metrics from memory (simplified)
                const metrics = {
                    totalMessagesPerSecond: this.module.HEAPU32[metricsPtr / 4],
                    totalBytesPerSecond: this.module.HEAPU32[metricsPtr / 4 + 2],
                    avgLatencyUs: this.module.HEAPF64[metricsPtr / 8 + 1],
                    throughputMbps: this.module.HEAPF64[metricsPtr / 8 + 3],
                    systemHealthScore: this.module.HEAPF64[metricsPtr / 8 + 5]
                };
                
                return metrics;
            }
        } finally {
            this.module._free(metricsPtr);
        }
        
        return null;
    }

    /**
     * Update internal performance statistics
     */
    updatePerformanceStats(sourceLang, targetLang, latency, messageSize) {
        const key = `${sourceLang}->${targetLang}`;
        
        if (!this.performanceStats.has(key)) {
            this.performanceStats.set(key, {
                count: 0,
                totalLatency: 0,
                totalBytes: 0,
                avgLatency: 0,
                avgThroughput: 0
            });
        }
        
        const stats = this.performanceStats.get(key);
        stats.count++;
        stats.totalLatency += latency;
        stats.totalBytes += messageSize;
        stats.avgLatency = stats.totalLatency / stats.count;
        stats.avgThroughput = stats.totalBytes / stats.totalLatency * 1000; // bytes per second
        
        this.performanceStats.set(key, stats);
    }

    /**
     * Get human-readable language name
     */
    getLangName(langCode) {
        const names = ['JavaScript', 'Python', 'Rust', 'Go', 'C#', 'C++'];
        return names[langCode] || `Language${langCode}`;
    }

    /**
     * Cleanup resources
     */
    destroy() {
        if (this.bus && this.module) {
            this.module._umsbb_wasm_free(this.bus);
            this.bus = null;
        }
        this.initialized = false;
        console.log('🧹 UMSBB Multi-Language System destroyed');
    }

    /**
     * Display performance dashboard
     */
    displayPerformanceDashboard() {
        console.log('\n📊 === UMSBB v3.0 Performance Dashboard ===');
        
        // Language-specific performance
        for (const [route, stats] of this.performanceStats) {
            console.log(`\n🔄 Route: ${route}`);
            console.log(`   Messages: ${stats.count}`);
            console.log(`   Avg Latency: ${stats.avgLatency.toFixed(2)}ms`);
            console.log(`   Throughput: ${(stats.avgThroughput / 1024 / 1024).toFixed(2)} MB/s`);
        }
        
        // System metrics
        const systemMetrics = this.getSystemMetrics();
        if (systemMetrics) {
            console.log('\n🏗️ System Metrics:');
            console.log(`   Throughput: ${systemMetrics.throughputMbps.toFixed(2)} Mbps`);
            console.log(`   Avg Latency: ${systemMetrics.avgLatencyUs.toFixed(2)}μs`);
            console.log(`   Health Score: ${(systemMetrics.systemHealthScore * 100).toFixed(1)}%`);
        }
    }
}

// ============================================================================
// DEMONSTRATION OF MULTI-LANGUAGE PRIORITY ROUTING
// ============================================================================

async function demonstrateMultiLanguagePrioritySystem() {
    const connector = new UMSBBMultiLanguageConnector();
    
    if (!await connector.initialize()) {
        console.error('Failed to initialize multi-language system');
        return;
    }

    console.log('\n🎭 === Multi-Language Priority Routing Demo ===\n');

    // Configure language-specific routing preferences
    connector.configureLanguageRouting(WASM_LANGUAGES.JAVASCRIPT, WASM_LANGUAGES.PYTHON, LANE_TYPES.EXPRESS);
    connector.configureLanguageRouting(WASM_LANGUAGES.PYTHON, WASM_LANGUAGES.RUST, LANE_TYPES.BULK);
    connector.configureLanguageRouting(WASM_LANGUAGES.RUST, WASM_LANGUAGES.GO, LANE_TYPES.PRIORITY);

    // Test different message types and routing scenarios
    const testScenarios = [
        {
            name: 'Critical JavaScript → Python',
            data: '{"urgent": true, "payload": "Critical real-time data"}',
            source: WASM_LANGUAGES.JAVASCRIPT,
            target: WASM_LANGUAGES.PYTHON,
            priority: 200
        },
        {
            name: 'Large Python → Rust Transfer',
            data: 'x'.repeat(64 * 1024), // 64KB data
            source: WASM_LANGUAGES.PYTHON,
            target: WASM_LANGUAGES.RUST,
            priority: 50
        },
        {
            name: 'Rust → Go Streaming',
            data: JSON.stringify({type: 'stream', data: Array(1000).fill(42)}),
            source: WASM_LANGUAGES.RUST,
            target: WASM_LANGUAGES.GO,
            priority: 75
        },
        {
            name: 'Go → C# Analytics',
            data: '{"analytics": {"metrics": [1,2,3,4,5], "timestamp": "2025-10-08"}}',
            source: WASM_LANGUAGES.GO,
            target: WASM_LANGUAGES.CSHARP,
            priority: 100
        }
    ];

    // Execute test scenarios
    for (const scenario of testScenarios) {
        console.log(`\n🧪 Testing: ${scenario.name}`);
        
        // Show lane selection
        const selectedLane = connector.selectOptimalLane(
            scenario.data.length,
            scenario.priority,
            scenario.source,
            scenario.target
        );
        
        // Send message
        await connector.sendMultiLanguage(
            scenario.data,
            scenario.source,
            scenario.target,
            scenario.priority
        );
        
        // Simulate processing time
        await new Promise(resolve => setTimeout(resolve, 10));
        
        // Attempt to receive
        const received = await connector.receiveMultiLanguage(scenario.target);
        if (received) {
            console.log(`✅ Message received successfully`);
        }
    }

    // Display performance results
    setTimeout(() => {
        connector.displayPerformanceDashboard();
        connector.destroy();
    }, 100);
}

// Auto-run demo if this script is executed directly
if (typeof window === 'undefined' && typeof process !== 'undefined') {
    demonstrateMultiLanguagePrioritySystem()
        .then(() => console.log('\n✨ Demo completed successfully'))
        .catch(error => console.error('❌ Demo failed:', error));
}

export { UMSBBMultiLanguageConnector, WASM_LANGUAGES, LANE_TYPES, demonstrateMultiLanguagePrioritySystem };