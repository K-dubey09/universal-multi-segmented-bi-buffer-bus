#pragma once
#include <stddef.h>
#include <stdbool.h>

// GPU capability structure
typedef struct {
    bool has_cuda;
    bool has_opencl;
    bool has_compute;
    bool has_memory_pool;
    size_t memory_size;
    int compute_capability;
    size_t max_threads;
} gpu_capabilities_t;

// GPU initialization and detection
bool initialize_gpu();
bool gpu_available();
gpu_capabilities_t get_gpu_capabilities();

// GPU memory management
bool gpu_parallel_copy(void* dst, const void* src, size_t size, size_t num_threads);
bool try_gpu_execute(void* ptr, size_t size);

// Cleanup
void cleanup_gpu();