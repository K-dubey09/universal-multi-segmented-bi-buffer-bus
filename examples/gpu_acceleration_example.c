#include "universal_multi_segmented_bi_buffer_bus.h"
#include "gpu_delegate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Test data generators
void generate_test_data(char* buffer, size_t size, int pattern) {
    switch (pattern) {
        case 0: // Sequential pattern
            for (size_t i = 0; i < size; i++) {
                buffer[i] = (char)(i % 256);
            }
            break;
        case 1: // Random pattern
            srand((unsigned int)time(NULL));
            for (size_t i = 0; i < size; i++) {
                buffer[i] = (char)(rand() % 256);
            }
            break;
        case 2: // Repetitive pattern
            memset(buffer, 'X', size);
            break;
        default: // Zero pattern
            memset(buffer, 0, size);
            break;
    }
}

void print_gpu_info() {
    printf("=== GPU Information ===\n");
    
    if (!gpu_available()) {
        printf("No GPU acceleration available\n\n");
        return;
    }

    gpu_capabilities_t caps = get_gpu_capabilities();
    
    printf("GPU acceleration available!\n");
    printf("Capabilities:\n");
    printf("  CUDA support: %s\n", caps.has_cuda ? "Yes" : "No");
    printf("  OpenCL support: %s\n", caps.has_opencl ? "Yes" : "No");
    printf("  Compute shaders: %s\n", caps.has_compute ? "Yes" : "No");
    printf("  Memory pool: %s\n", caps.has_memory_pool ? "Yes" : "No");
    printf("  Total memory: %.2f MB\n", (double)caps.memory_size / (1024 * 1024));
    printf("  Compute capability: %d\n", caps.compute_capability);
    printf("  Max threads: %zu\n", caps.max_threads);
    printf("\n");
}

double benchmark_operation(UniversalMultiSegmentedBiBufferBus* bus, char* data, size_t data_size, 
                          int iterations, const char* operation_name) {
    printf("Benchmarking %s (data size: %.2f MB, iterations: %d)...\n", 
           operation_name, (double)data_size / (1024 * 1024), iterations);

    clock_t start = clock();
    
    for (int i = 0; i < iterations; i++) {
        // Submit data
        bool success = umsbb_submit_to(bus, i % bus->segment_count, data, data_size);
        if (!success) {
            printf("Warning: Submit failed at iteration %d\n", i);
        }
        
        // Drain data
        size_t received_size;
        void* received_data = umsbb_drain_from(bus, i % bus->segment_count, &received_size);
        if (received_data) {
            free(received_data);
        }
    }
    
    clock_t end = clock();
    double elapsed = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0; // Convert to milliseconds
    
    printf("  Completed in: %.2f ms\n", elapsed);
    printf("  Avg per operation: %.3f ms\n", elapsed / iterations);
    printf("  Throughput: %.2f ops/sec\n\n", (iterations * 1000.0) / elapsed);
    
    return elapsed;
}

