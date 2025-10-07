#include "twin_lane.h"
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

bool twin_lane_init(twin_lane_manager_t* manager, uint32_t max_lanes) {
    if (!manager || max_lanes == 0) return false;
    
    memset(manager, 0, sizeof(twin_lane_manager_t));
    
    manager->lanes = calloc(max_lanes, sizeof(twin_lane_t));
    if (!manager->lanes) return false;
    
    manager->max_lanes = max_lanes;
    manager->lane_count = 0;
    
    atomic_store(&manager->global_tx_sequence, 0);
    atomic_store(&manager->global_rx_sequence, 0);
    atomic_store(&manager->tx_round_robin, 0);
    atomic_store(&manager->rx_round_robin, 0);
    
    return true;
}

void twin_lane_destroy(twin_lane_manager_t* manager) {
    if (!manager) return;
    
    if (manager->lanes) {
        for (uint32_t i = 0; i < manager->lane_count; i++) {
            twin_lane_t* lane = &manager->lanes[i];
            if (lane->tx_ring) free_aligned(lane->tx_ring);
            if (lane->rx_ring) free_aligned(lane->rx_ring);
        }
        free(manager->lanes);
    }
    
    memset(manager, 0, sizeof(twin_lane_manager_t));
}

uint32_t twin_lane_create(twin_lane_manager_t* manager, uint32_t peer_node_id, size_t tx_capacity, size_t rx_capacity) {
    if (!manager || manager->lane_count >= manager->max_lanes) return UINT32_MAX;
    
    uint32_t lane_id = manager->lane_count++;
    twin_lane_t* lane = &manager->lanes[lane_id];
    
    memset(lane, 0, sizeof(twin_lane_t));
    
    lane->lane_id = lane_id;
    lane->peer_node_id = peer_node_id;
    lane->symmetric_bandwidth = (tx_capacity == rx_capacity);
    lane->tx_capacity = tx_capacity;
    lane->rx_capacity = rx_capacity;
    
    // Allocate cache-aligned ring buffers
    if (posix_memalign(&lane->tx_ring, 64, tx_capacity) != 0) {
        return UINT32_MAX;
    }
    
    if (posix_memalign(&lane->rx_ring, 64, rx_capacity) != 0) {
        free_aligned(lane->tx_ring);
        return UINT32_MAX;
    }
    
    memset(lane->tx_ring, 0, tx_capacity);
    memset(lane->rx_ring, 0, rx_capacity);
    
    atomic_store(&lane->tx_head, 0);
    atomic_store(&lane->tx_tail, 0);
    atomic_store(&lane->rx_head, 0);
    atomic_store(&lane->rx_tail, 0);
    atomic_store(&lane->sync_sequence, 0);
    atomic_store(&lane->flow_control_state, 0);
    
    // Initialize flow control windows
    lane->tx_window_size = 64;  // Start with 64 outstanding messages
    lane->rx_window_size = 64;
    atomic_store(&lane->tx_inflight, 0);
    atomic_store(&lane->rx_inflight, 0);
    
    return lane_id;
}

bool twin_lane_destroy_lane(twin_lane_manager_t* manager, uint32_t lane_id) {
    if (!manager || lane_id >= manager->lane_count) return false;
    
    twin_lane_t* lane = &manager->lanes[lane_id];
    
    if (lane->tx_ring) {
        free_aligned(lane->tx_ring);
        lane->tx_ring = NULL;
    }
    
    if (lane->rx_ring) {
        free_aligned(lane->rx_ring);
        lane->rx_ring = NULL;
    }
    
    // Mark lane as inactive by setting capacity to 0
    lane->tx_capacity = 0;
    lane->rx_capacity = 0;
    
    return true;
}

