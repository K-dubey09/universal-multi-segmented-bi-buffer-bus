#include "gpu_accelerated_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#else
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#endif

// GPU device detection and initialization
bool gpu_buffer_is_available(gpu_processing_type_t type) {
    switch (type) {
        case GPU_PROCESSING_CUDA:
#ifdef ENABLE_CUDA
            int device_count = 0;
            cudaError_t error = cudaGetDeviceCount(&device_count);
            return (error == cudaSuccess && device_count > 0);
#else
            return false;
#endif
        
        case GPU_PROCESSING_OPENCL:
#ifdef ENABLE_OPENCL
            cl_uint platform_count = 0;
            cl_int error = clGetPlatformIDs(0, NULL, &platform_count);
            return (error == CL_SUCCESS && platform_count > 0);
#else
            return false;
#endif
        
        case GPU_PROCESSING_HYBRID:
            return gpu_buffer_is_available(GPU_PROCESSING_CUDA) || 
                   gpu_buffer_is_available(GPU_PROCESSING_OPENCL);
        
        default:
            return false;
    }
}

// GPU buffer initialization
int gpu_buffer_init(gpu_accelerated_buffer_t* buffer, uint64_t capacity, gpu_processing_type_t type) {
    if (!buffer || capacity == 0 || !gpu_is_power_of_two(capacity)) {
        return -1;
    }
    
    // Initialize basic structure
    memset(buffer, 0, sizeof(gpu_accelerated_buffer_t));
    buffer->capacity = capacity;
    buffer->mask = capacity - 1;
    buffer->gpu_type = type;
    
    // Allocate host-side ring buffer with page-aligned memory
    size_t buffer_size = capacity * sizeof(uint8_t);
    
#ifdef _WIN32
    buffer->host_buffer = (uint8_t*)VirtualAlloc(NULL, buffer_size, 
                                                MEM_COMMIT | MEM_RESERVE, 
                                                PAGE_READWRITE);
#else
    if (posix_memalign((void**)&buffer->host_buffer, 4096, buffer_size) != 0) {
        buffer->host_buffer = NULL;
    }
#endif
    
    if (!buffer->host_buffer) {
        printf("Failed to allocate host buffer of size %zu bytes\n", buffer_size);
        return -1;
    }
    
    // Initialize GPU processing if available
    if (type != GPU_PROCESSING_DISABLED && gpu_buffer_is_available(type)) {
        // Create GPU streams for parallel processing
        if (gpu_create_streams(buffer, MAX_GPU_STREAMS) != 0) {
            printf("Warning: Failed to create GPU streams, falling back to CPU processing\n");
            buffer->gpu_type = GPU_PROCESSING_DISABLED;
        }
        
        // Initialize GPU-specific components
        switch (type) {
#ifdef ENABLE_CUDA
            case GPU_PROCESSING_CUDA:
            case GPU_PROCESSING_HYBRID:
                if (cuda_initialize_device(buffer) != 0) {
                    printf("Warning: CUDA initialization failed\n");
                    if (type == GPU_PROCESSING_CUDA) {
                        buffer->gpu_type = GPU_PROCESSING_DISABLED;
                    }
                }
                break;
#endif
                
#ifdef ENABLE_OPENCL
            case GPU_PROCESSING_OPENCL:
                if (opencl_initialize_device(buffer) != 0) {
                    printf("Warning: OpenCL initialization failed\n");
                    buffer->gpu_type = GPU_PROCESSING_DISABLED;
                }
                break;
#endif
            default:
                break;
        }
        
        // Allocate GPU memory pool
        buffer->pool_size = 1024;  // Initial pool size
        buffer->memory_pool = (gpu_memory_block_t*)calloc(buffer->pool_size, sizeof(gpu_memory_block_t));
        if (!buffer->memory_pool) {
            printf("Failed to allocate GPU memory pool\n");
            gpu_buffer_cleanup(buffer);
            return -1;
        }
    }
    
    buffer->gpu_initialized = true;
    buffer->processing_active = true;
    
    printf("✅ GPU-accelerated buffer initialized: %s processing, %llu capacity\n",
           (type == GPU_PROCESSING_DISABLED) ? "CPU" :
           (type == GPU_PROCESSING_CUDA) ? "CUDA" :
           (type == GPU_PROCESSING_OPENCL) ? "OpenCL" : "Hybrid",
           (unsigned long long)capacity);
    
    return 0;
}

