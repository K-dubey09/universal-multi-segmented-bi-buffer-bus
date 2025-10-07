#pragma once
#include "bi_buffer.h"
#include "arena_allocator.h"
#include "capsule.h"
#include "feedback_stream.h"
#include "adaptive_batch.h"
#include "segment_ring.h"
#include "gpu_delegate.h"
#include "flow_control.h"
#include "event_scheduler.h"
#include "fast_lane.h"
#include "twin_lane.h"
#include "feedback_handshake.h"
#include "fault_tolerance.h"
#include "language_bindings.h"
#include "parallel_throughput_engine.h"
#include "atomic_compat.h"
#include <stdint.h>
#include <stdbool.h>

/*
Universal Multi-Segmented Bi-Buffer Bus Interface v3.0
Ultra-high-performance, fault-tolerant conductor for multi-language WebAssembly integration

NEW IN V3.0 - ENHANCED WEBASSEMBLY FEATURES:
🚄 Fast Lane System: Express, Bulk, Priority, and Streaming lanes for optimized throughput
🔄 Twin Lane Communication: Full-duplex bi-directional lanes with symmetric bandwidth
🤝 Feedback Handshake Protocol: Guaranteed delivery with ACK/NACK acknowledgments  
🛡️ Zero Fault Tolerance: Comprehensive fault detection, recovery, and prevention
🌐 WebAssembly Multi-Language: Direct FFI bindings for Python, JS, Rust, Go, C#
🎯 Priority Lane Selection: Intelligent routing based on language, size, and urgency
📊 Optional API: Configurable interfaces for minimal overhead or full features

Architecture:
- Multi-tier lane architecture with language-aware routing
- Priority-based lane selection with WebAssembly optimization
- Twin bi-directional communication channels with language context
- Acknowledgment-based reliable delivery across language boundaries
- Direct FFI bindings eliminating marshalling overhead
- Optional API layers for performance vs feature trade-offs
- Real-time language performance profiling and optimization
*/

// Optional API configuration flags
#ifndef UMSBB_API_LEVEL
#define UMSBB_API_LEVEL 3  // 0=minimal, 1=basic, 2=standard, 3=full
#endif

#ifndef UMSBB_ENABLE_WASM
#define UMSBB_ENABLE_WASM 1
#endif

#ifndef UMSBB_ENABLE_MULTILANG
#define UMSBB_ENABLE_MULTILANG 1
#endif

// WebAssembly language binding types
typedef enum {
    WASM_LANG_JAVASCRIPT = 0,
    WASM_LANG_PYTHON = 1,
    WASM_LANG_RUST = 2,
    WASM_LANG_GO = 3,
    WASM_LANG_CSHARP = 4,
    WASM_LANG_CPP = 5,
    WASM_LANG_COUNT = 6
} wasm_language_t;

// Priority lane selection criteria
typedef struct {
    wasm_language_t source_lang;
    wasm_language_t target_lang;
    size_t message_size;
    uint32_t priority;
    bool latency_critical;
    bool reliability_required;
    uint32_t retry_count;
    double timeout_ms;
} lane_selection_criteria_t;

// Multi-language message wrapper
typedef struct {
    void* data;
    size_t size;
    wasm_language_t source_lang;
    wasm_language_t target_lang;
    uint32_t type_id;
    uint32_t priority;
    uint64_t timestamp_us;
    uint32_t sequence_id;
    bool requires_ack;
    char language_hint[16];  // Optional language-specific metadata
} multilang_message_t;

