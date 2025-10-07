#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "atomic_compat.h"

/*
Twin Lane System - Bi-directional communication with dedicated send/receive lanes

Features:
- Separate TX/RX lanes for full-duplex communication
- Symmetric bandwidth allocation
- Cross-lane coordination
- Deadlock prevention
- Flow control synchronization
*/

typedef enum {
    TWIN_DIRECTION_TX = 0,  // Transmit direction
    TWIN_DIRECTION_RX = 1,  // Receive direction
    TWIN_DIRECTION_COUNT = 2
} twin_direction_t;

typedef struct {
    _Alignas(64) atomic_uint64_t tx_head;
    _Alignas(64) atomic_uint64_t tx_tail;
    _Alignas(64) atomic_uint64_t rx_head;
    _Alignas(64) atomic_uint64_t rx_tail;
    
    // Synchronization between TX and RX
    _Alignas(64) atomic_uint64_t sync_sequence;
    _Alignas(64) atomic_uint32_t flow_control_state;
    
    uint32_t lane_id;
    uint32_t peer_node_id;
    bool symmetric_bandwidth;
    
    // Separate ring buffers for each direction
    void* tx_ring;
    void* rx_ring;
    size_t tx_capacity;
    size_t rx_capacity;
    
    // Performance metrics per direction
    uint64_t tx_messages;
    uint64_t rx_messages;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    double tx_latency_us;
    double rx_latency_us;
    
    // Flow control state
    uint32_t tx_window_size;
    uint32_t rx_window_size;
    atomic_uint32_t tx_inflight;
    atomic_uint32_t rx_inflight;
} twin_lane_t;

typedef struct {
    twin_lane_t* lanes;
    uint32_t lane_count;
    uint32_t max_lanes;
    
    // Global coordination
    atomic_uint64_t global_tx_sequence;
    atomic_uint64_t global_rx_sequence;
    
    // Load balancing across twin lanes
    atomic_uint32_t tx_round_robin;
    atomic_uint32_t rx_round_robin;
    
    // System-wide metrics
    uint64_t total_tx_throughput_mbps;
    uint64_t total_rx_throughput_mbps;
    double system_duplex_efficiency;
} twin_lane_manager_t;

// Twin Lane API
bool twin_lane_init(twin_lane_manager_t* manager, uint32_t max_lanes);
void twin_lane_destroy(twin_lane_manager_t* manager);

// Lane management
uint32_t twin_lane_create(twin_lane_manager_t* manager, uint32_t peer_node_id, size_t tx_capacity, size_t rx_capacity);
bool twin_lane_destroy_lane(twin_lane_manager_t* manager, uint32_t lane_id);

// Bi-directional communication
bool twin_lane_send(twin_lane_manager_t* manager, uint32_t lane_id, const void* data, size_t size, uint32_t sequence);
void* twin_lane_receive(twin_lane_manager_t* manager, uint32_t lane_id, size_t* size, uint32_t* sequence);

// Flow control
bool twin_lane_can_send(twin_lane_manager_t* manager, uint32_t lane_id);
void twin_lane_update_flow_control(twin_lane_manager_t* manager, uint32_t lane_id, uint32_t ack_sequence);

// Performance monitoring
void twin_lane_get_duplex_metrics(twin_lane_manager_t* manager, uint32_t lane_id, struct duplex_metrics* metrics);

struct duplex_metrics {
    uint64_t tx_messages_per_second;
    uint64_t rx_messages_per_second;
    uint64_t tx_bytes_per_second;
    uint64_t rx_bytes_per_second;
    double tx_latency_us;
    double rx_latency_us;
    double duplex_efficiency;
    uint32_t tx_congestion_level;
    uint32_t rx_congestion_level;
};