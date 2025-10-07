#ifndef GPU_ACCELERATED_BUFFER_H
#define GPU_ACCELERATED_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#ifndef __MINGW32__
#include <intrin.h>
#endif
#else
#include <pthread.h>
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// GPU acceleration support
#ifdef ENABLE_CUDA
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <device_launch_parameters.h>
#endif

#ifdef ENABLE_OPENCL
    // Try to include OpenCL headers, with fallback for missing headers
    #ifdef _WIN32
        // On Windows, define minimal types if headers unavailable
        typedef void* cl_context;
        typedef void* cl_device_id;
        typedef void* cl_command_queue;
        typedef void* cl_mem;
        typedef void* cl_program;
        typedef void* cl_kernel;
        typedef int cl_int;
        #define CL_SUCCESS 0
        #define CL_DEVICE_TYPE_GPU 4
        #define CL_MEM_READ_WRITE (1 << 0)
        
        // Try to include the actual header if available
        #ifndef OPENCL_HEADERS_MISSING
            // This will be defined by build system if headers are missing
            #ifdef __MINGW32__
                // MinGW might not have OpenCL headers
                #define OPENCL_HEADERS_MISSING
            #endif
        #endif
        
        #ifndef OPENCL_HEADERS_MISSING
            #include <CL/cl.h>
        #endif
    #else
        #include <CL/cl.h>
    #endif
#endif

// Performance targets
#define TARGET_THROUGHPUT_GBPS 10.0     // 10 GB/s target
#define TARGET_MESSAGES_PER_SEC 1000000000ULL  // 1 billion messages/sec
#define MAX_GPU_STREAMS 32
#define GPU_BATCH_SIZE 65536
#define GPU_MEMORY_POOL_SIZE (1ULL << 30)  // 1GB GPU memory pool

// GPU processing types
typedef enum {
    GPU_PROCESSING_DISABLED = 0,
    GPU_PROCESSING_CUDA = 1,
    GPU_PROCESSING_OPENCL = 2,
    GPU_PROCESSING_HYBRID = 3
} gpu_processing_type_t;

// GPU memory management
typedef struct {
    void* device_ptr;
    void* host_ptr;
    size_t size;
    bool is_pinned;
    uint32_t stream_id;
} gpu_memory_block_t;

// GPU stream management
typedef struct {
    uint32_t stream_id;
    void* stream_handle;
    bool is_active;
    uint64_t messages_processed;
    double throughput_gbps;
    uint64_t last_update_time;
} gpu_stream_t;

// GPU accelerated ring buffer
typedef struct {
    // Host-side ring buffer
    uint8_t* host_buffer;
    volatile uint64_t head;
    volatile uint64_t tail;
    uint64_t capacity;
    uint64_t mask;
    
    // GPU-side acceleration
    gpu_memory_block_t* gpu_blocks;
    gpu_stream_t* gpu_streams;
    uint32_t num_streams;
    gpu_processing_type_t gpu_type;
    
    // Performance monitoring
    volatile uint64_t total_messages;
    volatile uint64_t total_bytes;
    volatile double current_throughput_gbps;
    volatile uint64_t gpu_operations_count;
    
    // GPU-specific handles
#ifdef ENABLE_CUDA
    cudaStream_t* cuda_streams;
    cublasHandle_t cublas_handle;
    cudaEvent_t* events;
#endif
    
#ifdef ENABLE_OPENCL
    cl_context cl_context;
    cl_device_id cl_device;
    cl_command_queue* cl_queues;
    cl_program cl_program;
    cl_kernel cl_kernel;
#endif
    
    // Memory pools
    gpu_memory_block_t* memory_pool;
    uint32_t pool_size;
    uint32_t pool_allocated;
    
    // Synchronization
    volatile bool gpu_initialized;
    volatile bool processing_active;
    
} gpu_accelerated_buffer_t;

// GPU batch processing structure
typedef struct {
    uint8_t* data;
    uint64_t* offsets;
    uint32_t* lengths;
    uint32_t* lane_ids;
    uint32_t count;
    uint64_t total_size;
    uint64_t batch_id;
    uint64_t timestamp;
} gpu_batch_t;

// GPU performance metrics
typedef struct {
    uint64_t messages_per_second;
    double throughput_gbps;
    double gpu_utilization;
    uint32_t active_streams;
    uint64_t total_gpu_memory_used;
    double memory_bandwidth_utilization;
    uint64_t cuda_kernel_launches;
    double average_kernel_time_ms;
    uint64_t memory_transfers_host_to_device;
    uint64_t memory_transfers_device_to_host;
    double pcie_bandwidth_utilization;
} gpu_performance_metrics_t;

// Function declarations

