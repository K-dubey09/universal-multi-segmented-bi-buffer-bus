#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "atomic_compat.h"

/*
Zero Fault Tolerance System - Comprehensive fault detection, recovery, and prevention

Features:
- Automatic error detection and recovery
- Message persistence and replay
- Health monitoring and diagnostics
- Graceful degradation under failure
- Self-healing capabilities
*/

typedef enum {
    FAULT_TYPE_NONE = 0,
    FAULT_TYPE_CORRUPTION = 1,      // Data corruption detected
    FAULT_TYPE_TIMEOUT = 2,         // Operation timeout
    FAULT_TYPE_OVERFLOW = 3,        // Buffer overflow
    FAULT_TYPE_UNDERFLOW = 4,       // Buffer underflow
    FAULT_TYPE_MEMORY = 5,          // Memory allocation failure
    FAULT_TYPE_NETWORK = 6,         // Network connectivity issue
    FAULT_TYPE_GPU = 7,             // GPU acceleration failure
    FAULT_TYPE_DEADLOCK = 8,        // Deadlock detected
    FAULT_TYPE_STARVATION = 9       // Resource starvation
} fault_type_t;

typedef enum {
    RECOVERY_ACTION_NONE = 0,
    RECOVERY_ACTION_RETRY = 1,      // Retry the operation
    RECOVERY_ACTION_FALLBACK = 2,   // Use fallback mechanism
    RECOVERY_ACTION_RESET = 3,      // Reset component
    RECOVERY_ACTION_ISOLATE = 4,    // Isolate faulty component
    RECOVERY_ACTION_ESCALATE = 5    // Escalate to higher level
} recovery_action_t;

typedef struct {
    uint64_t fault_id;
    fault_type_t type;
    uint64_t timestamp_us;
    uint32_t component_id;
    uint32_t severity;  // 0=info, 1=warning, 2=error, 3=critical
    
    // Fault context
    char description[128];
    uint64_t sequence_number;
    size_t data_size;
    uint32_t retry_count;
    
    // Recovery information
    recovery_action_t action_taken;
    bool recovery_successful;
    uint64_t recovery_time_us;
} fault_record_t;

typedef struct {
    fault_record_t* records;
    uint32_t capacity;
    atomic_uint32_t head;
    atomic_uint32_t tail;
    atomic_uint32_t active_faults;
    
    // Health monitoring
    atomic_uint64_t total_faults;
    atomic_uint64_t recovered_faults;
    atomic_uint64_t unrecoverable_faults;
    
    // Performance impact tracking
    uint64_t total_recovery_time_us;
    uint32_t degraded_components;
    double system_health_score;  // 0.0 to 1.0
    
    // Configuration
    uint32_t max_retry_attempts;
    uint32_t fault_escalation_threshold;
    bool auto_recovery_enabled;
    bool persistent_logging_enabled;
} fault_tolerance_manager_t;

typedef struct {
    uint32_t component_id;
    atomic_uint64_t operations_count;
    atomic_uint64_t success_count;
    atomic_uint64_t failure_count;
    atomic_uint64_t last_heartbeat_us;
    
    // Performance metrics
    double avg_response_time_us;
    double max_response_time_us;
    uint32_t consecutive_failures;
    
    // Health status
    bool is_healthy;
    bool is_degraded;
    bool is_isolated;
    double health_score;
} component_health_t;

// Fault Tolerance API
bool fault_tolerance_init(fault_tolerance_manager_t* manager, uint32_t capacity);
void fault_tolerance_destroy(fault_tolerance_manager_t* manager);

// Fault detection and reporting
uint64_t fault_tolerance_report_fault(fault_tolerance_manager_t* manager, fault_type_t type, 
                                      uint32_t component_id, const char* description);
bool fault_tolerance_is_component_healthy(fault_tolerance_manager_t* manager, uint32_t component_id);

// Recovery management
recovery_action_t fault_tolerance_determine_recovery_action(fault_tolerance_manager_t* manager, 
                                                            const fault_record_t* fault);
bool fault_tolerance_execute_recovery(fault_tolerance_manager_t* manager, uint64_t fault_id, 
                                       recovery_action_t action);

// Health monitoring
void fault_tolerance_update_component_health(fault_tolerance_manager_t* manager, uint32_t component_id, 
                                             bool operation_success, uint64_t response_time_us);
double fault_tolerance_get_system_health(fault_tolerance_manager_t* manager);

// Message persistence for zero-loss guarantee
bool fault_tolerance_persist_message(uint64_t sequence, const void* data, size_t size);
void* fault_tolerance_recover_message(uint64_t sequence, size_t* size);
bool fault_tolerance_replay_messages(fault_tolerance_manager_t* manager, uint64_t from_sequence, uint64_t to_sequence);

// Graceful degradation
bool fault_tolerance_enable_degraded_mode(fault_tolerance_manager_t* manager, uint32_t component_id);
bool fault_tolerance_disable_component(fault_tolerance_manager_t* manager, uint32_t component_id);
bool fault_tolerance_isolate_component(fault_tolerance_manager_t* manager, uint32_t component_id);

// Diagnostics and metrics
// Diagnostics and metrics
struct fault_tolerance_metrics {
    uint64_t total_faults;
    uint64_t active_faults;
    double fault_rate_per_second;
    double recovery_success_rate;
    double avg_recovery_time_us;
    double system_health_score;
    uint32_t degraded_components;
    uint64_t messages_persisted;
    uint64_t messages_recovered;
};

void fault_tolerance_get_metrics(fault_tolerance_manager_t* manager, struct fault_tolerance_metrics* metrics);
void fault_tolerance_generate_health_report(fault_tolerance_manager_t* manager, char* report, size_t report_size);