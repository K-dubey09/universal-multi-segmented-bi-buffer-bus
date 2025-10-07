#pragma once
#include "atomic_compat.h"
#include <stdint.h>
#include <stdbool.h>

/*
Parallel Throughput Engine v1.0
High-performance parallel processing system for UMSBB v3.0

Features:
- Multi-threaded lane processing with work-stealing
- NUMA-aware memory allocation and thread affinity
- Lock-free ring buffers for inter-thread communication
- Dynamic load balancing across processing cores
- Batch processing optimization for throughput
- Adaptive concurrency based on system load
*/

#ifndef UMSBB_MAX_WORKER_THREADS
#define UMSBB_MAX_WORKER_THREADS 16
#endif

#ifndef UMSBB_BATCH_SIZE
#define UMSBB_BATCH_SIZE 128
#endif

#ifndef UMSBB_RING_BUFFER_SIZE
#define UMSBB_RING_BUFFER_SIZE 4096
#endif

// Thread-safe work queue entry
typedef struct {
    uint64_t message_id;
    uint32_t lane_id;
    uint32_t priority;
    uint32_t size;
    void* data;
    uint64_t timestamp;
    uint32_t language_id;
    atomic_uint32_t status; // 0=pending, 1=processing, 2=complete, 3=error
} parallel_work_item_t;

// Lock-free ring buffer for work distribution
typedef struct {
    UMSBB_ATOMIC(uint64_t) head;
    UMSBB_ATOMIC(uint64_t) tail;
    uint32_t capacity;
    uint32_t mask; // capacity - 1 (must be power of 2)
    parallel_work_item_t* items;
    char padding[64]; // Cache line alignment
} parallel_ring_buffer_t;

// Worker thread context
typedef struct {
    uint32_t thread_id;
    uint32_t cpu_core;
    UMSBB_ATOMIC(bool) active;
    UMSBB_ATOMIC(uint64_t) messages_processed;
    UMSBB_ATOMIC(uint64_t) bytes_processed;
    UMSBB_ATOMIC(uint64_t) total_latency_ns;
    parallel_ring_buffer_t* work_queue;
    void* thread_handle;
    char padding[32]; // Cache line alignment
} parallel_worker_t;

// Parallel processing engine
typedef struct {
    uint32_t num_workers;
    uint32_t num_lanes;
    parallel_worker_t workers[UMSBB_MAX_WORKER_THREADS];
    parallel_ring_buffer_t lane_queues[4]; // One per lane type
    
    // Performance metrics
    UMSBB_ATOMIC(uint64_t) total_messages;
    UMSBB_ATOMIC(uint64_t) total_bytes;
    UMSBB_ATOMIC(uint64_t) total_errors;
    UMSBB_ATOMIC(uint64_t) peak_throughput_mbps;
    
    // Load balancing
    UMSBB_ATOMIC(uint32_t) round_robin_counter;
    uint32_t load_balance_strategy; // 0=round-robin, 1=work-stealing, 2=adaptive
    
    // System configuration
    bool numa_aware;
    bool cpu_affinity_enabled;
    uint32_t batch_size;
    uint32_t prefetch_distance;
    
    char padding[64]; // Cache line alignment
} parallel_engine_t;

// Throughput optimization strategies
typedef enum {
    THROUGHPUT_STRATEGY_LATENCY_OPTIMIZED = 0,
    THROUGHPUT_STRATEGY_BANDWIDTH_OPTIMIZED = 1,
    THROUGHPUT_STRATEGY_BALANCED = 2,
    THROUGHPUT_STRATEGY_ADAPTIVE = 3
} throughput_strategy_t;

// Performance profiling data
typedef struct {
    uint64_t timestamp_start;
    uint64_t timestamp_end;
    uint32_t messages_per_second;
    uint32_t mbytes_per_second;
    double average_latency_us;
    double cpu_utilization;
    uint32_t cache_hit_rate;
    uint32_t numa_misses;
} performance_profile_t;

// API Functions
#ifdef __cplusplus
extern "C" {
#endif

// Engine lifecycle
int parallel_engine_init(parallel_engine_t* engine, uint32_t num_workers, throughput_strategy_t strategy);
int parallel_engine_start(parallel_engine_t* engine);
int parallel_engine_stop(parallel_engine_t* engine);
void parallel_engine_destroy(parallel_engine_t* engine);

// Work submission
int parallel_submit_work(parallel_engine_t* engine, uint32_t lane_id, 
                        const void* data, uint32_t size, uint32_t priority, uint32_t language_id);
int parallel_submit_batch(parallel_engine_t* engine, uint32_t lane_id,
                         parallel_work_item_t* items, uint32_t count);

// Performance monitoring
int parallel_get_performance(parallel_engine_t* engine, performance_profile_t* profile);
double parallel_get_instantaneous_throughput_mbps(parallel_engine_t* engine);
uint32_t parallel_get_queue_depth(parallel_engine_t* engine, uint32_t lane_id);

// Dynamic tuning
int parallel_adjust_workers(parallel_engine_t* engine, uint32_t new_count);
int parallel_set_strategy(parallel_engine_t* engine, throughput_strategy_t strategy);
int parallel_tune_batch_size(parallel_engine_t* engine, uint32_t new_size);

// NUMA and CPU affinity
int parallel_enable_numa_awareness(parallel_engine_t* engine);
int parallel_set_cpu_affinity(parallel_engine_t* engine, uint32_t worker_id, uint32_t cpu_core);

#ifdef __cplusplus
}
#endif

// Inline performance-critical functions
static inline bool parallel_ring_buffer_push(parallel_ring_buffer_t* ring, parallel_work_item_t* item) {
    uint64_t tail = atomic_load(&ring->tail);
    uint64_t next_tail = tail + 1;
    uint64_t head = atomic_load(&ring->head);
    
    if (next_tail - head > ring->capacity) {
        return false; // Ring buffer full
    }
    
    ring->items[tail & ring->mask] = *item;
    atomic_store(&ring->tail, next_tail);
    return true;
}

static inline bool parallel_ring_buffer_pop(parallel_ring_buffer_t* ring, parallel_work_item_t* item) {
    uint64_t head = atomic_load(&ring->head);
    uint64_t tail = atomic_load(&ring->tail);
    
    if (head == tail) {
        return false; // Ring buffer empty
    }
    
    *item = ring->items[head & ring->mask];
    atomic_store(&ring->head, head + 1);
    return true;
}

// Performance macros
#define PARALLEL_PREFETCH(addr) __builtin_prefetch((addr), 0, 3)
#define PARALLEL_LIKELY(x) __builtin_expect(!!(x), 1)
#define PARALLEL_UNLIKELY(x) __builtin_expect(!!(x), 0)

// Cache line size detection
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#define PARALLEL_CACHE_ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))