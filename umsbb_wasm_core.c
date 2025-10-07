/*
 * UMSBB WebAssembly Core - Optimized for Web Integration
 * Universal Multi-Segmented Bi-Buffer Bus v4.0
 * 
 * This is a web-optimized version of the UMSBB core designed specifically
 * for WebAssembly compilation and direct webpage integration.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WASM_EXPORT
#endif

// Configuration constants
#define UMSBB_MAX_BUFFERS 16
#define UMSBB_NUM_SEGMENTS 8
#define UMSBB_DEFAULT_SEGMENT_SIZE (1024 * 1024)  // 1MB
#define UMSBB_MAX_MESSAGE_SIZE (1024 * 1024)      // 1MB
#define UMSBB_HEADER_SIZE 16

// Error codes
typedef enum {
    UMSBB_SUCCESS = 0,
    UMSBB_ERROR_INVALID_PARAMS = -1,
    UMSBB_ERROR_BUFFER_FULL = -2,
    UMSBB_ERROR_BUFFER_EMPTY = -3,
    UMSBB_ERROR_INVALID_BUFFER = -4,
    UMSBB_ERROR_MESSAGE_TOO_LARGE = -5,
    UMSBB_ERROR_MEMORY_ALLOCATION = -6,
    UMSBB_ERROR_SYSTEM_NOT_INITIALIZED = -7
} umsbb_error_t;

// Message header structure
typedef struct {
    uint32_t size;
    uint32_t checksum;
    uint64_t timestamp;
} __attribute__((packed)) umsbb_message_header_t;

// Buffer segment structure
typedef struct {
    uint8_t* data;
    volatile uint32_t write_pos;
    volatile uint32_t read_pos;
    uint32_t capacity;
    volatile uint32_t message_count;
} umsbb_segment_t;

// Buffer statistics
typedef struct {
    uint64_t total_messages_written;
    uint64_t total_messages_read;
    uint64_t total_bytes_written;
    uint64_t total_bytes_read;
    uint32_t pending_messages;
    uint32_t active_segments;
    uint32_t peak_pending_messages;
    double average_message_size;
} umsbb_stats_t;

// Main buffer structure
typedef struct {
    umsbb_segment_t segments[UMSBB_NUM_SEGMENTS];
    volatile uint32_t current_write_segment;
    volatile uint32_t current_read_segment;
    uint32_t segment_size;
    uint32_t num_segments;
    umsbb_stats_t stats;
    int is_initialized;
} umsbb_buffer_t;

// Global state
static umsbb_buffer_t* g_buffers[UMSBB_MAX_BUFFERS];
static int g_system_initialized = 0;
static volatile uint32_t g_next_buffer_id = 1;

// Utility functions
static uint32_t calculate_checksum(const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t checksum = 0x5A5A5A5A;
    for (size_t i = 0; i < size; i++) {
        checksum = ((checksum << 1) | (checksum >> 31)) ^ bytes[i];
    }
    return checksum;
}

static uint64_t get_timestamp_ms() {
    // Simplified timestamp for WebAssembly
    static uint64_t counter = 0;
    return ++counter;
}

static void* wasm_malloc(size_t size) {
    // Simple allocation for WebAssembly
    static uint8_t heap[8 * 1024 * 1024]; // 8MB heap
    static size_t heap_pos = 0;
    
    size = (size + 7) & ~7; // 8-byte alignment
    if (heap_pos + size > sizeof(heap)) {
        return NULL;
    }
    
    void* ptr = &heap[heap_pos];
    heap_pos += size;
    return ptr;
}

// Forward declarations
WASM_EXPORT int umsbb_destroy_buffer(int buffer_id);

// Core API Functions

WASM_EXPORT int umsbb_init_system() {
    if (g_system_initialized) {
        return UMSBB_SUCCESS;
    }
    
    // Initialize global state
    for (int i = 0; i < UMSBB_MAX_BUFFERS; i++) {
        g_buffers[i] = NULL;
    }
    
    g_system_initialized = 1;
    return UMSBB_SUCCESS;
}

WASM_EXPORT int umsbb_shutdown_system() {
    if (!g_system_initialized) {
        return UMSBB_ERROR_SYSTEM_NOT_INITIALIZED;
    }
    
    // Clean up all buffers
    for (int i = 0; i < UMSBB_MAX_BUFFERS; i++) {
        if (g_buffers[i]) {
            umsbb_destroy_buffer(i);
        }
    }
    
    g_system_initialized = 0;
    return UMSBB_SUCCESS;
}

WASM_EXPORT int umsbb_create_buffer(uint32_t segment_size, uint32_t num_segments) {
    if (!g_system_initialized) {
        return UMSBB_ERROR_SYSTEM_NOT_INITIALIZED;
    }
    
    if (segment_size == 0 || num_segments == 0 || num_segments > UMSBB_NUM_SEGMENTS) {
        return UMSBB_ERROR_INVALID_PARAMS;
    }
    
    // Find free buffer slot
    int buffer_id = -1;
    for (int i = 0; i < UMSBB_MAX_BUFFERS; i++) {
        if (g_buffers[i] == NULL) {
            buffer_id = i;
            break;
        }
    }
    
    if (buffer_id == -1) {
        return UMSBB_ERROR_MEMORY_ALLOCATION;
    }
    
    // Allocate buffer structure
    umsbb_buffer_t* buffer = (umsbb_buffer_t*)wasm_malloc(sizeof(umsbb_buffer_t));
    if (!buffer) {
        return UMSBB_ERROR_MEMORY_ALLOCATION;
    }
    
    // Initialize buffer
    memset(buffer, 0, sizeof(umsbb_buffer_t));
    buffer->segment_size = segment_size;
    buffer->num_segments = num_segments;
    
    // Allocate segments
    for (uint32_t i = 0; i < num_segments; i++) {
        buffer->segments[i].data = (uint8_t*)wasm_malloc(segment_size);
        if (!buffer->segments[i].data) {
            return UMSBB_ERROR_MEMORY_ALLOCATION;
        }
        buffer->segments[i].capacity = segment_size;
        buffer->segments[i].write_pos = 0;
        buffer->segments[i].read_pos = 0;
        buffer->segments[i].message_count = 0;
    }
    
    buffer->is_initialized = 1;
    g_buffers[buffer_id] = buffer;
    
    return buffer_id;
}

WASM_EXPORT int umsbb_destroy_buffer(int buffer_id) {
    if (!g_system_initialized || buffer_id < 0 || buffer_id >= UMSBB_MAX_BUFFERS) {
        return UMSBB_ERROR_INVALID_PARAMS;
    }
    
    umsbb_buffer_t* buffer = g_buffers[buffer_id];
    if (!buffer) {
        return UMSBB_ERROR_INVALID_BUFFER;
    }
    
    buffer->is_initialized = 0;
    g_buffers[buffer_id] = NULL;
    
    return UMSBB_SUCCESS;
}

WASM_EXPORT int umsbb_write_message(int buffer_id, const void* data, uint32_t size) {
    if (!g_system_initialized || buffer_id < 0 || buffer_id >= UMSBB_MAX_BUFFERS) {
        return UMSBB_ERROR_INVALID_PARAMS;
    }
    
    if (!data || size == 0 || size > UMSBB_MAX_MESSAGE_SIZE) {
        return UMSBB_ERROR_INVALID_PARAMS;
    }
    
    umsbb_buffer_t* buffer = g_buffers[buffer_id];
    if (!buffer || !buffer->is_initialized) {
        return UMSBB_ERROR_INVALID_BUFFER;
    }
    
    uint32_t total_size = UMSBB_HEADER_SIZE + size;
    
    // Find segment with enough space
    uint32_t segment_idx = buffer->current_write_segment;
    umsbb_segment_t* segment = &buffer->segments[segment_idx];
    
    // Check if current segment has space
    if (segment->write_pos + total_size > segment->capacity) {
        // Try next segment
        segment_idx = (segment_idx + 1) % buffer->num_segments;
        segment = &buffer->segments[segment_idx];
        
        // Reset segment if it's full
        if (segment->write_pos + total_size > segment->capacity) {
            segment->write_pos = 0;
            segment->read_pos = 0;
            segment->message_count = 0;
        }
        
        buffer->current_write_segment = segment_idx;
    }
    
    // Write message header
    umsbb_message_header_t header;
    header.size = size;
    header.checksum = calculate_checksum(data, size);
    header.timestamp = get_timestamp_ms();
    
    memcpy(segment->data + segment->write_pos, &header, UMSBB_HEADER_SIZE);
    memcpy(segment->data + segment->write_pos + UMSBB_HEADER_SIZE, data, size);
    
    segment->write_pos += total_size;
    segment->message_count++;
    
    // Update statistics
    buffer->stats.total_messages_written++;
    buffer->stats.total_bytes_written += size;
    buffer->stats.pending_messages++;
    
    if (buffer->stats.pending_messages > buffer->stats.peak_pending_messages) {
        buffer->stats.peak_pending_messages = buffer->stats.pending_messages;
    }
    
    buffer->stats.average_message_size = 
        (double)buffer->stats.total_bytes_written / buffer->stats.total_messages_written;
    
    return UMSBB_SUCCESS;
}

WASM_EXPORT int umsbb_read_message(int buffer_id, void* output_buffer, uint32_t max_size) {
    if (!g_system_initialized || buffer_id < 0 || buffer_id >= UMSBB_MAX_BUFFERS) {
        return UMSBB_ERROR_INVALID_PARAMS;
    }
    
    if (!output_buffer || max_size == 0) {
        return UMSBB_ERROR_INVALID_PARAMS;
    }
    
    umsbb_buffer_t* buffer = g_buffers[buffer_id];
    if (!buffer || !buffer->is_initialized) {
        return UMSBB_ERROR_INVALID_BUFFER;
    }
    
    // Find segment with messages
    uint32_t segment_idx = buffer->current_read_segment;
    umsbb_segment_t* segment = &buffer->segments[segment_idx];
    
    // Check if current segment has messages
    if (segment->read_pos >= segment->write_pos || segment->message_count == 0) {
        // Try other segments
        for (uint32_t i = 0; i < buffer->num_segments; i++) {
            uint32_t idx = (segment_idx + i) % buffer->num_segments;
            if (buffer->segments[idx].message_count > 0 && 
                buffer->segments[idx].read_pos < buffer->segments[idx].write_pos) {
                segment_idx = idx;
                segment = &buffer->segments[segment_idx];
                buffer->current_read_segment = segment_idx;
                break;
            }
        }
        
        if (segment->read_pos >= segment->write_pos || segment->message_count == 0) {
            return UMSBB_ERROR_BUFFER_EMPTY;
        }
    }
    
    // Read message header
    if (segment->read_pos + UMSBB_HEADER_SIZE > segment->write_pos) {
        return UMSBB_ERROR_BUFFER_EMPTY;
    }
    
    umsbb_message_header_t header;
    memcpy(&header, segment->data + segment->read_pos, UMSBB_HEADER_SIZE);
    
    if (header.size > max_size) {
        return UMSBB_ERROR_MESSAGE_TOO_LARGE;
    }
    
    // Read message data
    memcpy(output_buffer, segment->data + segment->read_pos + UMSBB_HEADER_SIZE, header.size);
    
    // Verify checksum
    uint32_t checksum = calculate_checksum(output_buffer, header.size);
    if (checksum != header.checksum) {
        return UMSBB_ERROR_INVALID_PARAMS; // Corruption detected
    }
    
    segment->read_pos += UMSBB_HEADER_SIZE + header.size;
    segment->message_count--;
    
    // Update statistics
    buffer->stats.total_messages_read++;
    buffer->stats.total_bytes_read += header.size;
    buffer->stats.pending_messages--;
    
    return (int)header.size;
}

// Statistics and info functions
WASM_EXPORT uint64_t umsbb_get_total_messages(int buffer_id) {
    if (!g_system_initialized || buffer_id < 0 || buffer_id >= UMSBB_MAX_BUFFERS) {
        return 0;
    }
    
    umsbb_buffer_t* buffer = g_buffers[buffer_id];
    if (!buffer || !buffer->is_initialized) {
        return 0;
    }
    
    return buffer->stats.total_messages_written;
}

WASM_EXPORT uint64_t umsbb_get_total_bytes(int buffer_id) {
    if (!g_system_initialized || buffer_id < 0 || buffer_id >= UMSBB_MAX_BUFFERS) {
        return 0;
    }
    
    umsbb_buffer_t* buffer = g_buffers[buffer_id];
    if (!buffer || !buffer->is_initialized) {
        return 0;
    }
    
    return buffer->stats.total_bytes_written;
}

WASM_EXPORT uint32_t umsbb_get_pending_messages(int buffer_id) {
    if (!g_system_initialized || buffer_id < 0 || buffer_id >= UMSBB_MAX_BUFFERS) {
        return 0;
    }
    
    umsbb_buffer_t* buffer = g_buffers[buffer_id];
    if (!buffer || !buffer->is_initialized) {
        return 0;
    }
    
    return buffer->stats.pending_messages;
}

WASM_EXPORT const char* umsbb_get_version() {
    return "UMSBB WebAssembly Core v4.0";
}

WASM_EXPORT const char* umsbb_get_error_string(int error_code) {
    switch (error_code) {
        case UMSBB_SUCCESS: return "Success";
        case UMSBB_ERROR_INVALID_PARAMS: return "Invalid parameters";
        case UMSBB_ERROR_BUFFER_FULL: return "Buffer full";
        case UMSBB_ERROR_BUFFER_EMPTY: return "Buffer empty";
        case UMSBB_ERROR_INVALID_BUFFER: return "Invalid buffer";
        case UMSBB_ERROR_MESSAGE_TOO_LARGE: return "Message too large";
        case UMSBB_ERROR_MEMORY_ALLOCATION: return "Memory allocation failed";
        case UMSBB_ERROR_SYSTEM_NOT_INITIALIZED: return "System not initialized";
        default: return "Unknown error";
    }
}

// Performance testing function
WASM_EXPORT int umsbb_run_performance_test(int buffer_id, uint32_t message_count, uint32_t message_size) {
    if (!g_system_initialized || buffer_id < 0 || buffer_id >= UMSBB_MAX_BUFFERS) {
        return UMSBB_ERROR_INVALID_PARAMS;
    }
    
    if (message_size > UMSBB_MAX_MESSAGE_SIZE) {
        return UMSBB_ERROR_MESSAGE_TOO_LARGE;
    }
    
    umsbb_buffer_t* buffer = g_buffers[buffer_id];
    if (!buffer || !buffer->is_initialized) {
        return UMSBB_ERROR_INVALID_BUFFER;
    }
    
    // Allocate test data
    uint8_t* test_data = (uint8_t*)wasm_malloc(message_size);
    if (!test_data) {
        return UMSBB_ERROR_MEMORY_ALLOCATION;
    }
    
    // Fill test data
    for (uint32_t i = 0; i < message_size; i++) {
        test_data[i] = (uint8_t)(i & 0xFF);
    }
    
    // Record start statistics
    uint64_t start_messages = buffer->stats.total_messages_written;
    
    // Write messages
    for (uint32_t i = 0; i < message_count; i++) {
        int result = umsbb_write_message(buffer_id, test_data, message_size);
        if (result != UMSBB_SUCCESS) {
            return result;
        }
    }
    
    // Calculate messages written
    uint64_t messages_written = buffer->stats.total_messages_written - start_messages;
    
    return (int)messages_written;
}

// Memory management for WebAssembly
WASM_EXPORT void* umsbb_malloc(size_t size) {
    return wasm_malloc(size);
}

WASM_EXPORT void umsbb_free(void* ptr) {
    // Simple allocator doesn't support free
    (void)ptr;
}