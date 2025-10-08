#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "atomic_compat.h"

/*
Feedback Handshake System - Reliable delivery with acknowledgment-based flow control

Features:
- Guaranteed delivery with ACK/NACK mechanism
- Adaptive timeout and retry logic
- Sequence-based ordering
- Congestion control via feedback
- Zero-loss guarantee with persistent retry
*/

typedef enum {
    HANDSHAKE_STATE_PENDING = 0,    // Waiting for ACK
    HANDSHAKE_STATE_ACKED = 1,      // Successfully acknowledged
    HANDSHAKE_STATE_NACKED = 2,     // Negative acknowledgment
    HANDSHAKE_STATE_TIMEOUT = 3,    // Timeout expired
    HANDSHAKE_STATE_RETRY = 4       // Scheduled for retry
} handshake_state_t;

typedef enum {
    FEEDBACK_TYPE_ACK = 0,          // Positive acknowledgment
    FEEDBACK_TYPE_NACK = 1,         // Negative acknowledgment
    FEEDBACK_TYPE_BUSY = 2,         // Consumer busy, retry later
    FEEDBACK_TYPE_OVERFLOW = 3,     // Buffer overflow
    FEEDBACK_TYPE_READY = 4         // Ready for next message
} feedback_type_t;

typedef struct {
    uint64_t sequence;
    uint32_t producer_id;
    uint32_t consumer_id;
    uint64_t timestamp_us;
    feedback_type_t type;
    uint32_t retry_count;
    handshake_state_t state;
    
    // Message metadata
    size_t message_size;
    uint32_t message_hash;  // For integrity verification
    
    // Timing information
    uint64_t sent_timestamp_us;
    uint64_t ack_timestamp_us;
    uint32_t timeout_ms;
} handshake_entry_t;

typedef struct {
    handshake_entry_t* entries;
    uint32_t capacity;
    atomic_uint32_t head;
    atomic_uint32_t tail;
    atomic_uint32_t pending_count;
    
    // Configuration
    uint32_t default_timeout_ms;
    uint32_t max_retries;
    uint32_t retry_backoff_ms;
    bool zero_loss_mode;
    
    // Performance metrics
    uint64_t total_messages;
    uint64_t successful_acks;
    uint64_t failed_deliveries;
    uint64_t timeouts;
    uint64_t retries;
    double avg_ack_latency_us;
} handshake_manager_t;

typedef struct {
    uint64_t sequence;
    feedback_type_t type;
    uint32_t producer_id;
    uint32_t consumer_id;
    uint64_t timestamp_us;
    uint32_t error_code;
    char error_message[64];
} feedback_message_t;

// Performance metrics struct must be visible before prototypes
struct handshake_metrics {
    uint64_t messages_per_second;
    uint64_t acks_per_second;
    double delivery_success_rate;
    double avg_ack_latency_us;
    double p99_ack_latency_us;
    uint32_t pending_messages;
    uint32_t retry_rate;
    uint64_t total_timeouts;
};

// Handshake API
bool handshake_init(handshake_manager_t* manager, uint32_t capacity);
void handshake_destroy(handshake_manager_t* manager);

// Message lifecycle management
uint64_t handshake_send_message(handshake_manager_t* manager, uint32_t producer_id, uint32_t consumer_id, 
                                const void* data, size_t size);
bool handshake_process_feedback(handshake_manager_t* manager, const feedback_message_t* feedback);

// Timeout and retry management
void handshake_process_timeouts(handshake_manager_t* manager);
bool handshake_retry_failed_messages(handshake_manager_t* manager);

// Consumer feedback generation
feedback_message_t handshake_create_ack(uint64_t sequence, uint32_t producer_id, uint32_t consumer_id);
feedback_message_t handshake_create_nack(uint64_t sequence, uint32_t producer_id, uint32_t consumer_id, 
                                         uint32_t error_code, const char* error_message);
feedback_message_t handshake_create_ready(uint32_t producer_id, uint32_t consumer_id);

// Flow control
bool handshake_can_send_more(handshake_manager_t* manager, uint32_t producer_id);
uint32_t handshake_get_pending_count(handshake_manager_t* manager, uint32_t producer_id);

// Performance monitoring
void handshake_get_metrics(handshake_manager_t* manager, struct handshake_metrics* metrics);