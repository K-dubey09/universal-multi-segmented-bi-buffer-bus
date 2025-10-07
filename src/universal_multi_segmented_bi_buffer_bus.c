#include "universal_multi_segmented_bi_buffer_bus.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

UniversalMultiSegmentedBiBufferBus* umsbb_init(size_t bufCap, uint32_t segmentCount) {
    UniversalMultiSegmentedBiBufferBus* bus = malloc(sizeof(UniversalMultiSegmentedBiBufferBus));
    if (!bus) return NULL;
    
    // Auto-determine segment count if not specified
    if (segmentCount == 0) {
        segmentCount = 4; // Default
    }
    
    segment_ring_init(&bus->ring, segmentCount, bufCap);
    arena_init(&bus->arena, bufCap * segmentCount);
    feedback_clear(&bus->feedback);
    batch_init(&bus->batch, 1, 8, 1);
    flow_control_init(&bus->flow, bufCap * 0.8);
    event_init(&bus->scheduler);
    
    // Initialize V3.0 enhanced systems
    if (!fast_lane_init(&bus->fast_lanes)) {
        free(bus);
        return NULL;
    }
    
    if (!twin_lane_init(&bus->twin_lanes, 32)) { // Support up to 32 twin lanes
        fast_lane_destroy(&bus->fast_lanes);
        free(bus);
        return NULL;
    }
    
    if (!handshake_init(&bus->handshake, 1024)) { // Support 1024 pending messages
        fast_lane_destroy(&bus->fast_lanes);
        twin_lane_destroy(&bus->twin_lanes);
        free(bus);
        return NULL;
    }
    
    if (!fault_tolerance_init(&bus->fault_tolerance, 512)) { // Track 512 fault records
        fast_lane_destroy(&bus->fast_lanes);
        twin_lane_destroy(&bus->twin_lanes);
        handshake_destroy(&bus->handshake);
        free(bus);
        return NULL;
    }
    
    bus->sequence = 0;
    bus->segment_count = segmentCount;
    bus->gpu_enabled = false;
    bus->load_factor = 0.0;
    bus->total_operations = 0;
    
    // Initialize performance metrics
    bus->messages_per_second = 0;
    bus->bytes_per_second = 0;
    bus->system_latency_us = 0.0;
    bus->throughput_mbps = 0.0;
    bus->reliability_score = 1.0;
    
    // Initialize GPU if available
    if (gpu_available()) {
        bus->gpu_enabled = initialize_gpu();
    }
    
    return bus;
}

void umsbb_submit(UniversalMultiSegmentedBiBufferBus* bus, const char* msg, size_t size) {
    size_t lane = bus->ring.currentIndex;
    umsbb_submit_to(bus, lane, msg, size);
    bus->ring.currentIndex = (lane + 1) % bus->ring.activeCount;
}

bool umsbb_submit_to(UniversalMultiSegmentedBiBufferBus* bus, size_t laneIndex, const char* msg, size_t size) {
    if (laneIndex >= bus->ring.activeCount) return false;
    BiBuffer* target = &bus->ring.buffers[laneIndex];

    if (flow_should_throttle(target, &bus->flow)) {
        FeedbackEntry fb = {
            .sequence = bus->sequence,
            .type = FEEDBACK_THROTTLED,
            .note = "High-water mark reached",
            .timestamp = (uint64_t)time(NULL)
        };
        feedback_push(&bus->feedback, fb);
        return false;
    }

    // Try GPU acceleration for large messages
    if (bus->gpu_enabled && size > 1024 * 1024) {
        if (try_gpu_execute((void*)msg, size)) {
            bus->total_operations++;
            return true;
        }
    }

    // Check adaptive batching - accumulate multiple messages if beneficial
    size_t batchSize = batch_next(&bus->batch, 85); // Assume 85% success rate for now
    
    MessageCapsule* cap = arena_alloc(&bus->arena, sizeof(MessageCapsule));
    if (!cap) {
        FeedbackEntry fb = {
            .sequence = bus->sequence,
            .type = FEEDBACK_SKIPPED,
            .note = "Arena allocation failed",
            .timestamp = (uint64_t)time(NULL)
        };
        feedback_push(&bus->feedback, fb);
        return false;
    }
    
    capsule_wrap(cap, bus->sequence++, msg, size);

    void* ptr = bi_buffer_claim(target, sizeof(MessageCapsule));
    if (ptr) {
        memcpy(ptr, cap, sizeof(MessageCapsule));
        bi_buffer_commit(target, ptr, sizeof(MessageCapsule));
        event_signal(&bus->scheduler);
        
        // Track successful submission for batching feedback
        FeedbackEntry fb = {
            .sequence = cap->header.sequence,
            .type = FEEDBACK_OK,
            .note = "Message submitted successfully",
            .timestamp = (uint64_t)time(NULL)
        };
        feedback_push(&bus->feedback, fb);
        
        bus->total_operations++;
        bus->load_factor = (double)bus->total_operations / (bus->segment_count * 1000.0);
        return true;
    } else {
        FeedbackEntry fb = {
            .sequence = cap->header.sequence,
            .type = FEEDBACK_SKIPPED,
            .note = "Buffer claim failed",
            .timestamp = (uint64_t)time(NULL)
        };
        feedback_push(&bus->feedback, fb);
        return false;
    }
}