// GPU initialization and cleanup
int gpu_buffer_init(gpu_accelerated_buffer_t* buffer, uint64_t capacity, gpu_processing_type_t type);
void gpu_buffer_cleanup(gpu_accelerated_buffer_t* buffer);
bool gpu_buffer_is_available(gpu_processing_type_t type);

// GPU memory management
gpu_memory_block_t* gpu_allocate_pinned_memory(size_t size);
void gpu_free_pinned_memory(gpu_memory_block_t* block);
int gpu_transfer_to_device(gpu_memory_block_t* block, const void* host_data, size_t size);
int gpu_transfer_from_device(gpu_memory_block_t* block, void* host_data, size_t size);

// GPU stream management
int gpu_create_streams(gpu_accelerated_buffer_t* buffer, uint32_t num_streams);
void gpu_destroy_streams(gpu_accelerated_buffer_t* buffer);
gpu_stream_t* gpu_get_available_stream(gpu_accelerated_buffer_t* buffer);

// GPU batch processing
int gpu_process_batch(gpu_accelerated_buffer_t* buffer, gpu_batch_t* batch);
int gpu_prepare_batch(gpu_accelerated_buffer_t* buffer, const void* data, size_t size, gpu_batch_t* batch);
int gpu_execute_parallel_processing(gpu_accelerated_buffer_t* buffer, gpu_batch_t* batch);

// High-level GPU operations
int gpu_buffer_write_accelerated(gpu_accelerated_buffer_t* buffer, const void* data, size_t size, uint32_t lane_id);
int gpu_buffer_read_accelerated(gpu_accelerated_buffer_t* buffer, void* data, size_t max_size, size_t* actual_size);
int gpu_buffer_bulk_transfer(gpu_accelerated_buffer_t* buffer, const void* data, size_t size, uint32_t num_parallel_streams);

// Performance monitoring
void gpu_update_performance_metrics(gpu_accelerated_buffer_t* buffer, gpu_performance_metrics_t* metrics);
double gpu_calculate_throughput(gpu_accelerated_buffer_t* buffer);
void gpu_reset_performance_counters(gpu_accelerated_buffer_t* buffer);

// CUDA-specific functions
#ifdef ENABLE_CUDA
int cuda_initialize_device(gpu_accelerated_buffer_t* buffer);
int cuda_create_streams(gpu_accelerated_buffer_t* buffer, uint32_t num_streams);
int cuda_launch_processing_kernel(gpu_accelerated_buffer_t* buffer, gpu_batch_t* batch, uint32_t stream_id);
int cuda_synchronize_streams(gpu_accelerated_buffer_t* buffer);
#endif

// OpenCL-specific functions
#ifdef ENABLE_OPENCL
int opencl_initialize_device(gpu_accelerated_buffer_t* buffer);
int opencl_create_command_queues(gpu_accelerated_buffer_t* buffer, uint32_t num_queues);
int opencl_execute_kernel(gpu_accelerated_buffer_t* buffer, gpu_batch_t* batch, uint32_t queue_id);
int opencl_finish_queues(gpu_accelerated_buffer_t* buffer);
#endif

// Utility functions
static inline uint64_t gpu_get_timestamp_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

static inline bool gpu_is_power_of_two(uint64_t value) {
    return (value != 0) && ((value & (value - 1)) == 0);
}

static inline uint32_t gpu_hash_lane_selection(const void* data, size_t size) {
    uint32_t hash = 2166136261U;
    const uint8_t* bytes = (const uint8_t*)data;
    for (size_t i = 0; i < size && i < 64; i++) {
        hash ^= bytes[i];
        hash *= 16777619U;
    }
    return hash % 4; // 4 lanes: EXPRESS, PRIORITY, STREAMING, BULK
}

// Inline performance functions
static inline void gpu_atomic_add_uint64(volatile uint64_t* target, uint64_t value) {
#ifdef _WIN32
#ifndef __MINGW32__
    _InterlockedExchangeAdd64((volatile LONG64*)target, (LONG64)value);
#else
    __atomic_add_fetch(target, value, __ATOMIC_RELAXED);
#endif
#else
    __atomic_add_fetch(target, value, __ATOMIC_RELAXED);
#endif
}

static inline void gpu_atomic_store_double(volatile double* target, double value) {
    union { double d; uint64_t u; } cast;
    cast.d = value;
#ifdef _WIN32
#ifndef __MINGW32__
    _InterlockedExchange64((volatile LONG64*)target, (LONG64)cast.u);
#else
    __atomic_store_n((uint64_t*)target, cast.u, __ATOMIC_RELAXED);
#endif
#else
    __atomic_store_n((uint64_t*)target, cast.u, __ATOMIC_RELAXED);
#endif
}

#ifdef __cplusplus
}
#endif

#endif // GPU_ACCELERATED_BUFFER_H