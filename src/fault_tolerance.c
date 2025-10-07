#include "fault_tolerance.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
    static uint64_t get_timestamp_us() {
        LARGE_INTEGER frequency, counter;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&counter);
        return (counter.QuadPart * 1000000ULL) / frequency.QuadPart;
    }
#else
    #include <sys/time.h>
    static uint64_t get_timestamp_us() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000000ULL + tv.tv_usec;
    }
#endif

static component_health_t* components = NULL;
static uint32_t max_components = 0;
static uint32_t component_count = 0;

bool fault_tolerance_init(fault_tolerance_manager_t* manager, uint32_t capacity) {
    if (!manager || capacity == 0) return false;
    
    memset(manager, 0, sizeof(fault_tolerance_manager_t));
    
    manager->records = calloc(capacity, sizeof(fault_record_t));
    if (!manager->records) return false;
    
    manager->capacity = capacity;
    atomic_store(&manager->head, 0);
    atomic_store(&manager->tail, 0);
    atomic_store(&manager->active_faults, 0);
    atomic_store(&manager->total_faults, 0);
    atomic_store(&manager->recovered_faults, 0);
    atomic_store(&manager->unrecoverable_faults, 0);
    
    // Default configuration
    manager->max_retry_attempts = 3;
    manager->fault_escalation_threshold = 10;
    manager->auto_recovery_enabled = true;
    manager->persistent_logging_enabled = true;
    manager->system_health_score = 1.0;
    
    // Initialize component health tracking
    max_components = 64; // Support up to 64 components
    components = calloc(max_components, sizeof(component_health_t));
    if (!components) {
        free(manager->records);
        return false;
    }
    
    return true;
}

void fault_tolerance_destroy(fault_tolerance_manager_t* manager) {
    if (!manager) return;
    
    if (manager->records) {
        free(manager->records);
    }
    
    if (components) {
        free(components);
        components = NULL;
    }
    
    memset(manager, 0, sizeof(fault_tolerance_manager_t));
}

uint64_t fault_tolerance_report_fault(fault_tolerance_manager_t* manager, fault_type_t type, 
                                      uint32_t component_id, const char* description) {
    if (!manager) return 0;
    
    uint32_t current_head = atomic_load(&manager->head);
    uint32_t index = current_head % manager->capacity;
    fault_record_t* record = &manager->records[index];
    
    uint64_t fault_id = atomic_fetch_add(&manager->total_faults, 1);
    
    // Initialize fault record
    record->fault_id = fault_id;
    record->type = type;
    record->timestamp_us = get_timestamp_us();
    record->component_id = component_id;
    record->retry_count = 0;
    record->action_taken = RECOVERY_ACTION_NONE;
    record->recovery_successful = false;
    record->recovery_time_us = 0;
    
    // Set severity based on fault type
    switch (type) {
        case FAULT_TYPE_CORRUPTION:
        case FAULT_TYPE_DEADLOCK:
            record->severity = 3; // Critical
            break;
        case FAULT_TYPE_TIMEOUT:
        case FAULT_TYPE_OVERFLOW:
        case FAULT_TYPE_GPU:
            record->severity = 2; // Error
            break;
        case FAULT_TYPE_UNDERFLOW:
        case FAULT_TYPE_MEMORY:
            record->severity = 1; // Warning
            break;
        default:
            record->severity = 0; // Info
            break;
    }
    
    if (description) {
        strncpy(record->description, description, sizeof(record->description) - 1);
        record->description[sizeof(record->description) - 1] = '\0';
    }
    
    atomic_fetch_add(&manager->head, 1);
    atomic_fetch_add(&manager->active_faults, 1);
    
    // Update component health
    if (component_id < max_components) {
        component_health_t* comp = &components[component_id];
        comp->component_id = component_id;
        comp->consecutive_failures++;
        atomic_fetch_add(&comp->failure_count, 1);
        
        // Update health score
        uint64_t total_ops = atomic_load(&comp->operations_count);
        uint64_t failures = atomic_load(&comp->failure_count);
        
        if (total_ops > 0) {
            comp->health_score = 1.0 - ((double)failures / total_ops);
        }
        
        // Mark as unhealthy if too many consecutive failures
        if (comp->consecutive_failures >= 5) {
            comp->is_healthy = false;
            comp->is_degraded = true;
        }
    }
    
    // Trigger automatic recovery if enabled
    if (manager->auto_recovery_enabled) {
        recovery_action_t action = fault_tolerance_determine_recovery_action(manager, record);
        fault_tolerance_execute_recovery(manager, fault_id, action);
    }
    
    // Update system health score
    uint64_t total_faults = atomic_load(&manager->total_faults);
    uint64_t recovered_faults = atomic_load(&manager->recovered_faults);
    
    if (total_faults > 0) {
        manager->system_health_score = (double)recovered_faults / total_faults;
    }
    
    return fault_id;
}