void umsbb_drain(UniversalMultiSegmentedBiBufferBus* bus) {
    size_t lane = bus->ring.currentIndex;
    umsbb_drain_from(bus, lane);
    bus->ring.currentIndex = (lane + 1) % bus->ring.activeCount;
}

void* umsbb_drain_from(UniversalMultiSegmentedBiBufferBus* bus, size_t laneIndex, size_t* dataSize) {
    *dataSize = 0;
    if (laneIndex >= bus->ring.activeCount) return NULL;
    BiBuffer* buf = &bus->ring.buffers[laneIndex];

    size_t size;
    void* ptr = bi_buffer_read(buf, &size);
    if (!ptr) {
        FeedbackEntry fb = {
            .sequence = bus->sequence,
            .type = FEEDBACK_IDLE,
            .note = "No data to drain",
            .timestamp = (uint64_t)time(NULL)
        };
        feedback_push(&bus->feedback, fb);
        return NULL;
    }

    MessageCapsule* cap = (MessageCapsule*)ptr;
    FeedbackEntry fb = {
        .sequence = cap->header.sequence,
        .timestamp = (uint64_t)time(NULL),
        .note = "",
        .type = FEEDBACK_OK
    };

    if (!capsule_validate(cap)) {
        fb.type = FEEDBACK_CORRUPTED;
        fb.note = "Checksum mismatch - state machine integrity failure";
        feedback_push(&bus->feedback, fb);
        bi_buffer_release(buf);
        return NULL;
    }

    // Allocate memory for the payload data
    void* result = malloc(cap->size);
    if (!result) {
        fb.type = FEEDBACK_SKIPPED;
        fb.note = "Memory allocation failed";
        feedback_push(&bus->feedback, fb);
        bi_buffer_release(buf);
        return NULL;
    }

    // Copy payload data
    memcpy(result, cap->payload, cap->size);
    *dataSize = cap->size;

    if (bus->gpu_enabled && try_gpu_execute(result, cap->size)) {
        fb.type = FEEDBACK_GPU_EXECUTED;
        fb.note = "GPU acceleration successful";
        batch_next(&bus->batch, 95); // High success rate for GPU
    } else {
        fb.type = FEEDBACK_CPU_EXECUTED;
        fb.note = "CPU fallback execution";
        batch_next(&bus->batch, 75); // Lower success rate for CPU
    }

    feedback_push(&bus->feedback, fb);
    bi_buffer_release(buf); // This transitions through FEEDBACK → FREE
    
    bus->total_operations++;
    
    // Only clear event if no more data is available across all buffers
    bool hasMoreData = false;
    for (size_t i = 0; i < bus->ring.activeCount; ++i) {
        size_t testSize;
        if (bi_buffer_read(&bus->ring.buffers[i], &testSize)) {
            hasMoreData = true;
            break;
        }
    }
    if (!hasMoreData) {
        event_clear(&bus->scheduler);
    }
    
    return result;
}

// Legacy drain_from function for compatibility
void umsbb_drain_from_legacy(UniversalMultiSegmentedBiBufferBus* bus, size_t laneIndex) {
    size_t size;
    void* data = umsbb_drain_from(bus, laneIndex, &size);
    if (data) {
        free(data); // Just discard the data for legacy compatibility
    }
}

// Enhanced API functions
bool umsbb_configure_gpu(UniversalMultiSegmentedBiBufferBus* bus, bool enable) {
    if (!bus) return false;
    
    if (enable && gpu_available()) {
        bus->gpu_enabled = initialize_gpu();
        return bus->gpu_enabled;
    } else {
        bus->gpu_enabled = false;
        return true;
    }
}

double umsbb_get_load_factor(UniversalMultiSegmentedBiBufferBus* bus) {
    if (!bus) return 0.0;
    return bus->load_factor;
}

uint32_t umsbb_get_optimal_segments(UniversalMultiSegmentedBiBufferBus* bus) {
    if (!bus) return 4;
    
    // Calculate optimal segments based on load factor
    double load = bus->load_factor;
    if (load > 0.8) {
        return bus->segment_count * 2; // Scale up
    } else if (load < 0.3 && bus->segment_count > 2) {
        return bus->segment_count / 2; // Scale down
    }
    
    return bus->segment_count; // No change needed
}