typedef struct {
    SegmentRing ring;
    ArenaAllocator arena;
    FeedbackStream feedback;
    AdaptiveBatch batch;
    FlowControl flow;
    EventScheduler scheduler;
    
    // V3.0 Enhanced Systems
    fast_lane_manager_t fast_lanes;
    twin_lane_manager_t twin_lanes;
    handshake_manager_t handshake;
    fault_tolerance_manager_t fault_tolerance;
    
    // V3.1 Parallel Processing Engine
    parallel_engine_t parallel_engine;
    bool parallel_processing_enabled;
    uint32_t worker_thread_count;
    throughput_strategy_t current_strategy;
    
#if UMSBB_ENABLE_MULTILANG
    // Multi-language support
    language_runtime_t registered_languages[WASM_LANG_COUNT];
    uint32_t active_languages;
    uint64_t language_performance_stats[WASM_LANG_COUNT];
    
    // Priority lane routing tables
    lane_type_t lang_to_lane_map[WASM_LANG_COUNT][WASM_LANG_COUNT];
    uint32_t lane_priority_weights[LANE_COUNT];
    double lane_performance_history[LANE_COUNT];
#endif

#if UMSBB_ENABLE_WASM
    // WebAssembly runtime context
    void* wasm_context;
    bool wasm_initialized;
    uint32_t wasm_heap_size;
    void* wasm_memory_base;
#endif
    
    size_t sequence;
    uint32_t segment_count;
    bool gpu_enabled;
    double load_factor;
    uint64_t total_operations;
    
    // Performance metrics
    uint64_t messages_per_second;
    uint64_t bytes_per_second;
    double system_latency_us;
    double throughput_mbps;
    double reliability_score;
    
    // API level configuration
    uint8_t api_level;
    bool optional_features_enabled;
} UniversalMultiSegmentedBiBufferBus;

// ============================================================================
// CORE API FUNCTIONS (API Level 0+) - Always Available
// ============================================================================

UniversalMultiSegmentedBiBufferBus* umsbb_init(size_t bufCap, uint32_t segmentCount);
void umsbb_free(UniversalMultiSegmentedBiBufferBus* bus);

// Basic message operations
bool umsbb_submit_to(UniversalMultiSegmentedBiBufferBus* bus, size_t laneIndex, const char* msg, size_t size);
void* umsbb_drain_from(UniversalMultiSegmentedBiBufferBus* bus, size_t laneIndex, size_t* size);

#if UMSBB_API_LEVEL >= 1
// ============================================================================
// BASIC API FUNCTIONS (API Level 1+) - Standard Operations
// ============================================================================

void umsbb_submit(UniversalMultiSegmentedBiBufferBus* bus, const char* msg, size_t size);
void umsbb_drain(UniversalMultiSegmentedBiBufferBus* bus);
FeedbackEntry* umsbb_get_feedback(UniversalMultiSegmentedBiBufferBus* bus, size_t* count);

// Basic configuration
bool umsbb_configure_gpu(UniversalMultiSegmentedBiBufferBus* bus, bool enable);
double umsbb_get_load_factor(UniversalMultiSegmentedBiBufferBus* bus);
#endif

#if UMSBB_API_LEVEL >= 2
// ============================================================================
// STANDARD API FUNCTIONS (API Level 2+) - Enhanced Features
// ============================================================================

// Auto-scaling and optimization
uint32_t umsbb_get_optimal_segments(UniversalMultiSegmentedBiBufferBus* bus);
bool umsbb_scale_segments(UniversalMultiSegmentedBiBufferBus* bus, uint32_t newCount);

// V3.0 Fast Lane API
bool umsbb_fast_lane_submit(UniversalMultiSegmentedBiBufferBus* bus, lane_type_t lane, 
                           const void* data, size_t size, uint32_t priority);
void* umsbb_fast_lane_drain(UniversalMultiSegmentedBiBufferBus* bus, lane_type_t lane, 
                           size_t* size, uint32_t* priority);

// V3.0 Twin Lane API  
uint32_t umsbb_twin_lane_create(UniversalMultiSegmentedBiBufferBus* bus, uint32_t peer_node_id, 
                               size_t tx_capacity, size_t rx_capacity);
bool umsbb_twin_lane_send(UniversalMultiSegmentedBiBufferBus* bus, uint32_t lane_id, 
                         const void* data, size_t size, uint32_t sequence);
