#include "feedback_handshake.h"
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
#else
    #include <sys/time.h>
    static uint64_t get_timestamp_us() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000000ULL + tv.tv_usec;
    }
#endif

static uint32_t calculate_hash(const void* data, size_t size) {
    // Simple FNV-1a hash for message integrity
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    
    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 16777619U;
    }
    
    return hash;
}

bool handshake_init(handshake_manager_t* manager, uint32_t capacity) {
    if (!manager || capacity == 0) return false;
    
    memset(manager, 0, sizeof(handshake_manager_t));
    
    manager->entries = calloc(capacity, sizeof(handshake_entry_t));
    if (!manager->entries) return false;
    
    manager->capacity = capacity;
    atomic_store(&manager->head, 0);
    atomic_store(&manager->tail, 0);
    atomic_store(&manager->pending_count, 0);
    
    // Default configuration
    manager->default_timeout_ms = 1000;  // 1 second default timeout
    manager->max_retries = 3;
    manager->retry_backoff_ms = 100;     // 100ms backoff
    manager->zero_loss_mode = true;      // Enable zero-loss by default
    
    return true;
}

void handshake_destroy(handshake_manager_t* manager) {
    if (!manager) return;
    
    if (manager->entries) {
        free(manager->entries);
    }
    
    memset(manager, 0, sizeof(handshake_manager_t));
}

uint64_t handshake_send_message(handshake_manager_t* manager, uint32_t producer_id, uint32_t consumer_id, 
                                const void* data, size_t size) {
    if (!manager || !data || size == 0) return 0;
    
    uint32_t current_head = atomic_load(&manager->head);
    uint32_t current_tail = atomic_load(&manager->tail);
    
    // Check if queue is full
    if ((current_head + 1) % manager->capacity == current_tail) {
        return 0; // Queue full
    }
    
    uint64_t sequence = atomic_fetch_add(&manager->head, 1);
    uint32_t index = sequence % manager->capacity;
    handshake_entry_t* entry = &manager->entries[index];
    
    // Initialize handshake entry
    entry->sequence = sequence;
    entry->producer_id = producer_id;
    entry->consumer_id = consumer_id;
    entry->timestamp_us = get_timestamp_us();
    entry->state = HANDSHAKE_STATE_PENDING;
    entry->retry_count = 0;
    entry->message_size = size;
    entry->message_hash = calculate_hash(data, size);
    entry->sent_timestamp_us = entry->timestamp_us;
    entry->ack_timestamp_us = 0;
    entry->timeout_ms = manager->default_timeout_ms;
    
    // Increment pending count
    atomic_fetch_add(&manager->pending_count, 1);
    manager->total_messages++;
    
    return sequence;
}

