#include "../include/somakernel.h"
#include "../include/feedback_stream.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
double get_time() {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / freq.QuadPart;
}
#else
double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}
#endif

void benchmark_submit_performance(SomakernelBus* bus, int iterations) {
    printf("🚀 Submit Performance Benchmark\n");
    printf("================================\n");
    
    const char* testMsg = "Benchmark message payload for performance testing";
    size_t msgSize = strlen(testMsg);
    
    double startTime = get_time();
    
    for (int i = 0; i < iterations; ++i) {
        somakernel_submit_to(bus, i % bus->ring.activeCount, testMsg, msgSize);
    }
    
    double endTime = get_time();
    double duration = endTime - startTime;
    double throughput = iterations / duration;
    double latency = (duration / iterations) * 1000000; // microseconds
    
    printf("Messages: %d\n", iterations);
    printf("Duration: %.3f seconds\n", duration);
    printf("Throughput: %.0f msg/sec\n", throughput);
    printf("Latency: %.2f μs per message\n", latency);
    printf("\n");
}

void benchmark_drain_performance(SomakernelBus* bus, int iterations) {
    printf("📥 Drain Performance Benchmark\n");
    printf("==============================\n");
    
    double startTime = get_time();
    
    for (int i = 0; i < iterations; ++i) {
        somakernel_drain_from(bus, i % bus->ring.activeCount);
    }
    
    double endTime = get_time();
    double duration = endTime - startTime;
    double throughput = iterations / duration;
    double latency = (duration / iterations) * 1000000; // microseconds
    
    printf("Drain attempts: %d\n", iterations);
    printf("Duration: %.3f seconds\n", duration);
    printf("Throughput: %.0f ops/sec\n", throughput);
    printf("Latency: %.2f μs per operation\n", latency);
    printf("\n");
}

void benchmark_feedback_performance(SomakernelBus* bus, int iterations) {
    printf("🧾 Feedback Performance Benchmark\n");
    printf("=================================\n");
    
    double startTime = get_time();
    
    for (int i = 0; i < iterations; ++i) {
        size_t count;
        FeedbackEntry* entries = somakernel_get_feedback(bus, &count);
        (void)entries; // Suppress unused variable warning
    }
    
    double endTime = get_time();
    double duration = endTime - startTime;
    double throughput = iterations / duration;
    double latency = (duration / iterations) * 1000000; // microseconds
    
    printf("Feedback queries: %d\n", iterations);
    printf("Duration: %.3f seconds\n", duration);
    printf("Throughput: %.0f ops/sec\n", throughput);
    printf("Latency: %.2f μs per operation\n", latency);
    printf("\n");
}

int main() {
    printf("⚡ Somakernel Performance Benchmark Suite\n");
    printf("==========================================\n");
    printf("Platform: Windows (compiled with GCC)\n");
    printf("Timestamp: %ld\n\n", (long)time(NULL));
    
    // Initialize with larger capacity for benchmarking
    SomakernelBus* bus = somakernel_init(64 * 1024, 128 * 1024); // 64KB buffers, 128KB arena
    printf("✅ Bus initialized with %zu segments\n", bus->ring.activeCount);
    printf("Buffer capacity: %zu bytes\n", bus->ring.buffers[0].capacity);
    printf("Arena capacity: %zu bytes\n\n", bus->arena.capacity);
    
    // Benchmark iterations
    const int BENCH_ITERATIONS = 10000;
    
    // Submit benchmark
    benchmark_submit_performance(bus, BENCH_ITERATIONS);
    
    // Drain benchmark (will process submitted messages)
    benchmark_drain_performance(bus, BENCH_ITERATIONS);
    
    // Feedback benchmark
    benchmark_feedback_performance(bus, BENCH_ITERATIONS);
    
    // Summary statistics
    printf("📊 Final Statistics\n");
    printf("===================\n");
    size_t feedbackCount;
    FeedbackEntry* entries = somakernel_get_feedback(bus, &feedbackCount);
    
    // Count feedback types
    int okCount = 0, cpuCount = 0, gpuCount = 0, throttledCount = 0, skippedCount = 0, idleCount = 0;
    for (size_t i = 0; i < feedbackCount; ++i) {
        switch (entries[i].type) {
            case FEEDBACK_OK: okCount++; break;
            case FEEDBACK_CPU_EXECUTED: cpuCount++; break;
            case FEEDBACK_GPU_EXECUTED: gpuCount++; break;
            case FEEDBACK_THROTTLED: throttledCount++; break;
            case FEEDBACK_SKIPPED: skippedCount++; break;
            case FEEDBACK_IDLE: idleCount++; break;
            default: break;
        }
    }
    
    printf("Total feedback entries: %zu\n", feedbackCount);
    printf("  - OK: %d\n", okCount);
    printf("  - CPU Executed: %d\n", cpuCount);
    printf("  - GPU Executed: %d\n", gpuCount);
    printf("  - Throttled: %d\n", throttledCount);
    printf("  - Skipped: %d\n", skippedCount);
    printf("  - Idle: %d\n", idleCount);
    
    double successRate = (double)(okCount + cpuCount + gpuCount) / feedbackCount * 100;
    printf("Success rate: %.1f%%\n", successRate);
    
    printf("\n🧹 Cleanup and exit...\n");
    somakernel_free(bus);
    printf("✅ Benchmark completed successfully!\n");
    
    return 0;
}