void* umsbb_twin_lane_receive(UniversalMultiSegmentedBiBufferBus* bus, uint32_t lane_id, 
                             size_t* size, uint32_t* sequence);
#endif

#if UMSBB_API_LEVEL >= 3
// ============================================================================
// FULL API FUNCTIONS (API Level 3+) - Complete Feature Set
// ============================================================================

// V3.0 Reliable Delivery API
uint64_t umsbb_send_reliable(UniversalMultiSegmentedBiBufferBus* bus, uint32_t producer_id, 
                            uint32_t consumer_id, const void* data, size_t size);
bool umsbb_send_feedback(UniversalMultiSegmentedBiBufferBus* bus, const feedback_message_t* feedback);
bool umsbb_process_acknowledgments(UniversalMultiSegmentedBiBufferBus* bus);

// V3.0 Fault Tolerance API
bool umsbb_is_component_healthy(UniversalMultiSegmentedBiBufferBus* bus, uint32_t component_id);
uint64_t umsbb_report_fault(UniversalMultiSegmentedBiBufferBus* bus, fault_type_t type, 
                           uint32_t component_id, const char* description);
double umsbb_get_system_health(UniversalMultiSegmentedBiBufferBus* bus);

// Performance monitoring
void umsbb_get_performance_metrics(UniversalMultiSegmentedBiBufferBus* bus, struct system_metrics* metrics);
#endif

#if UMSBB_ENABLE_MULTILANG
// ============================================================================
// MULTI-LANGUAGE WEBASSEMBLY API - Priority Lane Selection
// ============================================================================

// Initialize multi-language support
bool umsbb_init_multilang(UniversalMultiSegmentedBiBufferBus* bus);
bool umsbb_register_language(UniversalMultiSegmentedBiBufferBus* bus, wasm_language_t lang, 
                            const language_runtime_t* runtime);

// Priority-based lane selection
lane_type_t umsbb_select_priority_lane(UniversalMultiSegmentedBiBufferBus* bus, 
                                      const lane_selection_criteria_t* criteria);
bool umsbb_configure_lane_routing(UniversalMultiSegmentedBiBufferBus* bus, 
                                 wasm_language_t source, wasm_language_t target, 
                                 lane_type_t preferred_lane);

// Multi-language message operations
bool umsbb_submit_multilang(UniversalMultiSegmentedBiBufferBus* bus, 
                           const multilang_message_t* msg);
multilang_message_t* umsbb_drain_multilang(UniversalMultiSegmentedBiBufferBus* bus, 
                                          wasm_language_t target_lang);

// Language performance optimization
double umsbb_get_language_performance(UniversalMultiSegmentedBiBufferBus* bus, wasm_language_t lang);
bool umsbb_optimize_language_routing(UniversalMultiSegmentedBiBufferBus* bus);

// Direct FFI bindings (zero-copy, no marshalling)
void* umsbb_get_direct_buffer(UniversalMultiSegmentedBiBufferBus* bus, wasm_language_t lang, size_t size);
bool umsbb_submit_direct_buffer(UniversalMultiSegmentedBiBufferBus* bus, void* buffer, 
                               wasm_language_t source, wasm_language_t target);
#endif

#if UMSBB_ENABLE_WASM
// ============================================================================
// WEBASSEMBLY RUNTIME INTEGRATION
// ============================================================================

// WebAssembly context management
bool umsbb_init_wasm_context(UniversalMultiSegmentedBiBufferBus* bus, uint32_t heap_size);
void* umsbb_get_wasm_memory(UniversalMultiSegmentedBiBufferBus* bus);
bool umsbb_resize_wasm_heap(UniversalMultiSegmentedBiBufferBus* bus, uint32_t new_size);

// WASM-optimized message operations
uint32_t umsbb_wasm_submit(UniversalMultiSegmentedBiBufferBus* bus, uint32_t data_ptr, 
                          uint32_t size, wasm_language_t source_lang);