// GPU memory management
gpu_memory_block_t* gpu_allocate_pinned_memory(size_t size) {
    gpu_memory_block_t* block = (gpu_memory_block_t*)malloc(sizeof(gpu_memory_block_t));
    if (!block) return NULL;
    
    memset(block, 0, sizeof(gpu_memory_block_t));
    block->size = size;
    
#ifdef ENABLE_CUDA
    // Allocate CUDA pinned memory for faster transfers
    cudaError_t error = cudaMallocHost(&block->host_ptr, size);
    if (error == cudaSuccess) {
        block->is_pinned = true;
        cudaMalloc(&block->device_ptr, size);
    } else {
        // Fallback to regular malloc
        block->host_ptr = malloc(size);
        block->is_pinned = false;
    }
#else
    block->host_ptr = malloc(size);
    block->is_pinned = false;
#endif
    
    return block;
}

void gpu_free_pinned_memory(gpu_memory_block_t* block) {
    if (!block) return;
    
#ifdef ENABLE_CUDA
    if (block->is_pinned) {
        if (block->host_ptr) cudaFreeHost(block->host_ptr);
        if (block->device_ptr) cudaFree(block->device_ptr);
    } else {
        free(block->host_ptr);
    }
#else
    free(block->host_ptr);
#endif
    
    free(block);
}

// GPU stream management
int gpu_create_streams(gpu_accelerated_buffer_t* buffer, uint32_t num_streams) {
    if (!buffer || num_streams == 0 || num_streams > MAX_GPU_STREAMS) {
        return -1;
    }
    
    buffer->gpu_streams = (gpu_stream_t*)calloc(num_streams, sizeof(gpu_stream_t));
    if (!buffer->gpu_streams) {
        return -1;
    }
    
    buffer->num_streams = num_streams;
    
    // Initialize stream structures
    for (uint32_t i = 0; i < num_streams; i++) {
        buffer->gpu_streams[i].stream_id = i;
        buffer->gpu_streams[i].is_active = false;
        buffer->gpu_streams[i].messages_processed = 0;
        buffer->gpu_streams[i].throughput_gbps = 0.0;
        buffer->gpu_streams[i].last_update_time = gpu_get_timestamp_ns();
    }
    
#ifdef ENABLE_CUDA
    if (buffer->gpu_type == GPU_PROCESSING_CUDA || buffer->gpu_type == GPU_PROCESSING_HYBRID) {
        return cuda_create_streams(buffer, num_streams);
    }
#endif
    
#ifdef ENABLE_OPENCL
    if (buffer->gpu_type == GPU_PROCESSING_OPENCL) {
        return opencl_create_command_queues(buffer, num_streams);
    }
#endif
    
    return 0;
}

gpu_stream_t* gpu_get_available_stream(gpu_accelerated_buffer_t* buffer) {
    if (!buffer || !buffer->gpu_streams) return NULL;
    
    // Find least busy stream
    gpu_stream_t* best_stream = &buffer->gpu_streams[0];
    uint64_t min_messages = best_stream->messages_processed;
    
    for (uint32_t i = 1; i < buffer->num_streams; i++) {
        if (buffer->gpu_streams[i].messages_processed < min_messages) {
            min_messages = buffer->gpu_streams[i].messages_processed;
            best_stream = &buffer->gpu_streams[i];
        }
    }
    
    return best_stream;
}

// High-performance batch processing
int gpu_prepare_batch(gpu_accelerated_buffer_t* buffer, const void* data, size_t size, gpu_batch_t* batch) {
    if (!buffer || !data || !batch || size == 0) {
        return -1;
    }
    
    // Allocate batch structures
    batch->count = (uint32_t)(size / 1024); // Assume 1KB average message size
    if (batch->count > GPU_BATCH_SIZE) {
        batch->count = GPU_BATCH_SIZE;
    }
    
    batch->data = (uint8_t*)data;
    batch->total_size = size;
    batch->batch_id = buffer->total_messages;
    batch->timestamp = gpu_get_timestamp_ns();
    
    // Allocate auxiliary arrays
    batch->offsets = (uint64_t*)malloc(batch->count * sizeof(uint64_t));
    batch->lengths = (uint32_t*)malloc(batch->count * sizeof(uint32_t));
    batch->lane_ids = (uint32_t*)malloc(batch->count * sizeof(uint32_t));
    
    if (!batch->offsets || !batch->lengths || !batch->lane_ids) {
        free(batch->offsets);
        free(batch->lengths);
        free(batch->lane_ids);
        return -1;
    }
    
    // Prepare message metadata
    const uint8_t* current = (const uint8_t*)data;
    size_t remaining = size;
    
    for (uint32_t i = 0; i < batch->count && remaining > 0; i++) {
        size_t msg_size = (remaining > 1024) ? 1024 : remaining;
        batch->offsets[i] = current - (const uint8_t*)data;
        batch->lengths[i] = (uint32_t)msg_size;
        batch->lane_ids[i] = gpu_hash_lane_selection(current, msg_size);
        
        current += msg_size;
        remaining -= msg_size;
    }
    
    return 0;
}