bool twin_lane_send(twin_lane_manager_t* manager, uint32_t lane_id, const void* data, size_t size, uint32_t sequence) {
    if (!manager || lane_id >= manager->lane_count || !data || size == 0) return false;
    
    twin_lane_t* lane = &manager->lanes[lane_id];
    if (lane->tx_capacity == 0) return false; // Lane destroyed
    
    uint64_t start_time = get_timestamp_us();
    
    // Check flow control
    uint32_t inflight = atomic_load(&lane->tx_inflight);
    if (inflight >= lane->tx_window_size) {
        return false; // Flow control limit reached
    }
    
    // Get TX slot
    uint64_t tx_head = atomic_fetch_add(&lane->tx_head, 1);
    uint64_t slot_offset = (tx_head % (lane->tx_capacity / (size + sizeof(uint64_t) + sizeof(uint32_t)))) * 
                          (size + sizeof(uint64_t) + sizeof(uint32_t));
    
    if (slot_offset + size + sizeof(uint64_t) + sizeof(uint32_t) > lane->tx_capacity) {
        return false; // Not enough space
    }
    
    char* slot_addr = (char*)lane->tx_ring + slot_offset;
    
    // Write message with header: [sequence][size][data]
    *(uint64_t*)slot_addr = sequence;
    *(uint32_t*)(slot_addr + sizeof(uint64_t)) = (uint32_t)size;
    memcpy(slot_addr + sizeof(uint64_t) + sizeof(uint32_t), data, size);
    
    // Update metrics
    lane->tx_messages++;
    lane->tx_bytes += size;
    atomic_fetch_add(&lane->tx_inflight, 1);
    
    // Update latency tracking
    uint64_t end_time = get_timestamp_us();
    double latency = (double)(end_time - start_time);
    lane->tx_latency_us = (lane->tx_latency_us * 0.9) + (latency * 0.1);
    
    // Update global sequence for coordination
    atomic_store(&manager->global_tx_sequence, sequence);
    
    return true;
}

void* twin_lane_receive(twin_lane_manager_t* manager, uint32_t lane_id, size_t* size, uint32_t* sequence) {
    if (!manager || lane_id >= manager->lane_count || !size || !sequence) return NULL;
    
    twin_lane_t* lane = &manager->lanes[lane_id];
    if (lane->rx_capacity == 0) return NULL; // Lane destroyed
    
    uint64_t start_time = get_timestamp_us();
    
    uint64_t rx_tail = atomic_load(&lane->rx_tail);
    uint64_t rx_head = atomic_load(&lane->rx_head);
    
    // Check if messages available
    if (rx_tail >= rx_head) {
        *size = 0;
        *sequence = 0;
        return NULL;
    }
    
    // Calculate slot offset (simplified - assumes fixed slot size)
    uint64_t max_slot_size = 4096; // Maximum message size
    uint64_t slot_offset = (rx_tail % (lane->rx_capacity / max_slot_size)) * max_slot_size;
    
    char* slot_addr = (char*)lane->rx_ring + slot_offset;
    
    // Read message header: [sequence][size][data]
    uint64_t msg_sequence = *(uint64_t*)slot_addr;
    uint32_t msg_size = *(uint32_t*)(slot_addr + sizeof(uint64_t));
    
    if (msg_size == 0 || msg_size > max_slot_size - sizeof(uint64_t) - sizeof(uint32_t)) {
        // Invalid message, advance tail
        atomic_fetch_add(&lane->rx_tail, 1);
        *size = 0;
        *sequence = 0;
        return NULL;
    }
    
    // Allocate result buffer
    void* result = malloc(msg_size);
    if (!result) {
        *size = 0;
        *sequence = 0;
        return NULL;
    }
    
    // Copy message data
    memcpy(result, slot_addr + sizeof(uint64_t) + sizeof(uint32_t), msg_size);
    *size = msg_size;
    *sequence = (uint32_t)msg_sequence;
    
    // Advance RX tail
    atomic_fetch_add(&lane->rx_tail, 1);
    atomic_fetch_add(&lane->rx_inflight, 1);
    
    // Update metrics
    lane->rx_messages++;
    lane->rx_bytes += msg_size;
    
    // Update latency tracking
    uint64_t end_time = get_timestamp_us();
    double latency = (double)(end_time - start_time);
    lane->rx_latency_us = (lane->rx_latency_us * 0.9) + (latency * 0.1);
    
    return result;
}

