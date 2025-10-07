/*
 * Universal Multi-Segmented Bi-Buffer Bus (UMSBB) - Fast Lane Implementation
 * 
 * Copyright (c) 2025 UMSBB Development Team
 * Licensed under the MIT License - see LICENSE file for details.
 */

#include "fast_lane.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    static uint64_t get_timestamp_us() {
        LARGE_INTEGER frequency, counter;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&counter);
        return (counter.QuadPart * 1000000ULL) / frequency.QuadPart;
    }
    
    static int posix_memalign(void **memptr, size_t alignment, size_t size) {
        *memptr = _aligned_malloc(size, alignment);
        return (*memptr == NULL) ? ENOMEM : 0;
    }
    
    #define free_aligned(ptr) _aligned_free(ptr)
#else
    #include <sys/time.h>
    static uint64_t get_timestamp_us() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000000ULL + tv.tv_usec;
    }
    
    #define free_aligned(ptr) free(ptr)
#endif

bool fast_lane_init(fast_lane_manager_t* manager) {
    if (!manager) return false;
    
    memset(manager, 0, sizeof(fast_lane_manager_t));
    
    // Initialize each lane type with optimized configurations
    for (int i = 0; i < LANE_COUNT; i++) {
        fast_lane_t* lane = &manager->lanes[i];
        lane->type = (lane_type_t)i;
        
        switch (lane->type) {
            case LANE_EXPRESS:
                lane->capacity = 1024;           // Small ring for low latency
                lane->slot_size = 256;           // Small messages
                lane->priority = 3;              // Highest priority
                lane->latency_target_us = 1.0;   // 1 microsecond target
                lane->gpu_preferred = false;     // CPU for speed
                break;
                
            case LANE_BULK:
                lane->capacity = 8192;           // Large ring for throughput
                lane->slot_size = 64 * 1024;     // 64KB messages
                lane->priority = 1;              // Lower priority
                lane->latency_target_us = 100.0; // 100 microsecond target
                lane->gpu_preferred = true;      // GPU for large data
                break;
                
            case LANE_PRIORITY:
                lane->capacity = 512;            // Medium ring
                lane->slot_size = 1024;          // Medium messages
                lane->priority = 4;              // Maximum priority
                lane->latency_target_us = 0.5;   // 0.5 microsecond target
                lane->gpu_preferred = false;     // CPU for reliability
                break;
                
            case LANE_STREAMING:
                lane->capacity = 16384;          // Largest ring
                lane->slot_size = 4096;          // 4KB streaming chunks
                lane->priority = 2;              // Medium priority
                lane->latency_target_us = 50.0;  // 50 microsecond target
                lane->gpu_preferred = true;      // GPU for parallel processing
                break;
        }
        
        // Allocate cache-aligned ring buffer
        size_t ring_size = lane->capacity * lane->slot_size;
        if (posix_memalign(&lane->ring_buffer, 64, ring_size) != 0) {
            // Cleanup previously allocated lanes
            for (int j = 0; j < i; j++) {
                free_aligned(manager->lanes[j].ring_buffer);
            }
            return false;
        }
        
        memset(lane->ring_buffer, 0, ring_size);
        atomic_store(&lane->head, 0);
        atomic_store(&lane->tail, 0);
        atomic_store(&lane->message_count, 0);
        atomic_store(&lane->bytes_transferred, 0);
    }
    
    manager->active_lanes = LANE_COUNT;
    atomic_store(&manager->global_sequence, 0);
    atomic_store(&manager->round_robin_counter, 0);
    
    // Set lane weights for load balancing
    manager->lane_weights[LANE_EXPRESS] = 4;     // Highest weight
    manager->lane_weights[LANE_BULK] = 1;        // Lowest weight
    manager->lane_weights[LANE_PRIORITY] = 8;    // Maximum weight
    manager->lane_weights[LANE_STREAMING] = 2;   // Medium weight
    
    return true;
}

void fast_lane_destroy(fast_lane_manager_t* manager) {
    if (!manager) return;
    
    for (int i = 0; i < LANE_COUNT; i++) {
        if (manager->lanes[i].ring_buffer) {
            free_aligned(manager->lanes[i].ring_buffer);
        }
    }
    
    memset(manager, 0, sizeof(fast_lane_manager_t));
}

lane_type_t fast_lane_select_optimal(size_t message_size, uint32_t priority, bool latency_critical) {
    // Priority messages always go to priority lane
    if (priority >= 3) {
        return LANE_PRIORITY;
    }
    
    // Latency-critical small messages go to express lane
    if (latency_critical && message_size <= 256) {
        return LANE_EXPRESS;
    }
    
    // Large messages go to bulk lane
    if (message_size >= 4096) {
        return LANE_BULK;
    }
    
    // Continuous data flows go to streaming lane
    return LANE_STREAMING;
}