int gpu_execute_parallel_processing(gpu_accelerated_buffer_t* buffer, gpu_batch_t* batch) {
    if (!buffer || !batch) return -1;
    
    switch (buffer->gpu_type) {
#ifdef ENABLE_CUDA
        case GPU_PROCESSING_CUDA:
        case GPU_PROCESSING_HYBRID: {
            gpu_stream_t* stream = gpu_get_available_stream(buffer);
            if (stream) {
                return cuda_launch_processing_kernel(buffer, batch, stream->stream_id);
            }
            break;
        }
#endif
        
#ifdef ENABLE_OPENCL
        case GPU_PROCESSING_OPENCL: {
            gpu_stream_t* stream = gpu_get_available_stream(buffer);
            if (stream) {
                return opencl_execute_kernel(buffer, batch, stream->stream_id);
            }
            break;
        }
#endif
        
        default:
            // CPU fallback processing
            for (uint32_t i = 0; i < batch->count; i++) {
                uint64_t offset = (buffer->head + batch->offsets[i]) & buffer->mask;
                uint32_t length = batch->lengths[i];
                
                // Simple memcpy for CPU processing
                if (offset + length <= buffer->capacity) {
                    memcpy(buffer->host_buffer + offset, batch->data + batch->offsets[i], length);
                } else {
                    // Handle wrap-around
                    uint32_t first_part = (uint32_t)(buffer->capacity - offset);
                    memcpy(buffer->host_buffer + offset, batch->data + batch->offsets[i], first_part);
                    memcpy(buffer->host_buffer, batch->data + batch->offsets[i] + first_part, length - first_part);
                }
            }
            break;
    }
    
    // Update performance counters
    gpu_atomic_add_uint64(&buffer->total_messages, batch->count);
    gpu_atomic_add_uint64(&buffer->total_bytes, batch->total_size);
    gpu_atomic_add_uint64(&buffer->gpu_operations_count, 1);
    
    return 0;
}

// High-level write operation with GPU acceleration
int gpu_buffer_write_accelerated(gpu_accelerated_buffer_t* buffer, const void* data, size_t size, uint32_t lane_id) {
    if (!buffer || !data || size == 0 || !buffer->processing_active) {
        return -1;
    }
    
    // For large writes, use batch processing
    if (size >= 4096 && buffer->gpu_type != GPU_PROCESSING_DISABLED) {
        gpu_batch_t batch;
        if (gpu_prepare_batch(buffer, data, size, &batch) == 0) {
            int result = gpu_execute_parallel_processing(buffer, &batch);
            
            // Cleanup batch
            free(batch.offsets);
            free(batch.lengths);
            free(batch.lane_ids);
            
            if (result == 0) {
                buffer->head = (buffer->head + size) & buffer->mask;
                return 0;
            }
        }
    }
    
    // Fallback to direct memory copy
    uint64_t current_head = buffer->head;
    uint64_t new_head = (current_head + size) & buffer->mask;
    
    if (current_head + size <= buffer->capacity) {
        memcpy(buffer->host_buffer + current_head, data, size);
    } else {
        // Handle wrap-around
        size_t first_part = buffer->capacity - current_head;
        memcpy(buffer->host_buffer + current_head, data, first_part);
        memcpy(buffer->host_buffer, (const uint8_t*)data + first_part, size - first_part);
    }
    
    buffer->head = new_head;
    gpu_atomic_add_uint64(&buffer->total_messages, 1);
    gpu_atomic_add_uint64(&buffer->total_bytes, size);
    
    return 0;
}