uint32_t umsbb_wasm_drain(UniversalMultiSegmentedBiBufferBus* bus, wasm_language_t target_lang);

// JavaScript interface functions (for Emscripten)
void umsbb_js_register_callback(UniversalMultiSegmentedBiBufferBus* bus, 
                               const char* callback_name, void* callback_ptr);
bool umsbb_js_set_priority_weights(UniversalMultiSegmentedBiBufferBus* bus, 
                                  uint32_t* weights, size_t count);
#endif

// ============================================================================
// INTELLIGENT LANE SELECTION ALGORITHM
// ============================================================================

// Smart lane selection based on comprehensive criteria
static inline lane_type_t umsbb_select_optimal_lane(size_t message_size, uint32_t priority, 
                                                   bool latency_critical, 
                                                   wasm_language_t source_lang, 
                                                   wasm_language_t target_lang) {
    // Express lane for critical, small messages
    if (latency_critical && message_size < 1024) {
        return LANE_EXPRESS;
    }
    
    // Bulk lane for large transfers
    if (message_size > 64 * 1024) {
        return LANE_BULK;
    }
    
    // Priority lane for high-priority messages
    if (priority > 100) {
        return LANE_PRIORITY;
    }
    
    // Language-specific optimizations
    if (source_lang == WASM_LANG_JAVASCRIPT || target_lang == WASM_LANG_JAVASCRIPT) {
        // JavaScript tends to benefit from streaming
        return LANE_STREAMING;
    }
    
    if (source_lang == WASM_LANG_RUST || target_lang == WASM_LANG_RUST) {
        // Rust benefits from express lane efficiency
        return LANE_EXPRESS;
    }
    
    // Default to streaming for steady data flows
    return LANE_STREAMING;
}

// ============================================================================
// PERFORMANCE METRICS AND DIAGNOSTICS
// ============================================================================

struct system_metrics {
    uint64_t total_messages_per_second;
    uint64_t total_bytes_per_second;
    double avg_latency_us;
    double p99_latency_us;
    double throughput_mbps;
    double reliability_score;
    double system_health_score;
    uint32_t active_lanes;
    uint32_t active_twin_lanes;
    uint32_t pending_acknowledgments;
    uint32_t active_faults;
    
#if UMSBB_ENABLE_MULTILANG
    // Multi-language specific metrics
    uint64_t language_messages[WASM_LANG_COUNT];
    double language_avg_latency[WASM_LANG_COUNT];
    uint32_t language_error_count[WASM_LANG_COUNT];
    double cross_language_efficiency;
#endif

#if UMSBB_ENABLE_WASM
    // WebAssembly specific metrics
    uint32_t wasm_heap_utilization_percent;
    uint32_t wasm_gc_collections;
    double wasm_overhead_percent;
#endif
};

// V3.1 Parallel Processing API functions
bool umsbb_enable_parallel_processing(UniversalMultiSegmentedBiBufferBus* bus, uint32_t worker_count, 
                                     throughput_strategy_t strategy);
void umsbb_disable_parallel_processing(UniversalMultiSegmentedBiBufferBus* bus);
bool umsbb_submit_parallel(UniversalMultiSegmentedBiBufferBus* bus, uint32_t lane_id,
                          const void* data, uint32_t size, uint32_t priority, uint32_t language_id);
bool umsbb_submit_batch_parallel(UniversalMultiSegmentedBiBufferBus* bus, 
                                multilang_message_t* messages, uint32_t count);
performance_profile_t umsbb_get_parallel_performance(UniversalMultiSegmentedBiBufferBus* bus);
double umsbb_get_peak_throughput_mbps(UniversalMultiSegmentedBiBufferBus* bus);
uint32_t umsbb_get_active_workers(UniversalMultiSegmentedBiBufferBus* bus);
bool umsbb_tune_parallel_performance(UniversalMultiSegmentedBiBufferBus* bus, 
                                    uint32_t new_worker_count, uint32_t new_batch_size);

