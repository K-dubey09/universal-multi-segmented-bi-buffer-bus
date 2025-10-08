#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "atomic_compat.h"

/*
Fast Lane System - High-throughput dedicated lanes with priority routing

Features:
- Express lanes for low-latency traffic
- Bulk lanes for high-throughput transfers
- Priority queues with preemption
- Cache-line optimized structures
- NUMA-aware memory allocation
*/

typedef enum {
    LANE_EXPRESS = 0,    // Ultra-low latency, small messages
    LANE_BULK = 1,       // High throughput, large transfers
    LANE_PRIORITY = 2,   // Critical system messages
    LANE_STREAMING = 3,  // Continuous data flows
    LANE_COUNT = 4
} lane_type_t;

typedef struct {
    _Alignas(64) atomic_uint64_t head;
    _Alignas(64) atomic_uint64_t tail;
    _Alignas(64) atomic_uint32_t message_count;
    _Alignas(64) atomic_uint64_t bytes_transferred;
    
    lane_type_t type;
    uint32_t priority;
    uint32_t capacity;
    bool gpu_preferred;
    double latency_target_us;  // Target latency in microseconds
    
    void* ring_buffer;
    size_t slot_size;
    
    // Performance metrics
    uint64_t total_messages;
    uint64_t total_bytes;
    double avg_latency_us;
    double max_latency_us;
    uint64_t congestion_events;
} fast_lane_t;

typedef struct {
    fast_lane_t lanes[LANE_COUNT];
    uint32_t active_lanes;
    atomic_uint32_t global_sequence;
    
    // Load balancing
    atomic_uint32_t round_robin_counter;
    uint32_t lane_weights[LANE_COUNT];
    
    // Performance monitoring
    uint64_t total_throughput_mbps;
    double system_latency_us;
    uint32_t active_producers;
    uint32_t active_consumers;
} fast_lane_manager_t;

// Performance metrics structure (declare before API prototypes)
struct lane_metrics {
    uint64_t messages_per_second;
    uint64_t bytes_per_second;
    double avg_latency_us;
    double p99_latency_us;
    uint32_t congestion_level;
    double utilization_percent;
};

// Fast Lane API
bool fast_lane_init(fast_lane_manager_t* manager);
void fast_lane_destroy(fast_lane_manager_t* manager);

// Lane selection based on message characteristics

lane_type_t fast_lane_select_optimal(size_t message_size, uint32_t priority, bool latency_critical);

// High-performance message operations
bool fast_lane_submit(fast_lane_manager_t* manager, lane_type_t lane, const void* data, size_t size, uint32_t priority);
void* fast_lane_drain(fast_lane_manager_t* manager, lane_type_t lane, size_t* size, uint32_t* priority);

// Performance monitoring
void fast_lane_get_metrics(fast_lane_manager_t* manager, lane_type_t lane, struct lane_metrics* metrics);
double fast_lane_get_system_throughput(fast_lane_manager_t* manager);