bool fast_lane_submit(fast_lane_manager_t* manager, lane_type_t lane_type, const void* data, size_t size, uint32_t priority) {
    if (!manager || !data || size == 0 || lane_type >= LANE_COUNT) return false;
    
    fast_lane_t* lane = &manager->lanes[lane_type];
    
    // Check if message fits in slot
    if (size > lane->slot_size) {
        return false;
    }
    
    uint64_t start_time = get_timestamp_us();
    
    // Get next slot with atomic increment
    uint64_t slot_index = atomic_fetch_add(&lane->head, 1) % lane->capacity;
    
    // Calculate slot address
    char* slot_addr = (char*)lane->ring_buffer + (slot_index * lane->slot_size);
    
    // Wait for slot to be available (busy wait for ultra-low latency)
    uint64_t expected_tail = atomic_load(&lane->tail);
    while ((slot_index - expected_tail) >= lane->capacity) {
        // Slot not available, check if we should yield
        if (get_timestamp_us() - start_time > lane->latency_target_us * 2) {
            return false; // Timeout
        }
        expected_tail = atomic_load(&lane->tail);
    }
    
    // Copy message data to slot
    memcpy(slot_addr, data, size);
    
    // Update metrics
    atomic_fetch_add(&lane->message_count, 1);
    atomic_fetch_add(&lane->bytes_transferred, size);
    lane->total_messages++;
    lane->total_bytes += size;
    
    // Update latency tracking
    uint64_t end_time = get_timestamp_us();
    double latency = (double)(end_time - start_time);
    
    // Simple moving average for latency
    if (lane->avg_latency_us == 0.0) {
        lane->avg_latency_us = latency;
    } else {
        lane->avg_latency_us = (lane->avg_latency_us * 0.9) + (latency * 0.1);
    }
    
    if (latency > lane->max_latency_us) {
        lane->max_latency_us = latency;
    }
    
    // Check for congestion
    if (latency > lane->latency_target_us * 2) {
        lane->congestion_events++;
    }
    
    return true;
}

void* fast_lane_drain(fast_lane_manager_t* manager, lane_type_t lane_type, size_t* size, uint32_t* priority) {
    if (!manager || !size || lane_type >= LANE_COUNT) return NULL;
    
    fast_lane_t* lane = &manager->lanes[lane_type];
    
    uint64_t current_tail = atomic_load(&lane->tail);
    uint64_t current_head = atomic_load(&lane->head);
    
    // Check if messages are available
    if (current_tail >= current_head) {
        *size = 0;
        return NULL;
    }
    
    uint64_t start_time = get_timestamp_us();
    
    // Get slot address
    uint64_t slot_index = current_tail % lane->capacity;
    char* slot_addr = (char*)lane->ring_buffer + (slot_index * lane->slot_size);
    
    // Determine message size (first 4 bytes store size)
    uint32_t message_size = *(uint32_t*)slot_addr;
    if (message_size == 0 || message_size > lane->slot_size - sizeof(uint32_t)) {
        // Invalid message size, skip this slot
        atomic_fetch_add(&lane->tail, 1);
        *size = 0;
        return NULL;
    }
    
    // Allocate return buffer
    void* result = malloc(message_size);
    if (!result) {
        *size = 0;
        return NULL;
    }
    
    // Copy message data (skip size prefix)
    memcpy(result, slot_addr + sizeof(uint32_t), message_size);
    *size = message_size;
    
    // Advance tail pointer
    atomic_fetch_add(&lane->tail, 1);
    atomic_fetch_sub(&lane->message_count, 1);
    
    // Update performance metrics
    uint64_t end_time = get_timestamp_us();
    double drain_latency = (double)(end_time - start_time);
    
    // Update system metrics
    manager->active_consumers++;
    
    return result;
}

void fast_lane_get_metrics(fast_lane_manager_t* manager, lane_type_t lane_type, struct lane_metrics* metrics) {
    if (!manager || !metrics || lane_type >= LANE_COUNT) return;
    
    fast_lane_t* lane = &manager->lanes[lane_type];
    
    // Calculate rates (simplified - real implementation would use sliding windows)
    static uint64_t last_messages = 0;
    static uint64_t last_bytes = 0;
    static uint64_t last_timestamp = 0;
    
    uint64_t current_time = get_timestamp_us();
    uint64_t time_diff = current_time - last_timestamp;
    
    if (time_diff > 0) {
        uint64_t message_diff = lane->total_messages - last_messages;
        uint64_t byte_diff = lane->total_bytes - last_bytes;
        
        metrics->messages_per_second = (message_diff * 1000000) / time_diff;
        metrics->bytes_per_second = (byte_diff * 1000000) / time_diff;
        
        last_messages = lane->total_messages;
        last_bytes = lane->total_bytes;
        last_timestamp = current_time;
    }
    
    metrics->avg_latency_us = lane->avg_latency_us;
    metrics->p99_latency_us = lane->max_latency_us * 0.99; // Simplified P99
    metrics->congestion_level = (uint32_t)(lane->congestion_events % 100);
    
    // Calculate utilization
    uint32_t current_count = atomic_load(&lane->message_count);
    metrics->utilization_percent = ((double)current_count / lane->capacity) * 100.0;
}

double fast_lane_get_system_throughput(fast_lane_manager_t* manager) {
    if (!manager) return 0.0;
    
    uint64_t total_bytes = 0;
    for (int i = 0; i < LANE_COUNT; i++) {
        total_bytes += atomic_load(&manager->lanes[i].bytes_transferred);
    }
    
    // Convert to Mbps (simplified calculation)
    return (double)total_bytes / (1024.0 * 1024.0);
}