bool umsbb_scale_segments(UniversalMultiSegmentedBiBufferBus* bus, uint32_t newCount) {
    if (!bus || newCount == 0 || newCount == bus->segment_count) return false;
    
    // For now, this is a placeholder - full implementation would require
    // careful migration of existing data
    bus->segment_count = newCount;
    return true;
}

FeedbackEntry* umsbb_get_feedback(UniversalMultiSegmentedBiBufferBus* bus, size_t* count) {
    *count = bus->feedback.count;
    return bus->feedback.entries;
}

// V3.0 Fast Lane API implementations
bool umsbb_fast_lane_submit(UniversalMultiSegmentedBiBufferBus* bus, lane_type_t lane, 
                           const void* data, size_t size, uint32_t priority) {
    if (!bus) return false;
    
    bool success = fast_lane_submit(&bus->fast_lanes, lane, data, size, priority);
    
    if (success) {
        bus->total_operations++;
        
        // Update performance metrics
        bus->messages_per_second++;
        bus->bytes_per_second += size;
        
        // Report success to fault tolerance system
        fault_tolerance_update_component_health(&bus->fault_tolerance, (uint32_t)lane, true, 0);
    } else {
        // Report failure to fault tolerance system
        fault_tolerance_report_fault(&bus->fault_tolerance, FAULT_TYPE_OVERFLOW, 
                                   (uint32_t)lane, "Fast lane submit failed");
    }
    
    return success;
}

void* umsbb_fast_lane_drain(UniversalMultiSegmentedBiBufferBus* bus, lane_type_t lane, 
                           size_t* size, uint32_t* priority) {
    if (!bus) return NULL;
    
    void* result = fast_lane_drain(&bus->fast_lanes, lane, size, priority);
    
    if (result) {
        bus->total_operations++;
        
        // Update performance metrics
        bus->messages_per_second++;
        bus->bytes_per_second += *size;
        
        // Report success to fault tolerance system
        fault_tolerance_update_component_health(&bus->fault_tolerance, (uint32_t)lane, true, 0);
    }
    
    return result;
}

lane_type_t umsbb_select_optimal_lane(size_t message_size, uint32_t priority, bool latency_critical) {
    return fast_lane_select_optimal(message_size, priority, latency_critical);
}

// V3.0 Twin Lane API implementations
uint32_t umsbb_twin_lane_create(UniversalMultiSegmentedBiBufferBus* bus, uint32_t peer_node_id, 
                               size_t tx_capacity, size_t rx_capacity) {
    if (!bus) return UINT32_MAX;
    
    uint32_t lane_id = twin_lane_create(&bus->twin_lanes, peer_node_id, tx_capacity, rx_capacity);
    
    if (lane_id != UINT32_MAX) {
        // Report successful creation to fault tolerance system
        fault_tolerance_update_component_health(&bus->fault_tolerance, lane_id + 1000, true, 0);
    } else {
        // Report failure
        fault_tolerance_report_fault(&bus->fault_tolerance, FAULT_TYPE_MEMORY, 
                                   0, "Twin lane creation failed");
    }
    
    return lane_id;
}

bool umsbb_twin_lane_send(UniversalMultiSegmentedBiBufferBus* bus, uint32_t lane_id, 
                         const void* data, size_t size, uint32_t sequence) {
    if (!bus) return false;
    
    bool success = twin_lane_send(&bus->twin_lanes, lane_id, data, size, sequence);
    
    if (success) {
        bus->total_operations++;
        bus->bytes_per_second += size;
        
        // Report success to fault tolerance system
        fault_tolerance_update_component_health(&bus->fault_tolerance, lane_id + 1000, true, 0);
    } else {
        // Report failure
        fault_tolerance_report_fault(&bus->fault_tolerance, FAULT_TYPE_OVERFLOW, 
                                   lane_id + 1000, "Twin lane send failed");
    }
    
    return success;
}

void* umsbb_twin_lane_receive(UniversalMultiSegmentedBiBufferBus* bus, uint32_t lane_id, 
                             size_t* size, uint32_t* sequence) {
    if (!bus) return NULL;
    
    void* result = twin_lane_receive(&bus->twin_lanes, lane_id, size, sequence);
    
    if (result) {
        bus->total_operations++;
        bus->bytes_per_second += *size;
        
        // Report success to fault tolerance system
        fault_tolerance_update_component_health(&bus->fault_tolerance, lane_id + 1000, true, 0);
    }
    
    return result;
}