recovery_action_t fault_tolerance_determine_recovery_action(fault_tolerance_manager_t* manager, 
                                                            const fault_record_t* fault) {
    if (!manager || !fault) return RECOVERY_ACTION_NONE;
    
    // Determine recovery action based on fault type and severity
    switch (fault->type) {
        case FAULT_TYPE_CORRUPTION:
            return RECOVERY_ACTION_RESET; // Reset component for corruption
            
        case FAULT_TYPE_TIMEOUT:
            if (fault->retry_count < manager->max_retry_attempts) {
                return RECOVERY_ACTION_RETRY;
            } else {
                return RECOVERY_ACTION_FALLBACK;
            }
            
        case FAULT_TYPE_OVERFLOW:
        case FAULT_TYPE_UNDERFLOW:
            return RECOVERY_ACTION_FALLBACK; // Use alternative buffer
            
        case FAULT_TYPE_MEMORY:
            return RECOVERY_ACTION_RETRY; // Retry memory allocation
            
        case FAULT_TYPE_NETWORK:
            return RECOVERY_ACTION_RETRY; // Retry network operation
            
        case FAULT_TYPE_GPU:
            return RECOVERY_ACTION_FALLBACK; // Fallback to CPU
            
        case FAULT_TYPE_DEADLOCK:
            return RECOVERY_ACTION_RESET; // Reset to break deadlock
            
        case FAULT_TYPE_STARVATION:
            return RECOVERY_ACTION_ESCALATE; // Escalate for resource allocation
            
        default:
            return RECOVERY_ACTION_NONE;
    }
}

bool fault_tolerance_execute_recovery(fault_tolerance_manager_t* manager, uint64_t fault_id, 
                                       recovery_action_t action) {
    if (!manager || action == RECOVERY_ACTION_NONE) return false;
    
    uint64_t start_time = get_timestamp_us();
    bool recovery_successful = false;
    
    // Find the fault record
    fault_record_t* record = NULL;
    for (uint32_t i = 0; i < manager->capacity; i++) {
        if (manager->records[i].fault_id == fault_id) {
            record = &manager->records[i];
            break;
        }
    }
    
    if (!record) return false;
    
    record->action_taken = action;
    record->retry_count++;
    
    switch (action) {
        case RECOVERY_ACTION_RETRY:
            // Implement retry logic (simplified)
            recovery_successful = (record->retry_count <= manager->max_retry_attempts);
            break;
            
        case RECOVERY_ACTION_FALLBACK:
            // Implement fallback mechanism
            recovery_successful = true; // Assume fallback always works
            break;
            
        case RECOVERY_ACTION_RESET:
            // Reset component state
            if (record->component_id < max_components) {
                component_health_t* comp = &components[record->component_id];
                comp->consecutive_failures = 0;
                comp->is_healthy = true;
                comp->is_degraded = false;
                recovery_successful = true;
            }
            break;
            
        case RECOVERY_ACTION_ISOLATE:
            // Isolate faulty component
            if (record->component_id < max_components) {
                component_health_t* comp = &components[record->component_id];
                comp->is_isolated = true;
                comp->is_healthy = false;
                manager->degraded_components++;
                recovery_successful = true;
            }
            break;
            
        case RECOVERY_ACTION_ESCALATE:
            // Escalate to higher-level recovery
            recovery_successful = false; // Needs manual intervention
            break;
            
        default:
            recovery_successful = false;
            break;
    }
    
    uint64_t end_time = get_timestamp_us();
    record->recovery_time_us = end_time - start_time;
    record->recovery_successful = recovery_successful;
    
    manager->total_recovery_time_us += record->recovery_time_us;
    
    if (recovery_successful) {
        atomic_fetch_add(&manager->recovered_faults, 1);
        atomic_fetch_sub(&manager->active_faults, 1);
    } else {
        atomic_fetch_add(&manager->unrecoverable_faults, 1);
    }
    
    return recovery_successful;
}

bool fault_tolerance_is_component_healthy(fault_tolerance_manager_t* manager, uint32_t component_id) {
    if (!manager || component_id >= max_components) return false;
    
    component_health_t* comp = &components[component_id];
    
    // Check heartbeat freshness
    uint64_t current_time = get_timestamp_us();
    uint64_t last_heartbeat = atomic_load(&comp->last_heartbeat_us);
    
    if (current_time - last_heartbeat > 5000000) { // 5 seconds
        comp->is_healthy = false;
        return false;
    }
    
    return comp->is_healthy && !comp->is_isolated;
}

