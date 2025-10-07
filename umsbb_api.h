/*
 * UMSBB C API Header - For external use
 * Provides clean interface to the complete core
 */

#ifndef UMSBB_API_H
#define UMSBB_API_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Version information
#define UMSBB_VERSION_MAJOR 4
#define UMSBB_VERSION_MINOR 0
#define UMSBB_VERSION_PATCH 0

// Configuration constants
#define UMSBB_MAX_BUFFER_SIZE (64 * 1024 * 1024)  // 64MB
#define UMSBB_MIN_BUFFER_SIZE (1 * 1024 * 1024)   // 1MB
#define UMSBB_MAX_MESSAGE_SIZE (64 * 1024)        // 64KB
#define UMSBB_SEGMENT_COUNT 8

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
// CORE API FUNCTIONS
// =============================================================================

/**
 * Initialize the UMSBB system
 * @return Error code
 */
int umsbb_init_system(void);

/**
 * Shutdown the UMSBB system
 * @return Error code
 */
int umsbb_shutdown_system(void);

/**
 * Create a new UMSBB buffer
 * @param size_mb Buffer size in megabytes (1-64)
 * @return Buffer handle (0 = error)
 */
umsbb_handle_t umsbb_create_buffer(uint32_t size_mb);

/**
 * Write a message to the buffer
 * @param handle Buffer handle
 * @param data Message data
 * @param size Message size in bytes
 * @return Error code
 */
int umsbb_write_message(umsbb_handle_t handle, const void* data, uint32_t size);

/**
 * Read a message from the buffer
 * @param handle Buffer handle
 * @param buffer Output buffer
 * @param buffer_size Output buffer size
 * @param actual_size Actual message size read
 * @return Error code
 */
int umsbb_read_message(umsbb_handle_t handle, void* buffer, uint32_t buffer_size, uint32_t* actual_size);

/**
 * Destroy a buffer and free resources
 * @param handle Buffer handle
 * @return Error code
 */
int umsbb_destroy_buffer(umsbb_handle_t handle);

// =============================================================================
// STATISTICS FUNCTIONS
// =============================================================================

/**
 * Get total messages processed
 * @param handle Buffer handle
 * @return Total message count
 */
uint64_t umsbb_get_total_messages(umsbb_handle_t handle);

/**
 * Get total bytes processed
 * @param handle Buffer handle
 * @return Total byte count
 */
uint64_t umsbb_get_total_bytes(umsbb_handle_t handle);

/**
 * Get pending messages in buffer
 * @param handle Buffer handle
 * @return Pending message count
 */
uint32_t umsbb_get_pending_messages(umsbb_handle_t handle);

/**
 * Get active segments
 * @param handle Buffer handle
 * @return Active segment count
 */
uint32_t umsbb_get_active_segments(umsbb_handle_t handle);

/**
 * Get comprehensive buffer statistics
 * @param handle Buffer handle
 * @param total_written Total messages written
 * @param total_read Total messages read
 * @param bytes_written Total bytes written
 * @param bytes_read Total bytes read
 * @param pending_messages Messages currently in buffer
 * @param active_segments Number of active segments
 * @param failed_writes Number of failed write operations
 * @param failed_reads Number of failed read operations
 */
void umsbb_get_comprehensive_stats(umsbb_handle_t handle, 
                                   uint64_t* total_written,
                                   uint64_t* total_read,
                                   uint64_t* bytes_written,
                                   uint64_t* bytes_read,
                                   uint32_t* pending_messages,
                                   uint32_t* active_segments,
                                   uint64_t* failed_writes,
                                   uint64_t* failed_reads);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * Get maximum message size
 * @return Maximum message size in bytes
 */
uint32_t umsbb_get_max_message_size(void);

/**
 * Get version information
 * @return Version as packed integer (major << 16 | minor << 8 | patch)
 */
uint32_t umsbb_get_version(void);

/**
 * Get human-readable error string
 * @param error_code Error code
 * @return Error description string
 */
const char* umsbb_get_error_string(int error_code);

/**
 * Get buffer configuration
 * @param handle Buffer handle
 * @param size_mb Buffer size in MB
 * @param segment_count Number of segments
 * @param segment_size Size of each segment
 * @param max_message_size Maximum message size
 */
void umsbb_get_buffer_config(umsbb_handle_t handle,
                             uint32_t* size_mb,
                             uint32_t* segment_count,
                             uint32_t* segment_size,
                             uint32_t* max_message_size);

/**
 * Get system information
 * @param active_buffers Number of active buffers
 * @param max_buffers Maximum number of buffers
 * @param segment_count Number of segments per buffer
 * @param max_message_size Maximum message size
 */
void umsbb_get_system_info(uint32_t* active_buffers,
                           uint32_t* max_buffers,
                           uint32_t* segment_count,
                           uint32_t* max_message_size);

// =============================================================================
// PERFORMANCE TESTING (Optional)
// =============================================================================

#ifdef UMSBB_INCLUDE_BENCHMARKS
/**
 * Run built-in performance test
 * @param buffer_size_mb Buffer size for test
 * @param message_count Number of messages to test
 * @param message_size Size of each message
 * @param messages_per_second Output: messages per second
 * @param bytes_per_second Output: bytes per second
 * @return Error code
 */
int umsbb_run_performance_test(uint32_t buffer_size_mb, 
                              uint32_t message_count, 
                              uint32_t message_size,
                              uint64_t* messages_per_second,
                              uint64_t* bytes_per_second);
#endif

// =============================================================================
// WEBASSEMBLY HELPERS
// =============================================================================

#ifdef __EMSCRIPTEN__
/**
 * Allocate memory (WebAssembly helper)
 * @param size Size to allocate
 * @return Pointer to allocated memory
 */
void* umsbb_malloc(uint32_t size);

/**
 * Free memory (WebAssembly helper)
 * @param ptr Pointer to free
 */
void umsbb_free(void* ptr);

/**
 * Write multiple messages in batch
 * @param handle Buffer handle
 * @param data_array Array of data pointers
 * @param sizes Array of message sizes
 * @param count Number of messages
 * @return Error code
 */
int umsbb_write_multiple_messages(umsbb_handle_t handle, 
                                  const void* data_array[], 
                                  const uint32_t sizes[], 
                                  uint32_t count);
#endif

#ifdef __cplusplus
}
#endif

#endif /* UMSBB_API_H */