bool handshake_process_feedback(handshake_manager_t* manager, const feedback_message_t* feedback) {
    if (!manager || !feedback) return false;
    
    // Find the handshake entry by sequence
    uint32_t index = feedback->sequence % manager->capacity;
    handshake_entry_t* entry = &manager->entries[index];
    
    // Verify this is the correct entry
    if (entry->sequence != feedback->sequence || 
        entry->producer_id != feedback->producer_id ||
        entry->consumer_id != feedback->consumer_id) {
        return false; // Entry mismatch
    }
    
    uint64_t current_time = get_timestamp_us();
    entry->ack_timestamp_us = current_time;
    
    switch (feedback->type) {
        case FEEDBACK_TYPE_ACK:
            entry->state = HANDSHAKE_STATE_ACKED;
            manager->successful_acks++;
            
            // Calculate and update latency metrics
            double ack_latency = (double)(current_time - entry->sent_timestamp_us);
            if (manager->avg_ack_latency_us == 0.0) {
                manager->avg_ack_latency_us = ack_latency;
            } else {
                manager->avg_ack_latency_us = (manager->avg_ack_latency_us * 0.9) + (ack_latency * 0.1);
            }
            
            // Decrement pending count
            atomic_fetch_sub(&manager->pending_count, 1);
            break;
            
        case FEEDBACK_TYPE_NACK:
            entry->state = HANDSHAKE_STATE_NACKED;
            if (manager->zero_loss_mode && entry->retry_count < manager->max_retries) {
                entry->state = HANDSHAKE_STATE_RETRY;
                entry->retry_count++;
                manager->retries++;
            } else {
                manager->failed_deliveries++;
                atomic_fetch_sub(&manager->pending_count, 1);
            }
            break;
            
        case FEEDBACK_TYPE_BUSY:
            // Consumer is busy, schedule retry
            if (entry->retry_count < manager->max_retries) {
                entry->state = HANDSHAKE_STATE_RETRY;
                entry->retry_count++;
                entry->timeout_ms += manager->retry_backoff_ms; // Increase timeout
                manager->retries++;
            } else {
                entry->state = HANDSHAKE_STATE_NACKED;
                manager->failed_deliveries++;
                atomic_fetch_sub(&manager->pending_count, 1);
            }
            break;
            
        case FEEDBACK_TYPE_OVERFLOW:
            // Buffer overflow, schedule retry with longer timeout
            if (entry->retry_count < manager->max_retries) {
                entry->state = HANDSHAKE_STATE_RETRY;
                entry->retry_count++;
                entry->timeout_ms *= 2; // Exponential backoff
                manager->retries++;
            } else {
                entry->state = HANDSHAKE_STATE_NACKED;
                manager->failed_deliveries++;
                atomic_fetch_sub(&manager->pending_count, 1);
            }
            break;
            
        case FEEDBACK_TYPE_READY:
            // Consumer is ready for next message (proactive flow control)
            // This doesn't change the current entry state but signals readiness
            break;
    }
    
    return true;
}

void handshake_process_timeouts(handshake_manager_t* manager) {
    if (!manager) return;
    
    uint64_t current_time = get_timestamp_us();
    uint32_t current_tail = atomic_load(&manager->tail);
    uint32_t current_head = atomic_load(&manager->head);
    
    // Process entries from tail to head
    for (uint32_t i = current_tail; i < current_head; i++) {
        uint32_t index = i % manager->capacity;
        handshake_entry_t* entry = &manager->entries[index];
        
        if (entry->state != HANDSHAKE_STATE_PENDING) continue;
        
        // Check if timeout occurred
        uint64_t elapsed_us = current_time - entry->sent_timestamp_us;
        if (elapsed_us > (entry->timeout_ms * 1000)) {
            entry->state = HANDSHAKE_STATE_TIMEOUT;
            manager->timeouts++;
            
            if (manager->zero_loss_mode && entry->retry_count < manager->max_retries) {
                entry->state = HANDSHAKE_STATE_RETRY;
                entry->retry_count++;
                entry->timeout_ms *= 2; // Exponential backoff
                manager->retries++;
            } else {
                manager->failed_deliveries++;
                atomic_fetch_sub(&manager->pending_count, 1);
            }
        }
    }
}

bool handshake_retry_failed_messages(handshake_manager_t* manager) {
    if (!manager) return false;
    
    uint32_t retries_processed = 0;
    uint32_t current_tail = atomic_load(&manager->tail);
    uint32_t current_head = atomic_load(&manager->head);
    
    // Process entries marked for retry
    for (uint32_t i = current_tail; i < current_head; i++) {
        uint32_t index = i % manager->capacity;
        handshake_entry_t* entry = &manager->entries[index];
        
        if (entry->state == HANDSHAKE_STATE_RETRY) {
            // Reset entry for retry
            entry->state = HANDSHAKE_STATE_PENDING;
            entry->sent_timestamp_us = get_timestamp_us();
            entry->ack_timestamp_us = 0;
            retries_processed++;
            
            // Limit retries per call to prevent blocking
            if (retries_processed >= 10) break;
        }
    }
    
    return retries_processed > 0;
}