void fault_tolerance_update_component_health(fault_tolerance_manager_t* manager, uint32_t component_id, 
                                             bool operation_success, uint64_t response_time_us) {
    if (!manager || component_id >= max_components) return;
    
    component_health_t* comp = &components[component_id];
    
    // Update operation counters
    atomic_fetch_add(&comp->operations_count, 1);
    if (operation_success) {
        atomic_fetch_add(&comp->success_count, 1);
        comp->consecutive_failures = 0;
    } else {
        atomic_fetch_add(&comp->failure_count, 1);
        comp->consecutive_failures++;
    }
    
    // Update response time metrics
    if (comp->avg_response_time_us == 0.0) {
        comp->avg_response_time_us = (double)response_time_us;
    } else {
        comp->avg_response_time_us = (comp->avg_response_time_us * 0.9) + (response_time_us * 0.1);
    }
    
    if (response_time_us > comp->max_response_time_us) {
        comp->max_response_time_us = (double)response_time_us;
    }
    
    // Update heartbeat
    atomic_store(&comp->last_heartbeat_us, get_timestamp_us());
    
    // Update health status
    uint64_t total_ops = atomic_load(&comp->operations_count);
    uint64_t success_ops = atomic_load(&comp->success_count);
    
    if (total_ops > 0) {
        comp->health_score = (double)success_ops / total_ops;
        comp->is_healthy = (comp->health_score > 0.95) && (comp->consecutive_failures < 3);
        comp->is_degraded = (comp->health_score < 0.95) && (comp->health_score > 0.7);
    }
}

double fault_tolerance_get_system_health(fault_tolerance_manager_t* manager) {
    if (!manager) return 0.0;
    
    // Calculate system health based on component health and fault rates
    double total_health = 0.0;
    uint32_t healthy_components = 0;
    
    for (uint32_t i = 0; i < component_count; i++) {
        component_health_t* comp = &components[i];
        if (comp->component_id != 0) { // Valid component
            total_health += comp->health_score;
            if (comp->is_healthy) healthy_components++;
        }
    }
    
    double component_health_avg = (component_count > 0) ? (total_health / component_count) : 1.0;
    double component_availability = (component_count > 0) ? ((double)healthy_components / component_count) : 1.0;
    
    // Factor in fault recovery rate
    uint64_t total_faults = atomic_load(&manager->total_faults);
    uint64_t recovered_faults = atomic_load(&manager->recovered_faults);
    double recovery_rate = (total_faults > 0) ? ((double)recovered_faults / total_faults) : 1.0;
    
    // Weighted system health score
    manager->system_health_score = (component_health_avg * 0.4) + 
                                   (component_availability * 0.4) + 
                                   (recovery_rate * 0.2);
    
    return manager->system_health_score;
}

void fault_tolerance_get_metrics(fault_tolerance_manager_t* manager, struct fault_tolerance_metrics* metrics) {
    if (!manager || !metrics) return;
    
    memset(metrics, 0, sizeof(struct fault_tolerance_metrics));
    
    metrics->total_faults = atomic_load(&manager->total_faults);
    metrics->active_faults = atomic_load(&manager->active_faults);
    
    // Calculate fault rate (simplified)
    static uint64_t last_faults = 0;
    static uint64_t last_timestamp = 0;
    
    uint64_t current_time = get_timestamp_us();
    uint64_t time_diff = current_time - last_timestamp;
    
    if (time_diff > 0) {
        uint64_t fault_diff = metrics->total_faults - last_faults;
        metrics->fault_rate_per_second = ((double)fault_diff * 1000000.0) / time_diff;
        
        last_faults = metrics->total_faults;
        last_timestamp = current_time;
    }
    
    // Calculate recovery success rate
    uint64_t recovered = atomic_load(&manager->recovered_faults);
    if (metrics->total_faults > 0) {
        metrics->recovery_success_rate = ((double)recovered / metrics->total_faults) * 100.0;
    }
    
    // Calculate average recovery time
    if (recovered > 0) {
        metrics->avg_recovery_time_us = (double)manager->total_recovery_time_us / recovered;
    }
    
    metrics->system_health_score = fault_tolerance_get_system_health(manager);
    metrics->degraded_components = manager->degraded_components;
    
    // Note: Message persistence metrics would be tracked separately
    metrics->messages_persisted = 0; // Placeholder
    metrics->messages_recovered = 0; // Placeholder
}

// Message persistence functions (simplified implementations)
bool fault_tolerance_persist_message(uint64_t sequence, const void* data, size_t size) {
    // In a real implementation, this would write to persistent storage
    // For now, just return success
    return true;
}

void* fault_tolerance_recover_message(uint64_t sequence, size_t* size) {
    // In a real implementation, this would read from persistent storage
    // For now, return NULL (message not found)
    *size = 0;
    return NULL;
}

bool fault_tolerance_replay_messages(fault_tolerance_manager_t* manager, uint64_t from_sequence, uint64_t to_sequence) {
    // In a real implementation, this would replay messages from persistent storage
    // For now, just return success
    return true;
}