// V3.0 Reliable Delivery API implementations
uint64_t umsbb_send_reliable(UniversalMultiSegmentedBiBufferBus* bus, uint32_t producer_id, 
                            uint32_t consumer_id, const void* data, size_t size) {
    if (!bus || !data || size == 0) return 0;
    
    uint64_t sequence = handshake_send_message(&bus->handshake, producer_id, consumer_id, data, size);
    
    if (sequence > 0) {
        bus->total_operations++;
        bus->bytes_per_second += size;
        
        // Persist message for zero-loss guarantee
        fault_tolerance_persist_message(sequence, data, size);
        
        // Update reliability score
        bus->reliability_score = (bus->reliability_score * 0.95) + (1.0 * 0.05);
    } else {
        // Report failure
        fault_tolerance_report_fault(&bus->fault_tolerance, FAULT_TYPE_OVERFLOW, 
                                   producer_id, "Reliable send failed");
        
        // Decrease reliability score
        bus->reliability_score = (bus->reliability_score * 0.95) + (0.0 * 0.05);
    }
    
    return sequence;
}

bool umsbb_send_feedback(UniversalMultiSegmentedBiBufferBus* bus, const feedback_message_t* feedback) {
    if (!bus || !feedback) return false;
    
    bool success = handshake_process_feedback(&bus->handshake, feedback);
    
    if (success) {
        // Update flow control based on feedback
        if (feedback->type == FEEDBACK_TYPE_ACK) {
            twin_lane_update_flow_control(&bus->twin_lanes, feedback->consumer_id, 
                                        (uint32_t)feedback->sequence);
        }
        
        // Update reliability score
        double feedback_score = (feedback->type == FEEDBACK_TYPE_ACK) ? 1.0 : 0.0;
        bus->reliability_score = (bus->reliability_score * 0.9) + (feedback_score * 0.1);
    }
    
    return success;
}

bool umsbb_process_acknowledgments(UniversalMultiSegmentedBiBufferBus* bus) {
    if (!bus) return false;
    
    // Process timeouts first
    handshake_process_timeouts(&bus->handshake);
    
    // Retry failed messages
    bool retries_processed = handshake_retry_failed_messages(&bus->handshake);
    
    return retries_processed;
}

// V3.0 Fault Tolerance API implementations
bool umsbb_is_component_healthy(UniversalMultiSegmentedBiBufferBus* bus, uint32_t component_id) {
    if (!bus) return false;
    
    return fault_tolerance_is_component_healthy(&bus->fault_tolerance, component_id);
}

uint64_t umsbb_report_fault(UniversalMultiSegmentedBiBufferBus* bus, fault_type_t type, 
                           uint32_t component_id, const char* description) {
    if (!bus) return 0;
    
    uint64_t fault_id = fault_tolerance_report_fault(&bus->fault_tolerance, type, 
                                                    component_id, description);
    
    // Update system health score
    bus->reliability_score = fault_tolerance_get_system_health(&bus->fault_tolerance);
    
    return fault_id;
}

double umsbb_get_system_health(UniversalMultiSegmentedBiBufferBus* bus) {
    if (!bus) return 0.0;
    
    return fault_tolerance_get_system_health(&bus->fault_tolerance);
}

// Performance monitoring
void umsbb_get_performance_metrics(UniversalMultiSegmentedBiBufferBus* bus, struct system_metrics* metrics) {
    if (!bus || !metrics) return;
    
    memset(metrics, 0, sizeof(struct system_metrics));
    
    // Aggregate metrics from all systems
    metrics->total_messages_per_second = bus->messages_per_second;
    metrics->total_bytes_per_second = bus->bytes_per_second;
    metrics->throughput_mbps = fast_lane_get_system_throughput(&bus->fast_lanes);
    metrics->reliability_score = bus->reliability_score;
    metrics->system_health_score = fault_tolerance_get_system_health(&bus->fault_tolerance);
    
    // Count active components
    metrics->active_lanes = bus->fast_lanes.active_lanes;
    metrics->active_twin_lanes = bus->twin_lanes.lane_count;
    metrics->pending_acknowledgments = handshake_get_pending_count(&bus->handshake, 0);
    metrics->active_faults = atomic_load(&bus->fault_tolerance.active_faults);
    
    // Calculate latency metrics (simplified)
    struct lane_metrics lane_metrics;
    fast_lane_get_metrics(&bus->fast_lanes, LANE_EXPRESS, &lane_metrics);
    metrics->avg_latency_us = lane_metrics.avg_latency_us;
    metrics->p99_latency_us = lane_metrics.p99_latency_us;
}

void umsbb_free(UniversalMultiSegmentedBiBufferBus* bus) {
    if (!bus) return;
    
    for (size_t i = 0; i < bus->ring.activeCount; ++i) {
        bi_buffer_destroy(&bus->ring.buffers[i]);
    }
    if (bus->arena.memory) free(bus->arena.memory);
    
    // Clean up V3.0 systems
    fast_lane_destroy(&bus->fast_lanes);
    twin_lane_destroy(&bus->twin_lanes);
    handshake_destroy(&bus->handshake);
    fault_tolerance_destroy(&bus->fault_tolerance);
    
    free(bus);
}