// ============================================================================
// WEBASSEMBLY EXPORTED FUNCTIONS (for Emscripten builds)
// ============================================================================

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// Core functions exported to JavaScript
EMSCRIPTEN_KEEPALIVE UniversalMultiSegmentedBiBufferBus* umsbb_wasm_init(uint32_t bufCap, uint32_t segmentCount);
EMSCRIPTEN_KEEPALIVE void umsbb_wasm_free(UniversalMultiSegmentedBiBufferBus* bus);
EMSCRIPTEN_KEEPALIVE uint32_t umsbb_wasm_submit_multilang(UniversalMultiSegmentedBiBufferBus* bus, 
                                                         uint32_t data_ptr, uint32_t size,
                                                         uint32_t source_lang, uint32_t target_lang, 
                                                         uint32_t priority);
EMSCRIPTEN_KEEPALIVE uint32_t umsbb_wasm_drain_multilang(UniversalMultiSegmentedBiBufferBus* bus, 
                                                        uint32_t target_lang);
EMSCRIPTEN_KEEPALIVE uint32_t umsbb_wasm_select_lane(uint32_t message_size, uint32_t priority, 
                                                    uint32_t source_lang, uint32_t target_lang);

// Performance monitoring exports
EMSCRIPTEN_KEEPALIVE double umsbb_wasm_get_language_performance(UniversalMultiSegmentedBiBufferBus* bus, uint32_t lang);
EMSCRIPTEN_KEEPALIVE uint32_t umsbb_wasm_get_metrics(UniversalMultiSegmentedBiBufferBus* bus, uint32_t metrics_ptr);

// Language registration
EMSCRIPTEN_KEEPALIVE bool umsbb_wasm_register_js_runtime(UniversalMultiSegmentedBiBufferBus* bus);
EMSCRIPTEN_KEEPALIVE bool umsbb_wasm_configure_priority_weights(UniversalMultiSegmentedBiBufferBus* bus, 
                                                               uint32_t weights_ptr, uint32_t count);
#endif

// ============================================================================
// USAGE EXAMPLE FOR WEBASSEMBLY MULTI-LANGUAGE INTEGRATION
// ============================================================================

/*
// JavaScript usage example:
const bus = Module._umsbb_wasm_init(1024*1024, 4);

// Register languages and configure routing
Module._umsbb_wasm_register_js_runtime(bus);

// Send data from JavaScript to Python with high priority
const data = "Critical data for Python processing";
const dataPtr = Module._malloc(data.length);
Module.writeStringToMemory(data, dataPtr);

const messageId = Module._umsbb_wasm_submit_multilang(
    bus, dataPtr, data.length,
    0, 1,  // JavaScript to Python
    100    // High priority
);

// Automatic lane selection based on criteria
const selectedLane = Module._umsbb_wasm_select_lane(
    data.length, 100, 0, 1  // size, priority, JS to Python
);

// Check language-specific performance
const jsPerfScore = Module._umsbb_wasm_get_language_performance(bus, 0);
const pythonPerfScore = Module._umsbb_wasm_get_language_performance(bus, 1);

// Cleanup
Module._free(dataPtr);
Module._umsbb_wasm_free(bus);
*/

// ============================================================================
// COMPILE-TIME API VALIDATION
// ============================================================================

#if UMSBB_API_LEVEL < 0 || UMSBB_API_LEVEL > 3
#error "UMSBB_API_LEVEL must be between 0 and 3"
#endif

#if UMSBB_ENABLE_MULTILANG && UMSBB_API_LEVEL < 2
#warning "Multi-language features require API_LEVEL >= 2 for optimal performance"
#endif

#if UMSBB_ENABLE_WASM && !defined(__EMSCRIPTEN__)
#warning "WASM features enabled but not building with Emscripten"
#endif