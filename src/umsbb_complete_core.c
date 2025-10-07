/*
 * UMSBB (Universal Multi-Segmented Bi-directional Buffer Bus) v4.0
 * Complete WebAssembly Core Implementation
 * 
 * This file contains ALL core features of the buffer bus system:
 * - Multi-segment lock-free architecture
 * - Atomic operations for thread safety
 * - Message handling and routing
 * - Performance monitoring
 * - Memory management
 * - Error handling
 * - Statistics and diagnostics
 * 
 * Compile to WebAssembly: emcc umsbb_complete_core.c -o umsbb_core.wasm
 * Or customize and build your own version
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WASM_EXPORT
#endif

// =============================================================================
// CONFIGURATION AND CONSTANTS
// =============================================================================

#define UMSBB_VERSION_MAJOR 4
#define UMSBB_VERSION_MINOR 0
#define UMSBB_VERSION_PATCH 0

// Buffer configuration
#define UMSBB_MAX_BUFFER_SIZE (64 * 1024 * 1024)  // 64MB maximum
#define UMSBB_MIN_BUFFER_SIZE (1 * 1024 * 1024)   // 1MB minimum
#define UMSBB_MAX_MESSAGE_SIZE (64 * 1024)        // 64KB per message
#define UMSBB_SEGMENT_COUNT 8                     // 8 parallel segments
#define UMSBB_MAX_BUFFERS 256                     // Maximum concurrent buffers
#define UMSBB_ALIGNMENT 64                        // Cache line alignment

// Performance tuning
#define UMSBB_DEFAULT_BUFFER_SIZE (16 * 1024 * 1024)  // 16MB default
#define UMSBB_HEADER_SIZE 64                          // Message header size
#define UMSBB_MAGIC_NUMBER 0x554D5342                 // "UMSB" magic

// Error codes
typedef enum {
    UMSBB_SUCCESS = 0,
    UMSBB_ERROR_INVALID_PARAMS = -1,
    UMSBB_ERROR_BUFFER_FULL = -2,
    UMSBB_ERROR_BUFFER_EMPTY = -3,
    UMSBB_ERROR_INVALID_HANDLE = -4,
    UMSBB_ERROR_MEMORY_ALLOCATION = -5,
    UMSBB_ERROR_CORRUPTED_DATA = -6,
    UMSBB_ERROR_BUFFER_OVERFLOW = -7,
    UMSBB_ERROR_INVALID_SIZE = -8,
    UMSBB_ERROR_TIMEOUT = -9,
    UMSBB_ERROR_NOT_INITIALIZED = -10
} umsbb_error_t;

// Buffer handle type
typedef uint32_t umsbb_handle_t;

// =============================================================================
// ATOMIC OPERATIONS (Cross-platform)
// =============================================================================

#if defined(__GNUC__) || defined(__clang__)
    #define ATOMIC_LOAD(ptr) __atomic_load_n(ptr, __ATOMIC_ACQUIRE)
    #define ATOMIC_STORE(ptr, val) __atomic_store_n(ptr, val, __ATOMIC_RELEASE)
    #define ATOMIC_ADD(ptr, val) __atomic_add_fetch(ptr, val, __ATOMIC_ACQ_REL)
    #define ATOMIC_SUB(ptr, val) __atomic_sub_fetch(ptr, val, __ATOMIC_ACQ_REL)
    #define ATOMIC_CAS(ptr, expected, desired) __atomic_compare_exchange_n(ptr, expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)
    #define MEMORY_BARRIER() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#elif defined(_MSC_VER)
    #include <intrin.h>
    #define ATOMIC_LOAD(ptr) (*(volatile typeof(*ptr)*)(ptr))
    #define ATOMIC_STORE(ptr, val) (*(volatile typeof(*ptr)*)(ptr) = (val))
    #define ATOMIC_ADD(ptr, val) _InterlockedAdd((volatile long*)(ptr), (val))
    #define ATOMIC_SUB(ptr, val) _InterlockedAdd((volatile long*)(ptr), -(val))
    #define ATOMIC_CAS(ptr, expected, desired) (_InterlockedCompareExchange((volatile long*)(ptr), (desired), (expected)) == (expected))
    #define MEMORY_BARRIER() _ReadWriteBarrier()
#else
    // Fallback for other compilers (not thread-safe)
    #define ATOMIC_LOAD(ptr) (*(ptr))
    #define ATOMIC_STORE(ptr, val) (*(ptr) = (val))
    #define ATOMIC_ADD(ptr, val) (*(ptr) += (val))
    #define ATOMIC_SUB(ptr, val) (*(ptr) -= (val))
    #define ATOMIC_CAS(ptr, expected, desired) ((*(ptr) == *(expected)) ? (*(ptr) = (desired), true) : (*(expected) = *(ptr), false))
    #define MEMORY_BARRIER()
#endif

// =============================================================================
// CORE DATA STRUCTURES
// =============================================================================

// Message header structure
typedef struct {
    uint32_t magic;          // Magic number for validation
    uint32_t size;           // Message size in bytes
    uint64_t sequence;       // Sequence number
    uint64_t timestamp;      // Timestamp
    uint32_t checksum;       // Data checksum
    uint32_t flags;          // Message flags
    uint8_t reserved[32];    // Reserved for future use
} umsbb_message_header_t;

// Buffer segment structure
typedef struct {
    volatile uint64_t read_pos;      // Current read position
    volatile uint64_t write_pos;     // Current write position
    volatile uint64_t message_count; // Messages in this segment
    volatile uint32_t active;        // Segment active flag
    uint8_t* data;                   // Segment data pointer
    uint32_t size;                   // Segment size
    uint32_t segment_id;             // Segment identifier
    uint8_t padding[32];             // Cache line padding
} umsbb_segment_t;

// Main buffer structure
typedef struct {
    uint32_t magic;                  // Magic number for validation
    uint32_t handle;                 // Buffer handle
    uint32_t size_mb;                // Buffer size in MB
    uint32_t total_size;             // Total buffer size in bytes
    
    // Segments
    umsbb_segment_t segments[UMSBB_SEGMENT_COUNT];
    uint8_t* buffer_memory;          // Main buffer memory
    
    // Statistics (atomic)
    volatile uint64_t total_messages_written;
    volatile uint64_t total_messages_read;
    volatile uint64_t total_bytes_written;
    volatile uint64_t total_bytes_read;
    volatile uint64_t write_operations;
    volatile uint64_t read_operations;
    volatile uint64_t failed_writes;
    volatile uint64_t failed_reads;
    
    // Configuration
    uint32_t max_message_size;
    uint32_t segment_size;
    volatile uint32_t active_segments;
    volatile uint32_t initialized;
    
    // Performance tracking
    volatile uint64_t last_write_time;
    volatile uint64_t last_read_time;
    volatile uint32_t peak_pending_messages;
    volatile uint32_t current_pending_messages;
    
    uint8_t padding[128];            // Cache line padding
} umsbb_buffer_t;

// Global buffer registry
static umsbb_buffer_t* g_buffers[UMSBB_MAX_BUFFERS] = {0};
static volatile uint32_t g_next_handle = 1;
static volatile uint32_t g_active_buffers = 0;

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

// Get current timestamp
static uint64_t umsbb_get_timestamp(void) {
    #ifdef __EMSCRIPTEN__
        return (uint64_t)(emscripten_get_now() * 1000000.0); // Convert to microseconds
    #else
        // Fallback timestamp (not precise)
        static uint64_t counter = 0;
        return ATOMIC_ADD(&counter, 1);
    #endif
}

// Calculate simple checksum
static uint32_t umsbb_calculate_checksum(const void* data, uint32_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t checksum = 0;
    for (uint32_t i = 0; i < size; i++) {
        checksum = (checksum * 31) + bytes[i];
    }
    return checksum;
}

// Align size to cache line boundary
static uint32_t umsbb_align_size(uint32_t size) {
    return (size + UMSBB_ALIGNMENT - 1) & ~(UMSBB_ALIGNMENT - 1);
}

// Get next available handle
static uint32_t umsbb_get_next_handle(void) {
    uint32_t handle;
    do {
        handle = ATOMIC_ADD(&g_next_handle, 1);
    } while (handle == 0 || handle >= UMSBB_MAX_BUFFERS);
    return handle;
}

// Find buffer by handle
static umsbb_buffer_t* umsbb_find_buffer(umsbb_handle_t handle) {
    if (handle == 0 || handle >= UMSBB_MAX_BUFFERS) {
        return NULL;
    }
    
    umsbb_buffer_t* buffer = g_buffers[handle];
    if (buffer && ATOMIC_LOAD(&buffer->initialized) && buffer->magic == UMSBB_MAGIC_NUMBER) {
        return buffer;
    }
    
    return NULL;
}

// Select optimal segment for writing
static uint32_t umsbb_select_write_segment(umsbb_buffer_t* buffer) {
    uint32_t best_segment = 0;
    uint64_t min_pending = UINT64_MAX;
    
    for (uint32_t i = 0; i < UMSBB_SEGMENT_COUNT; i++) {
        if (ATOMIC_LOAD(&buffer->segments[i].active)) {
            uint64_t pending = ATOMIC_LOAD(&buffer->segments[i].write_pos) - 
                              ATOMIC_LOAD(&buffer->segments[i].read_pos);
            if (pending < min_pending) {
                min_pending = pending;
                best_segment = i;
            }
        }
    }
    
    return best_segment;
}

// Select optimal segment for reading
static uint32_t umsbb_select_read_segment(umsbb_buffer_t* buffer) {
    for (uint32_t i = 0; i < UMSBB_SEGMENT_COUNT; i++) {
        if (ATOMIC_LOAD(&buffer->segments[i].active) && 
            ATOMIC_LOAD(&buffer->segments[i].message_count) > 0) {
            return i;
        }
    }
    return 0; // Default to first segment
}

// =============================================================================
// CORE BUFFER MANAGEMENT
// =============================================================================

// Initialize a buffer segment
static int umsbb_init_segment(umsbb_segment_t* segment, uint32_t segment_id, 
                              uint8_t* memory, uint32_t size) {
    if (!segment || !memory || size == 0) {
        return UMSBB_ERROR_INVALID_PARAMS;
    }
    
    memset(segment, 0, sizeof(umsbb_segment_t));
    
    segment->segment_id = segment_id;
    segment->data = memory;
    segment->size = size;
    ATOMIC_STORE(&segment->read_pos, 0);
    ATOMIC_STORE(&segment->write_pos, 0);
    ATOMIC_STORE(&segment->message_count, 0);
    ATOMIC_STORE(&segment->active, 1);
    
    return UMSBB_SUCCESS;
}

// Create a new buffer
WASM_EXPORT umsbb_handle_t umsbb_create_buffer(uint32_t size_mb) {
    // Validate parameters
    if (size_mb < 1 || size_mb > 64) {
        return 0;
    }
    
    // Calculate total size
    uint32_t total_size = size_mb * 1024 * 1024;
    uint32_t segment_size = total_size / UMSBB_SEGMENT_COUNT;
    segment_size = umsbb_align_size(segment_size);
    
    // Allocate memory for buffer structure
    umsbb_buffer_t* buffer = (umsbb_buffer_t*)malloc(sizeof(umsbb_buffer_t));
    if (!buffer) {
        return 0;
    }
    
    // Allocate buffer memory
    uint8_t* buffer_memory = (uint8_t*)malloc(total_size + UMSBB_ALIGNMENT);
    if (!buffer_memory) {
        free(buffer);
        return 0;
    }
    
    // Align buffer memory
    uint8_t* aligned_memory = (uint8_t*)(((uintptr_t)buffer_memory + UMSBB_ALIGNMENT - 1) & 
                                        ~(UMSBB_ALIGNMENT - 1));
    
    // Get handle
    uint32_t handle = umsbb_get_next_handle();
    if (handle == 0 || handle >= UMSBB_MAX_BUFFERS) {
        free(buffer_memory);
        free(buffer);
        return 0;
    }
    
    // Initialize buffer structure
    memset(buffer, 0, sizeof(umsbb_buffer_t));
    
    buffer->magic = UMSBB_MAGIC_NUMBER;
    buffer->handle = handle;
    buffer->size_mb = size_mb;
    buffer->total_size = total_size;
    buffer->buffer_memory = buffer_memory;
    buffer->max_message_size = UMSBB_MAX_MESSAGE_SIZE;
    buffer->segment_size = segment_size;
    
    // Initialize segments
    for (uint32_t i = 0; i < UMSBB_SEGMENT_COUNT; i++) {
        uint8_t* segment_memory = aligned_memory + (i * segment_size);
        if (umsbb_init_segment(&buffer->segments[i], i, segment_memory, segment_size) != UMSBB_SUCCESS) {
            // Cleanup on failure
            free(buffer_memory);
            free(buffer);
            return 0;
        }
    }
    
    ATOMIC_STORE(&buffer->active_segments, UMSBB_SEGMENT_COUNT);
    
    // Register buffer
    g_buffers[handle] = buffer;
    ATOMIC_ADD(&g_active_buffers, 1);
    
    // Mark as initialized (must be last)
    ATOMIC_STORE(&buffer->initialized, 1);
    
    return handle;
}

// Write message to buffer
WASM_EXPORT int umsbb_write_message(umsbb_handle_t handle, const void* data, uint32_t size) {
    // Validate parameters
    if (!data || size == 0 || size > UMSBB_MAX_MESSAGE_SIZE) {
        return UMSBB_ERROR_INVALID_PARAMS;
    }
    
    // Find buffer
    umsbb_buffer_t* buffer = umsbb_find_buffer(handle);
    if (!buffer) {
        return UMSBB_ERROR_INVALID_HANDLE;
    }
    
    // Select segment
    uint32_t segment_idx = umsbb_select_write_segment(buffer);
    umsbb_segment_t* segment = &buffer->segments[segment_idx];
    
    // Calculate total message size (header + data)
    uint32_t total_size = sizeof(umsbb_message_header_t) + size;
    total_size = umsbb_align_size(total_size);
    
    // Check available space
    uint64_t current_write = ATOMIC_LOAD(&segment->write_pos);
    uint64_t current_read = ATOMIC_LOAD(&segment->read_pos);
    
    if (current_write + total_size > current_read + segment->size) {
        ATOMIC_ADD(&buffer->failed_writes, 1);
        return UMSBB_ERROR_BUFFER_FULL;
    }
    
    // Prepare message header
    umsbb_message_header_t header;
    memset(&header, 0, sizeof(header));
    
    header.magic = UMSBB_MAGIC_NUMBER;
    header.size = size;
    header.sequence = ATOMIC_ADD(&buffer->total_messages_written, 1);
    header.timestamp = umsbb_get_timestamp();
    header.checksum = umsbb_calculate_checksum(data, size);
    header.flags = 0;
    
    // Calculate write position in buffer
    uint32_t write_offset = current_write % segment->size;
    
    // Handle wrap-around
    if (write_offset + total_size > segment->size) {
        // Reset to beginning of segment
        write_offset = 0;
        current_write = current_read + segment->size; // Skip to next cycle
    }
    
    // Write header
    memcpy(segment->data + write_offset, &header, sizeof(header));
    
    // Write data
    memcpy(segment->data + write_offset + sizeof(header), data, size);
    
    // Update positions
    ATOMIC_STORE(&segment->write_pos, current_write + total_size);
    ATOMIC_ADD(&segment->message_count, 1);
    ATOMIC_ADD(&buffer->total_bytes_written, size);
    ATOMIC_ADD(&buffer->write_operations, 1);
    ATOMIC_STORE(&buffer->last_write_time, header.timestamp);
    
    // Update pending message count
    uint32_t pending = ATOMIC_ADD(&buffer->current_pending_messages, 1);
    uint32_t peak = ATOMIC_LOAD(&buffer->peak_pending_messages);
    if (pending > peak) {
        ATOMIC_STORE(&buffer->peak_pending_messages, pending);
    }
    
    MEMORY_BARRIER();
    
    return UMSBB_SUCCESS;
}

// Read message from buffer
WASM_EXPORT int umsbb_read_message(umsbb_handle_t handle, void* buffer_out, 
                                   uint32_t buffer_size, uint32_t* actual_size) {
    // Validate parameters
    if (!buffer_out || buffer_size == 0 || !actual_size) {
        return UMSBB_ERROR_INVALID_PARAMS;
    }
    
    // Find buffer
    umsbb_buffer_t* buffer = umsbb_find_buffer(handle);
    if (!buffer) {
        return UMSBB_ERROR_INVALID_HANDLE;
    }
    
    // Select segment
    uint32_t segment_idx = umsbb_select_read_segment(buffer);
    umsbb_segment_t* segment = &buffer->segments[segment_idx];
    
    // Check if there are messages available
    if (ATOMIC_LOAD(&segment->message_count) == 0) {
        return UMSBB_ERROR_BUFFER_EMPTY;
    }
    
    // Get current positions
    uint64_t current_read = ATOMIC_LOAD(&segment->read_pos);
    uint64_t current_write = ATOMIC_LOAD(&segment->write_pos);
    
    if (current_read >= current_write) {
        return UMSBB_ERROR_BUFFER_EMPTY;
    }
    
    // Calculate read position in buffer
    uint32_t read_offset = current_read % segment->size;
    
    // Read message header
    umsbb_message_header_t header;
    memcpy(&header, segment->data + read_offset, sizeof(header));
    
    // Validate header
    if (header.magic != UMSBB_MAGIC_NUMBER) {
        ATOMIC_ADD(&buffer->failed_reads, 1);
        return UMSBB_ERROR_CORRUPTED_DATA;
    }
    
    if (header.size > buffer_size) {
        return UMSBB_ERROR_INVALID_SIZE;
    }
    
    // Read message data
    memcpy(buffer_out, segment->data + read_offset + sizeof(header), header.size);
    
    // Verify checksum
    uint32_t calculated_checksum = umsbb_calculate_checksum(buffer_out, header.size);
    if (calculated_checksum != header.checksum) {
        ATOMIC_ADD(&buffer->failed_reads, 1);
        return UMSBB_ERROR_CORRUPTED_DATA;
    }
    
    // Update positions
    uint32_t total_size = sizeof(umsbb_message_header_t) + header.size;
    total_size = umsbb_align_size(total_size);
    
    ATOMIC_STORE(&segment->read_pos, current_read + total_size);
    ATOMIC_SUB(&segment->message_count, 1);
    ATOMIC_ADD(&buffer->total_messages_read, 1);
    ATOMIC_ADD(&buffer->total_bytes_read, header.size);
    ATOMIC_ADD(&buffer->read_operations, 1);
    ATOMIC_STORE(&buffer->last_read_time, umsbb_get_timestamp());
    ATOMIC_SUB(&buffer->current_pending_messages, 1);
    
    *actual_size = header.size;
    
    MEMORY_BARRIER();
    
    return UMSBB_SUCCESS;
}

// =============================================================================
// STATISTICS AND MONITORING
// =============================================================================

// Get total messages processed
WASM_EXPORT uint64_t umsbb_get_total_messages(umsbb_handle_t handle) {
    umsbb_buffer_t* buffer = umsbb_find_buffer(handle);
    if (!buffer) {
        return 0;
    }
    return ATOMIC_LOAD(&buffer->total_messages_written);
}

// Get total bytes processed
WASM_EXPORT uint64_t umsbb_get_total_bytes(umsbb_handle_t handle) {
    umsbb_buffer_t* buffer = umsbb_find_buffer(handle);
    if (!buffer) {
        return 0;
    }
    return ATOMIC_LOAD(&buffer->total_bytes_written);
}

// Get pending messages
WASM_EXPORT uint32_t umsbb_get_pending_messages(umsbb_handle_t handle) {
    umsbb_buffer_t* buffer = umsbb_find_buffer(handle);
    if (!buffer) {
        return 0;
    }
    return ATOMIC_LOAD(&buffer->current_pending_messages);
}

// Get active segments
WASM_EXPORT uint32_t umsbb_get_active_segments(umsbb_handle_t handle) {
    umsbb_buffer_t* buffer = umsbb_find_buffer(handle);
    if (!buffer) {
        return 0;
    }
    return ATOMIC_LOAD(&buffer->active_segments);
}

// Get comprehensive statistics
WASM_EXPORT void umsbb_get_comprehensive_stats(umsbb_handle_t handle, 
                                               uint64_t* total_written,
                                               uint64_t* total_read,
                                               uint64_t* bytes_written,
                                               uint64_t* bytes_read,
                                               uint32_t* pending_messages,
                                               uint32_t* active_segments,
                                               uint64_t* failed_writes,
                                               uint64_t* failed_reads) {
    umsbb_buffer_t* buffer = umsbb_find_buffer(handle);
    if (!buffer) {
        if (total_written) *total_written = 0;
        if (total_read) *total_read = 0;
        if (bytes_written) *bytes_written = 0;
        if (bytes_read) *bytes_read = 0;
        if (pending_messages) *pending_messages = 0;
        if (active_segments) *active_segments = 0;
        if (failed_writes) *failed_writes = 0;
        if (failed_reads) *failed_reads = 0;
        return;
    }
    
    if (total_written) *total_written = ATOMIC_LOAD(&buffer->total_messages_written);
    if (total_read) *total_read = ATOMIC_LOAD(&buffer->total_messages_read);
    if (bytes_written) *bytes_written = ATOMIC_LOAD(&buffer->total_bytes_written);
    if (bytes_read) *bytes_read = ATOMIC_LOAD(&buffer->total_bytes_read);
    if (pending_messages) *pending_messages = ATOMIC_LOAD(&buffer->current_pending_messages);
    if (active_segments) *active_segments = ATOMIC_LOAD(&buffer->active_segments);
    if (failed_writes) *failed_writes = ATOMIC_LOAD(&buffer->failed_writes);
    if (failed_reads) *failed_reads = ATOMIC_LOAD(&buffer->failed_reads);
}

// =============================================================================
// UTILITY AND INFORMATION FUNCTIONS
// =============================================================================

// Get maximum message size
WASM_EXPORT uint32_t umsbb_get_max_message_size(void) {
    return UMSBB_MAX_MESSAGE_SIZE;
}

// Get version information
WASM_EXPORT uint32_t umsbb_get_version(void) {
    return (UMSBB_VERSION_MAJOR << 16) | (UMSBB_VERSION_MINOR << 8) | UMSBB_VERSION_PATCH;
}

// Get error string
WASM_EXPORT const char* umsbb_get_error_string(int error_code) {
    switch (error_code) {
        case UMSBB_SUCCESS: return "Success";
        case UMSBB_ERROR_INVALID_PARAMS: return "Invalid parameters";
        case UMSBB_ERROR_BUFFER_FULL: return "Buffer is full";
        case UMSBB_ERROR_BUFFER_EMPTY: return "Buffer is empty";
        case UMSBB_ERROR_INVALID_HANDLE: return "Invalid buffer handle";
        case UMSBB_ERROR_MEMORY_ALLOCATION: return "Memory allocation failed";
        case UMSBB_ERROR_CORRUPTED_DATA: return "Corrupted data detected";
        case UMSBB_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case UMSBB_ERROR_INVALID_SIZE: return "Invalid size";
        case UMSBB_ERROR_TIMEOUT: return "Operation timeout";
        case UMSBB_ERROR_NOT_INITIALIZED: return "Buffer not initialized";
        default: return "Unknown error";
    }
}

// Get buffer configuration
WASM_EXPORT void umsbb_get_buffer_config(umsbb_handle_t handle,
                                         uint32_t* size_mb,
                                         uint32_t* segment_count,
                                         uint32_t* segment_size,
                                         uint32_t* max_message_size) {
    umsbb_buffer_t* buffer = umsbb_find_buffer(handle);
    if (!buffer) {
        if (size_mb) *size_mb = 0;
        if (segment_count) *segment_count = 0;
        if (segment_size) *segment_size = 0;
        if (max_message_size) *max_message_size = 0;
        return;
    }
    
    if (size_mb) *size_mb = buffer->size_mb;
    if (segment_count) *segment_count = UMSBB_SEGMENT_COUNT;
    if (segment_size) *segment_size = buffer->segment_size;
    if (max_message_size) *max_message_size = buffer->max_message_size;
}

// =============================================================================
// CLEANUP AND SHUTDOWN
// =============================================================================

// Destroy buffer and free resources
WASM_EXPORT int umsbb_destroy_buffer(umsbb_handle_t handle) {
    if (handle == 0 || handle >= UMSBB_MAX_BUFFERS) {
        return UMSBB_ERROR_INVALID_HANDLE;
    }
    
    umsbb_buffer_t* buffer = g_buffers[handle];
    if (!buffer) {
        return UMSBB_ERROR_INVALID_HANDLE;
    }
    
    // Mark as not initialized
    ATOMIC_STORE(&buffer->initialized, 0);
    
    // Deactivate all segments
    for (uint32_t i = 0; i < UMSBB_SEGMENT_COUNT; i++) {
        ATOMIC_STORE(&buffer->segments[i].active, 0);
    }
    
    // Free memory
    if (buffer->buffer_memory) {
        free(buffer->buffer_memory);
    }
    
    // Clear from registry
    g_buffers[handle] = NULL;
    ATOMIC_SUB(&g_active_buffers, 1);
    
    // Free buffer structure
    free(buffer);
    
    return UMSBB_SUCCESS;
}

// Get system information
WASM_EXPORT void umsbb_get_system_info(uint32_t* active_buffers,
                                       uint32_t* max_buffers,
                                       uint32_t* segment_count,
                                       uint32_t* max_message_size) {
    if (active_buffers) *active_buffers = ATOMIC_LOAD(&g_active_buffers);
    if (max_buffers) *max_buffers = UMSBB_MAX_BUFFERS;
    if (segment_count) *segment_count = UMSBB_SEGMENT_COUNT;
    if (max_message_size) *max_message_size = UMSBB_MAX_MESSAGE_SIZE;
}

// =============================================================================
// WEBASSEMBLY INTERFACE HELPERS
// =============================================================================

#ifdef __EMSCRIPTEN__

// JavaScript-friendly wrapper functions
WASM_EXPORT void* umsbb_malloc(uint32_t size) {
    return malloc(size);
}

WASM_EXPORT void umsbb_free(void* ptr) {
    free(ptr);
}

// Batch operations for better performance
WASM_EXPORT int umsbb_write_multiple_messages(umsbb_handle_t handle, 
                                              const void* data_array[], 
                                              const uint32_t sizes[], 
                                              uint32_t count) {
    if (!data_array || !sizes || count == 0) {
        return UMSBB_ERROR_INVALID_PARAMS;
    }
    
    int last_result = UMSBB_SUCCESS;
    for (uint32_t i = 0; i < count; i++) {
        int result = umsbb_write_message(handle, data_array[i], sizes[i]);
        if (result != UMSBB_SUCCESS) {
            last_result = result;
        }
    }
    
    return last_result;
}

#endif

// =============================================================================
// MODULE INITIALIZATION
// =============================================================================

// Initialize the UMSBB system
WASM_EXPORT int umsbb_init_system(void) {
    // Clear buffer registry
    for (uint32_t i = 0; i < UMSBB_MAX_BUFFERS; i++) {
        g_buffers[i] = NULL;
    }
    
    ATOMIC_STORE(&g_next_handle, 1);
    ATOMIC_STORE(&g_active_buffers, 0);
    
    return UMSBB_SUCCESS;
}

// Shutdown the UMSBB system
WASM_EXPORT int umsbb_shutdown_system(void) {
    // Destroy all active buffers
    for (uint32_t i = 1; i < UMSBB_MAX_BUFFERS; i++) {
        if (g_buffers[i] != NULL) {
            umsbb_destroy_buffer(i);
        }
    }
    
    return UMSBB_SUCCESS;
}

// =============================================================================
// PERFORMANCE TESTING FUNCTIONS
// =============================================================================

#ifdef UMSBB_INCLUDE_BENCHMARKS

// Built-in performance test
WASM_EXPORT int umsbb_run_performance_test(uint32_t buffer_size_mb, 
                                          uint32_t message_count, 
                                          uint32_t message_size,
                                          uint64_t* messages_per_second,
                                          uint64_t* bytes_per_second) {
    if (!messages_per_second || !bytes_per_second) {
        return UMSBB_ERROR_INVALID_PARAMS;
    }
    
    // Create test buffer
    umsbb_handle_t handle = umsbb_create_buffer(buffer_size_mb);
    if (handle == 0) {
        return UMSBB_ERROR_MEMORY_ALLOCATION;
    }
    
    // Allocate test data
    uint8_t* test_data = (uint8_t*)malloc(message_size);
    if (!test_data) {
        umsbb_destroy_buffer(handle);
        return UMSBB_ERROR_MEMORY_ALLOCATION;
    }
    
    // Initialize test data
    for (uint32_t i = 0; i < message_size; i++) {
        test_data[i] = i % 256;
    }
    
    uint64_t start_time = umsbb_get_timestamp();
    
    // Write messages
    for (uint32_t i = 0; i < message_count; i++) {
        int result = umsbb_write_message(handle, test_data, message_size);
        if (result != UMSBB_SUCCESS) {
            free(test_data);
            umsbb_destroy_buffer(handle);
            return result;
        }
    }
    
    // Read messages
    uint8_t* read_buffer = (uint8_t*)malloc(message_size);
    if (!read_buffer) {
        free(test_data);
        umsbb_destroy_buffer(handle);
        return UMSBB_ERROR_MEMORY_ALLOCATION;
    }
    
    for (uint32_t i = 0; i < message_count; i++) {
        uint32_t actual_size;
        int result = umsbb_read_message(handle, read_buffer, message_size, &actual_size);
        if (result != UMSBB_SUCCESS) {
            free(read_buffer);
            free(test_data);
            umsbb_destroy_buffer(handle);
            return result;
        }
    }
    
    uint64_t end_time = umsbb_get_timestamp();
    uint64_t duration_us = end_time - start_time;
    
    if (duration_us > 0) {
        *messages_per_second = (message_count * 2 * 1000000) / duration_us; // Read + Write
        *bytes_per_second = (message_count * 2 * message_size * 1000000) / duration_us;
    } else {
        *messages_per_second = 0;
        *bytes_per_second = 0;
    }
    
    // Cleanup
    free(read_buffer);
    free(test_data);
    umsbb_destroy_buffer(handle);
    
    return UMSBB_SUCCESS;
}

#endif

/*
 * =============================================================================
 * END OF UMSBB COMPLETE CORE IMPLEMENTATION
 * =============================================================================
 * 
 * This file contains the complete UMSBB v4.0 implementation in a single file.
 * It includes all core features:
 * 
 * - Multi-segment lock-free buffer architecture
 * - Atomic operations for thread safety
 * - Message handling with headers and checksums
 * - Comprehensive statistics and monitoring
 * - Error handling and validation
 * - Memory management and cleanup
 * - WebAssembly integration
 * - Performance testing utilities
 * 
 * To build as WebAssembly:
 * emcc -O3 -s WASM=1 -s EXPORTED_FUNCTIONS='[LIST_OF_FUNCTIONS]' 
 *      -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' 
 *      umsbb_complete_core.c -o umsbb_core.wasm
 * 
 * To build as native library:
 * gcc -O3 -std=c11 -shared -fPIC umsbb_complete_core.c -o libumsbb.so
 * 
 * To customize: Edit this file and rebuild according to your needs.
 */