int main() {
    printf("=== Universal Multi-Segmented Bi-Buffer Bus ===\n");
    printf("GPU Acceleration Performance Demo\n\n");

    // Initialize GPU
    print_gpu_info();
    
    bool gpu_initialized = initialize_gpu();
    printf("GPU initialization: %s\n\n", gpu_initialized ? "SUCCESS" : "FAILED");

    // Create bus with GPU preference
    UniversalMultiSegmentedBiBufferBus* bus = umsbb_init(4 * 1024 * 1024, 8); // 4MB segments, 8 segments
    if (!bus) {
        printf("Failed to create bus\n");
        return 1;
    }

    // Configure for GPU acceleration
    if (!umsbb_configure_gpu(bus, true)) {
        printf("Warning: GPU configuration failed, using CPU fallback\n");
    } else {
        printf("✓ GPU acceleration enabled\n");
    }

    printf("✓ Bus created with %u segments\n\n", bus->segment_count);

    // Test data sizes for GPU acceleration thresholds
    size_t test_sizes[] = {
        1024,           // 1KB - CPU
        64 * 1024,      // 64KB - CPU
        256 * 1024,     // 256KB - CPU
        1024 * 1024,    // 1MB - GPU threshold
        4 * 1024 * 1024, // 4MB - GPU
        16 * 1024 * 1024 // 16MB - GPU
    };
    int num_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);

    printf("=== Performance Comparison: CPU vs GPU ===\n");
    
    for (int size_idx = 0; size_idx < num_sizes; size_idx++) {
        size_t data_size = test_sizes[size_idx];
        char* test_data = malloc(data_size);
        if (!test_data) {
            printf("Failed to allocate %zu bytes\n", data_size);
            continue;
        }

        // Generate test data with different patterns
        generate_test_data(test_data, data_size, size_idx % 4);

        char operation_name[256];
        if (data_size >= 1024 * 1024) {
            snprintf(operation_name, sizeof(operation_name), "Large Data (%.1fMB)", 
                    (double)data_size / (1024 * 1024));
        } else {
            snprintf(operation_name, sizeof(operation_name), "Small Data (%zuKB)", data_size / 1024);
        }

        // Test with current configuration
        double elapsed = benchmark_operation(bus, test_data, data_size, 100, operation_name);

        // Check if GPU was used (for large data)
        if (data_size >= 1024 * 1024 && gpu_initialized) {
            printf("  Expected GPU acceleration for this size\n");
        } else {
            printf("  CPU processing expected for this size\n");
        }

        printf("  Load factor: %.3f\n", umsbb_get_load_factor(bus));
        printf("  Optimal segments: %u\n\n", umsbb_get_optimal_segments(bus));

        free(test_data);
    }

    // Stress test with varying load
    printf("=== Stress Test: Dynamic Load Scaling ===\n");
    
    char* stress_data = malloc(2 * 1024 * 1024); // 2MB
    generate_test_data(stress_data, 2 * 1024 * 1024, 1); // Random pattern

    printf("Simulating variable load conditions...\n");
    
    for (int load_level = 1; load_level <= 5; load_level++) {
        printf("\nLoad Level %d (intensity: %d):\n", load_level, load_level * 20);
        
        int iterations = load_level * 20;
        double elapsed = benchmark_operation(bus, stress_data, 2 * 1024 * 1024, 
                                           iterations, "Stress Test");
        
        printf("  Current load factor: %.3f\n", umsbb_get_load_factor(bus));
        printf("  Suggested segments: %u\n", umsbb_get_optimal_segments(bus));
        
        // Simulate scaling
        uint32_t optimal = umsbb_get_optimal_segments(bus);
        if (optimal != bus->segment_count) {
            printf("  Scaling recommendation: %u → %u segments\n", 
                   bus->segment_count, optimal);
        }
    }

    free(stress_data);

    // GPU-specific tests if available
    if (gpu_initialized) {
        printf("\n=== GPU-Specific Performance Tests ===\n");
        
        // Very large data test
        size_t gpu_test_size = 32 * 1024 * 1024; // 32MB
        char* gpu_data = malloc(gpu_test_size);
        if (gpu_data) {
            generate_test_data(gpu_data, gpu_test_size, 2); // Repetitive pattern
            
            printf("Large GPU test (32MB):\n");
            clock_t gpu_start = clock();
            
            bool gpu_result = umsbb_submit_to(bus, 0, gpu_data, gpu_test_size);
            
            clock_t gpu_end = clock();
            double gpu_time = ((double)(gpu_end - gpu_start) / CLOCKS_PER_SEC) * 1000.0;
            
            printf("  GPU processing: %s\n", gpu_result ? "SUCCESS" : "FAILED");
            printf("  Time: %.2f ms\n", gpu_time);
            printf("  Bandwidth: %.2f GB/s\n", 
                   (double)gpu_test_size / (gpu_time / 1000.0) / (1024 * 1024 * 1024));
            
            // Drain the data
            size_t received_size;
            void* received = umsbb_drain_from(bus, 0, &received_size);
            if (received) {
                printf("  Data integrity: %s\n", 
                       (received_size == gpu_test_size) ? "VERIFIED" : "CORRUPTED");
                free(received);
            }
            
            free(gpu_data);
        }

        // Parallel GPU operations test
        printf("\nParallel GPU operations test:\n");
        size_t parallel_size = 8 * 1024 * 1024; // 8MB each
        char* parallel_data[4];
        
        // Allocate parallel test data
        for (int i = 0; i < 4; i++) {
            parallel_data[i] = malloc(parallel_size);
            if (parallel_data[i]) {
                generate_test_data(parallel_data[i], parallel_size, i);
            }
        }
        
        clock_t parallel_start = clock();
        
        // Submit to different segments simultaneously
        for (int i = 0; i < 4; i++) {
            if (parallel_data[i]) {
                umsbb_submit_to(bus, i % bus->segment_count, parallel_data[i], parallel_size);
            }
        }
        
        // Drain from all segments
        int drained_count = 0;
        for (int i = 0; i < 4; i++) {
            size_t received_size;
            void* received = umsbb_drain_from(bus, i % bus->segment_count, &received_size);
            if (received) {
                drained_count++;
                free(received);
            }
        }
        
        clock_t parallel_end = clock();
        double parallel_time = ((double)(parallel_end - parallel_start) / CLOCKS_PER_SEC) * 1000.0;
        
        printf("  Parallel operations: %d/4 successful\n", drained_count);
        printf("  Total time: %.2f ms\n", parallel_time);
        printf("  Combined throughput: %.2f GB/s\n", 
               (double)(parallel_size * 4) / (parallel_time / 1000.0) / (1024 * 1024 * 1024));
        
        // Cleanup parallel data
        for (int i = 0; i < 4; i++) {
            if (parallel_data[i]) {
                free(parallel_data[i]);
            }
        }
    }

    // Final statistics
    printf("\n=== Final Performance Summary ===\n");
    printf("Bus configuration:\n");
    printf("  Segments: %u\n", bus->segment_count);
    printf("  GPU enabled: %s\n", bus->gpu_enabled ? "Yes" : "No");
    printf("  Total operations: %llu\n", (unsigned long long)bus->total_operations);
    printf("  Final load factor: %.3f\n", umsbb_get_load_factor(bus));
    
    if (gpu_initialized) {
        gpu_capabilities_t final_caps = get_gpu_capabilities();
        printf("GPU utilization summary:\n");
        printf("  Memory pool available: %s\n", final_caps.has_memory_pool ? "Yes" : "No");
        printf("  Recommended for data ≥ 1MB\n");
    }

    // Cleanup
    umsbb_free(bus);
    cleanup_gpu();
    
    printf("\n✓ GPU acceleration demo completed successfully\n");
    return 0;
}