feedback_message_t handshake_create_ack(uint64_t sequence, uint32_t producer_id, uint32_t consumer_id) {
    feedback_message_t feedback = {0};
    feedback.sequence = sequence;
    feedback.type = FEEDBACK_TYPE_ACK;
    feedback.producer_id = producer_id;
    feedback.consumer_id = consumer_id;
    feedback.timestamp_us = get_timestamp_us();
    feedback.error_code = 0;
    strcpy(feedback.error_message, "OK");
    return feedback;
}

feedback_message_t handshake_create_nack(uint64_t sequence, uint32_t producer_id, uint32_t consumer_id, 
                                         uint32_t error_code, const char* error_message) {
    feedback_message_t feedback = {0};
    feedback.sequence = sequence;
    feedback.type = FEEDBACK_TYPE_NACK;
    feedback.producer_id = producer_id;
    feedback.consumer_id = consumer_id;
    feedback.timestamp_us = get_timestamp_us();
    feedback.error_code = error_code;
    
    if (error_message) {
        strncpy(feedback.error_message, error_message, sizeof(feedback.error_message) - 1);
        feedback.error_message[sizeof(feedback.error_message) - 1] = '\0';
    }
    
    return feedback;
}

feedback_message_t handshake_create_ready(uint32_t producer_id, uint32_t consumer_id) {
    feedback_message_t feedback = {0};
    feedback.sequence = 0; // Not tied to specific sequence
    feedback.type = FEEDBACK_TYPE_READY;
    feedback.producer_id = producer_id;
    feedback.consumer_id = consumer_id;
    feedback.timestamp_us = get_timestamp_us();
    feedback.error_code = 0;
    strcpy(feedback.error_message, "READY");
    return feedback;
}

bool handshake_can_send_more(handshake_manager_t* manager, uint32_t producer_id) {
    if (!manager) return false;
    
    uint32_t pending = atomic_load(&manager->pending_count);
    uint32_t max_pending = manager->capacity / 4; // Allow up to 25% of capacity pending
    
    return pending < max_pending;
}

uint32_t handshake_get_pending_count(handshake_manager_t* manager, uint32_t producer_id) {
    if (!manager) return 0;
    
    // For now, return global pending count
    // Real implementation would track per-producer
    return atomic_load(&manager->pending_count);
}

void handshake_get_metrics(handshake_manager_t* manager, struct handshake_metrics* metrics) {
    if (!manager || !metrics) return;
    
    memset(metrics, 0, sizeof(struct handshake_metrics));
    
    // Calculate rates (simplified)
    static uint64_t last_messages = 0;
    static uint64_t last_acks = 0;
    static uint64_t last_timestamp = 0;
    
    uint64_t current_time = get_timestamp_us();
    uint64_t time_diff = current_time - last_timestamp;
    
    if (time_diff > 0) {
        uint64_t message_diff = manager->total_messages - last_messages;
        uint64_t ack_diff = manager->successful_acks - last_acks;
        
        metrics->messages_per_second = (message_diff * 1000000) / time_diff;
        metrics->acks_per_second = (ack_diff * 1000000) / time_diff;
        
        last_messages = manager->total_messages;
        last_acks = manager->successful_acks;
        last_timestamp = current_time;
    }
    
    // Calculate delivery success rate
    if (manager->total_messages > 0) {
        metrics->delivery_success_rate = 
            ((double)manager->successful_acks / manager->total_messages) * 100.0;
    }
    
    metrics->avg_ack_latency_us = manager->avg_ack_latency_us;
    metrics->p99_ack_latency_us = manager->avg_ack_latency_us * 1.5; // Simplified P99
    metrics->pending_messages = atomic_load(&manager->pending_count);
    
    // Calculate retry rate
    if (manager->total_messages > 0) {
        metrics->retry_rate = (manager->retries * 100) / manager->total_messages;
    }
    
    metrics->total_timeouts = manager->timeouts;
}