bool twin_lane_can_send(twin_lane_manager_t* manager, uint32_t lane_id) {
    if (!manager || lane_id >= manager->lane_count) return false;
    
    twin_lane_t* lane = &manager->lanes[lane_id];
    uint32_t inflight = atomic_load(&lane->tx_inflight);
    
    return inflight < lane->tx_window_size;
}

void twin_lane_update_flow_control(twin_lane_manager_t* manager, uint32_t lane_id, uint32_t ack_sequence) {
    if (!manager || lane_id >= manager->lane_count) return;
    
    twin_lane_t* lane = &manager->lanes[lane_id];
    
    // Decrease inflight count for acknowledged message
    uint32_t current_inflight = atomic_load(&lane->tx_inflight);
    if (current_inflight > 0) {
        atomic_fetch_sub(&lane->tx_inflight, 1);
    }
    
    // Adaptive window sizing based on network conditions
    // Increase window if ACKs are coming back quickly
    uint64_t current_time = get_timestamp_us();
    static uint64_t last_ack_time = 0;
    
    if (last_ack_time > 0) {
        uint64_t ack_interval = current_time - last_ack_time;
        
        if (ack_interval < 1000 && lane->tx_window_size < 256) { // < 1ms
            lane->tx_window_size += 2; // Increase window
        } else if (ack_interval > 10000 && lane->tx_window_size > 8) { // > 10ms
            lane->tx_window_size -= 1; // Decrease window
        }
    }
    
    last_ack_time = current_time;
    
    // Update sync sequence for coordination
    atomic_store(&lane->sync_sequence, ack_sequence);
}

void twin_lane_get_duplex_metrics(twin_lane_manager_t* manager, uint32_t lane_id, struct duplex_metrics* metrics) {
    if (!manager || lane_id >= manager->lane_count || !metrics) return;
    
    twin_lane_t* lane = &manager->lanes[lane_id];
    
    // Calculate rates (simplified - using static variables for demo)
    static uint64_t last_tx_messages = 0;
    static uint64_t last_rx_messages = 0;
    static uint64_t last_tx_bytes = 0;
    static uint64_t last_rx_bytes = 0;
    static uint64_t last_timestamp = 0;
    
    uint64_t current_time = get_timestamp_us();
    uint64_t time_diff = current_time - last_timestamp;
    
    if (time_diff > 0) {
        uint64_t tx_msg_diff = lane->tx_messages - last_tx_messages;
        uint64_t rx_msg_diff = lane->rx_messages - last_rx_messages;
        uint64_t tx_byte_diff = lane->tx_bytes - last_tx_bytes;
        uint64_t rx_byte_diff = lane->rx_bytes - last_rx_bytes;
        
        metrics->tx_messages_per_second = (tx_msg_diff * 1000000) / time_diff;
        metrics->rx_messages_per_second = (rx_msg_diff * 1000000) / time_diff;
        metrics->tx_bytes_per_second = (tx_byte_diff * 1000000) / time_diff;
        metrics->rx_bytes_per_second = (rx_byte_diff * 1000000) / time_diff;
        
        last_tx_messages = lane->tx_messages;
        last_rx_messages = lane->rx_messages;
        last_tx_bytes = lane->tx_bytes;
        last_rx_bytes = lane->rx_bytes;
        last_timestamp = current_time;
    }
    
    metrics->tx_latency_us = lane->tx_latency_us;
    metrics->rx_latency_us = lane->rx_latency_us;
    
    // Calculate duplex efficiency
    double total_bandwidth = metrics->tx_bytes_per_second + metrics->rx_bytes_per_second;
    double theoretical_max = (double)(lane->tx_capacity + lane->rx_capacity);
    metrics->duplex_efficiency = (total_bandwidth / theoretical_max) * 100.0;
    
    // Calculate congestion levels based on flow control state
    uint32_t tx_inflight = atomic_load(&lane->tx_inflight);
    uint32_t rx_inflight = atomic_load(&lane->rx_inflight);
    
    metrics->tx_congestion_level = (tx_inflight * 100) / lane->tx_window_size;
    metrics->rx_congestion_level = (rx_inflight * 100) / lane->rx_window_size;
}