// Performance monitoring
void gpu_update_performance_metrics(gpu_accelerated_buffer_t* buffer, gpu_performance_metrics_t* metrics) {
    if (!buffer || !metrics) return;
    
    uint64_t current_time = gpu_get_timestamp_ns();
    static uint64_t last_update_time = 0;
    
    if (last_update_time == 0) {
        last_update_time = current_time;
        return;
    }
    
    double time_delta_seconds = (double)(current_time - last_update_time) / 1e9;
    if (time_delta_seconds < 0.1) return; // Update every 100ms
    
    // Calculate throughput
    static uint64_t last_total_messages = 0;
    static uint64_t last_total_bytes = 0;
    
    uint64_t messages_delta = buffer->total_messages - last_total_messages;
    uint64_t bytes_delta = buffer->total_bytes - last_total_bytes;
    
    metrics->messages_per_second = (uint64_t)(messages_delta / time_delta_seconds);
    metrics->throughput_gbps = (bytes_delta * 8.0) / (time_delta_seconds * 1e9);
    
    // Update buffer's current throughput
    gpu_atomic_store_double(&buffer->current_throughput_gbps, metrics->throughput_gbps);
    
    // GPU-specific metrics
    if (buffer->gpu_type != GPU_PROCESSING_DISABLED) {
        uint32_t active_streams = 0;
        for (uint32_t i = 0; i < buffer->num_streams; i++) {
            if (buffer->gpu_streams[i].is_active) {
                active_streams++;
            }
        }
        metrics->active_streams = active_streams;
        metrics->gpu_utilization = (double)active_streams / buffer->num_streams * 100.0;
    }
    
    last_update_time = current_time;
    last_total_messages = buffer->total_messages;
    last_total_bytes = buffer->total_bytes;
}

double gpu_calculate_throughput(gpu_accelerated_buffer_t* buffer) {
    return buffer->current_throughput_gbps;
}

// Cleanup
void gpu_buffer_cleanup(gpu_accelerated_buffer_t* buffer) {
    if (!buffer) return;
    
    buffer->processing_active = false;
    
    // Free host buffer
    if (buffer->host_buffer) {
#ifdef _WIN32
        VirtualFree(buffer->host_buffer, 0, MEM_RELEASE);
#else
        free(buffer->host_buffer);
#endif
        buffer->host_buffer = NULL;
    }
    
    // Free GPU resources
    if (buffer->memory_pool) {
        for (uint32_t i = 0; i < buffer->pool_allocated; i++) {
            gpu_free_pinned_memory(&buffer->memory_pool[i]);
        }
        free(buffer->memory_pool);
        buffer->memory_pool = NULL;
    }
    
    // Free streams
    gpu_destroy_streams(buffer);
    
    buffer->gpu_initialized = false;
    printf("🧹 GPU-accelerated buffer cleanup completed\n");
}

void gpu_destroy_streams(gpu_accelerated_buffer_t* buffer) {
    if (!buffer || !buffer->gpu_streams) return;
    
#ifdef ENABLE_CUDA
    if (buffer->cuda_streams) {
        for (uint32_t i = 0; i < buffer->num_streams; i++) {
            cudaStreamDestroy(buffer->cuda_streams[i]);
        }
        free(buffer->cuda_streams);
        buffer->cuda_streams = NULL;
    }
    
    if (buffer->events) {
        for (uint32_t i = 0; i < buffer->num_streams; i++) {
            cudaEventDestroy(buffer->events[i]);
        }
        free(buffer->events);
        buffer->events = NULL;
    }
#endif
    
#ifdef ENABLE_OPENCL
    if (buffer->cl_queues) {
        for (uint32_t i = 0; i < buffer->num_streams; i++) {
            clReleaseCommandQueue(buffer->cl_queues[i]);
        }
        free(buffer->cl_queues);
        buffer->cl_queues = NULL;
    }
#endif
    
    free(buffer->gpu_streams);
    buffer->gpu_streams = NULL;
    buffer->num_streams = 0;
}

// Stub implementations for CUDA functions (to be compiled with nvcc)
#ifdef ENABLE_CUDA
int cuda_initialize_device(gpu_accelerated_buffer_t* buffer) {
    // Implementation would go here
    return 0;
}

int cuda_create_streams(gpu_accelerated_buffer_t* buffer, uint32_t num_streams) {
    // Implementation would go here
    return 0;
}

int cuda_launch_processing_kernel(gpu_accelerated_buffer_t* buffer, gpu_batch_t* batch, uint32_t stream_id) {
    // Implementation would go here
    return 0;
}
#endif

// Stub implementations for OpenCL functions
#ifdef ENABLE_OPENCL
int opencl_initialize_device(gpu_accelerated_buffer_t* buffer) {
    // Implementation would go here
    return 0;
}

int opencl_create_command_queues(gpu_accelerated_buffer_t* buffer, uint32_t num_queues) {
    // Implementation would go here
    return 0;
}

int opencl_execute_kernel(gpu_accelerated_buffer_t* buffer, gpu_batch_t* batch, uint32_t queue_id) {
    // Implementation would go here
    return 0;
}
#endif