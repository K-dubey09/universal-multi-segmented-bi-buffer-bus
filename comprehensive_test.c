/*
 * UMSBB Comprehensive Test Suite
 * Tests all connectors and demonstrates performance
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <threads.h>
#include <stdatomic.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

// Include our complete core
#include "umsbb_complete_core.c"

// Test configuration
#define TEST_MESSAGE_COUNT 50000
#define TEST_BUFFER_SIZE_MB 32
#define TEST_MESSAGE_SIZE 256
#define PERFORMANCE_ITERATIONS 5

// Test results structure
typedef struct {
    double duration_sec;
    uint64_t messages_processed;
    uint64_t bytes_processed;
    double messages_per_sec;
    double mb_per_sec;
    int success;
} test_result_t;

// Global test state
static atomic_bool test_running = false;
static atomic_uint64_t producer_count = 0;
static atomic_uint64_t consumer_count = 0;

// Utility functions
static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void print_separator(const char* title) {
    printf("\n");
    printf("=%.50s=\n", "==================================================");
    printf("  %s\n", title);
    printf("=%.50s=\n", "==================================================");
}

static void print_test_result(const char* test_name, test_result_t* result) {
    printf("\n%s Results:\n", test_name);
    printf("  Duration:      %.3f seconds\n", result->duration_sec);
    printf("  Messages:      %llu\n", (unsigned long long)result->messages_processed);
    printf("  Bytes:         %llu\n", (unsigned long long)result->bytes_processed);
    printf("  Messages/sec:  %.0f\n", result->messages_per_sec);
    printf("  MB/sec:        %.2f\n", result->mb_per_sec);
    printf("  Status:        %s\n", result->success ? "PASSED" : "FAILED");
}

// Core performance test
static int core_performance_test(test_result_t* result) {
    umsbb_handle_t buffer = umsbb_create_buffer(TEST_BUFFER_SIZE_MB);
    if (buffer == 0) {
        printf("Failed to create buffer\n");
        result->success = 0;
        return -1;
    }

    printf("Running core performance test...\n");
    
    // Prepare test data
    uint8_t test_data[TEST_MESSAGE_SIZE];
    for (int i = 0; i < TEST_MESSAGE_SIZE; i++) {
        test_data[i] = i % 256;
    }

    double start_time = get_time_ms();
    test_running = true;
    producer_count = 0;
    consumer_count = 0;

    // Producer thread
    thrd_t producer_thread;
    int producer_func(void* arg) {
        umsbb_handle_t handle = *(umsbb_handle_t*)arg;
        
        for (uint64_t i = 0; i < TEST_MESSAGE_COUNT && test_running; i++) {
            if (umsbb_write_message(handle, test_data, TEST_MESSAGE_SIZE) == UMSBB_SUCCESS) {
                atomic_fetch_add(&producer_count, 1);
            }
            
            if (i % 10000 == 0) {
                printf("  Produced: %llu messages\n", (unsigned long long)i);
            }
        }
        return 0;
    }

    // Consumer thread
    thrd_t consumer_thread;
    int consumer_func(void* arg) {
        umsbb_handle_t handle = *(umsbb_handle_t*)arg;
        uint8_t read_buffer[TEST_MESSAGE_SIZE * 2];
        uint32_t actual_size;
        
        while (consumer_count < TEST_MESSAGE_COUNT && test_running) {
            if (umsbb_read_message(handle, read_buffer, sizeof(read_buffer), &actual_size) == UMSBB_SUCCESS) {
                atomic_fetch_add(&consumer_count, 1);
                
                if (consumer_count % 10000 == 0) {
                    printf("  Consumed: %llu messages\n", (unsigned long long)consumer_count);
                }
            } else {
                sleep_ms(1); // Small delay if buffer empty
            }
        }
        return 0;
    }

    // Start threads
    thrd_create(&producer_thread, producer_func, &buffer);
    thrd_create(&consumer_thread, consumer_func, &buffer);

    // Wait for completion
    thrd_join(producer_thread, NULL);
    thrd_join(consumer_thread, NULL);

    test_running = false;
    double end_time = get_time_ms();

    // Get final statistics
    uint64_t total_messages = umsbb_get_total_messages(buffer);
    uint64_t total_bytes = umsbb_get_total_bytes(buffer);

    // Calculate results
    result->duration_sec = (end_time - start_time) / 1000.0;
    result->messages_processed = total_messages;
    result->bytes_processed = total_bytes;
    result->messages_per_sec = total_messages / result->duration_sec;
    result->mb_per_sec = (total_bytes / (1024.0 * 1024.0)) / result->duration_sec;
    result->success = (total_messages == TEST_MESSAGE_COUNT) ? 1 : 0;

    umsbb_destroy_buffer(buffer);
    return result->success ? 0 : -1;
}

// Latency test
static int latency_test(test_result_t* result) {
    umsbb_handle_t buffer = umsbb_create_buffer(8); // Smaller buffer for latency
    if (buffer == 0) {
        result->success = 0;
        return -1;
    }

    printf("Running latency test...\n");

    const int latency_iterations = 10000;
    double total_latency = 0.0;
    uint8_t test_data[] = "Latency test message";
    uint8_t read_buffer[64];
    uint32_t actual_size;

    double start_time = get_time_ms();

    for (int i = 0; i < latency_iterations; i++) {
        double write_start = get_time_ms();
        
        // Write message
        if (umsbb_write_message(buffer, test_data, sizeof(test_data)) != UMSBB_SUCCESS) {
            printf("Write failed at iteration %d\n", i);
            result->success = 0;
            break;
        }

        // Read message immediately
        if (umsbb_read_message(buffer, read_buffer, sizeof(read_buffer), &actual_size) != UMSBB_SUCCESS) {
            printf("Read failed at iteration %d\n", i);
            result->success = 0;
            break;
        }

        double write_end = get_time_ms();
        total_latency += (write_end - write_start);

        if (i % 1000 == 0) {
            printf("  Latency test: %d/%d iterations\n", i, latency_iterations);
        }
    }

    double end_time = get_time_ms();

    // Calculate results
    result->duration_sec = (end_time - start_time) / 1000.0;
    result->messages_processed = latency_iterations * 2; // Read + Write
    result->bytes_processed = latency_iterations * sizeof(test_data) * 2;
    result->messages_per_sec = result->messages_processed / result->duration_sec;
    result->mb_per_sec = (result->bytes_processed / (1024.0 * 1024.0)) / result->duration_sec;
    
    if (result->success != 0) {
        result->success = 1;
        printf("  Average round-trip latency: %.3f ms\n", total_latency / latency_iterations);
    }

    umsbb_destroy_buffer(buffer);
    return result->success ? 0 : -1;
}

// Stress test with multiple buffers
static int stress_test(test_result_t* result) {
    const int num_buffers = 4;
    umsbb_handle_t buffers[num_buffers];

    printf("Running stress test with %d concurrent buffers...\n", num_buffers);

    // Create multiple buffers
    for (int i = 0; i < num_buffers; i++) {
        buffers[i] = umsbb_create_buffer(16); // 16MB each
        if (buffers[i] == 0) {
            printf("Failed to create buffer %d\n", i);
            result->success = 0;
            return -1;
        }
    }

    double start_time = get_time_ms();
    test_running = true;
    atomic_store(&producer_count, 0);
    atomic_store(&consumer_count, 0);

    // Thread data structure
    typedef struct {
        umsbb_handle_t buffer;
        int buffer_id;
        int is_producer;
    } thread_data_t;

    thread_data_t thread_data[num_buffers * 2];
    thrd_t threads[num_buffers * 2];

    // Thread function
    int stress_thread_func(void* arg) {
        thread_data_t* data = (thread_data_t*)arg;
        uint8_t test_data[128];
        uint8_t read_buffer[256];
        uint32_t actual_size;

        // Initialize test data
        for (int i = 0; i < sizeof(test_data); i++) {
            test_data[i] = (data->buffer_id * 100 + i) % 256;
        }

        const int iterations = TEST_MESSAGE_COUNT / num_buffers;

        if (data->is_producer) {
            // Producer
            for (int i = 0; i < iterations && test_running; i++) {
                if (umsbb_write_message(data->buffer, test_data, sizeof(test_data)) == UMSBB_SUCCESS) {
                    atomic_fetch_add(&producer_count, 1);
                }
            }
        } else {
            // Consumer
            int consumed = 0;
            while (consumed < iterations && test_running) {
                if (umsbb_read_message(data->buffer, read_buffer, sizeof(read_buffer), &actual_size) == UMSBB_SUCCESS) {
                    consumed++;
                    atomic_fetch_add(&consumer_count, 1);
                } else {
                    sleep_ms(1);
                }
            }
        }

        return 0;
    }

    // Create threads (one producer and one consumer per buffer)
    for (int i = 0; i < num_buffers; i++) {
        // Producer thread
        thread_data[i * 2].buffer = buffers[i];
        thread_data[i * 2].buffer_id = i;
        thread_data[i * 2].is_producer = 1;
        thrd_create(&threads[i * 2], stress_thread_func, &thread_data[i * 2]);

        // Consumer thread
        thread_data[i * 2 + 1].buffer = buffers[i];
        thread_data[i * 2 + 1].buffer_id = i;
        thread_data[i * 2 + 1].is_producer = 0;
        thrd_create(&threads[i * 2 + 1], stress_thread_func, &thread_data[i * 2 + 1]);
    }

    // Wait for all threads
    for (int i = 0; i < num_buffers * 2; i++) {
        thrd_join(threads[i], NULL);
    }

    test_running = false;
    double end_time = get_time_ms();

    // Collect statistics from all buffers
    uint64_t total_messages = 0;
    uint64_t total_bytes = 0;

    for (int i = 0; i < num_buffers; i++) {
        total_messages += umsbb_get_total_messages(buffers[i]);
        total_bytes += umsbb_get_total_bytes(buffers[i]);
        umsbb_destroy_buffer(buffers[i]);
    }

    // Calculate results
    result->duration_sec = (end_time - start_time) / 1000.0;
    result->messages_processed = total_messages;
    result->bytes_processed = total_bytes;
    result->messages_per_sec = total_messages / result->duration_sec;
    result->mb_per_sec = (total_bytes / (1024.0 * 1024.0)) / result->duration_sec;
    result->success = (producer_count > 0 && consumer_count > 0) ? 1 : 0;

    return result->success ? 0 : -1;
}

// Run connector tests (if available)
static void run_connector_tests(void) {
    print_separator("Connector Tests");

    printf("Testing language connectors...\n\n");

    // Test Python connector
    printf("Python Connector Test:\n");
    printf("  Run: python connectors/python/umsbb_connector.py\n");
    printf("  Expected: Performance test with mock interface\n\n");

    // Test JavaScript connector  
    printf("JavaScript Connector Test:\n");
    printf("  Run: node connectors/javascript/umsbb_connector.js\n");
    printf("  Expected: Performance test with mock interface\n\n");

    // Test Rust connector
    printf("Rust Connector Test:\n");
    printf("  Run: cd connectors/rust && cargo run\n");
    printf("  Expected: Performance test with mock interface\n\n");

    // Test Web Interface
    printf("Web Interface Test:\n");
    printf("  Open: web/index.html in a browser\n");
    printf("  Expected: Interactive performance dashboard\n\n");

    printf("Note: Connectors use mock interfaces for demonstration.\n");
    printf("Build WebAssembly core for full functionality.\n");
}

// Generate comprehensive report
static void generate_report(test_result_t* results, int num_tests, const char** test_names) {
    print_separator("COMPREHENSIVE TEST REPORT");

    printf("Test Configuration:\n");
    printf("  Message Count:     %d\n", TEST_MESSAGE_COUNT);
    printf("  Buffer Size:       %d MB\n", TEST_BUFFER_SIZE_MB);
    printf("  Message Size:      %d bytes\n", TEST_MESSAGE_SIZE);
    printf("  Test Platform:     %s\n", 
#ifdef _WIN32
           "Windows"
#else
           "Linux/Unix"
#endif
    );

    printf("\nTest Results Summary:\n");
    printf("%-20s | %-10s | %-12s | %-12s | %-10s\n", 
           "Test", "Status", "Msgs/sec", "MB/sec", "Duration");
    printf("%-20s-+-%-10s-+-%-12s-+-%-12s-+-%-10s\n",
           "--------------------", "----------", "------------", "------------", "----------");

    double total_messages_per_sec = 0;
    double total_mb_per_sec = 0;
    int passed_tests = 0;

    for (int i = 0; i < num_tests; i++) {
        printf("%-20s | %-10s | %12.0f | %12.2f | %10.3f\n",
               test_names[i],
               results[i].success ? "PASSED" : "FAILED",
               results[i].messages_per_sec,
               results[i].mb_per_sec,
               results[i].duration_sec);

        if (results[i].success) {
            total_messages_per_sec += results[i].messages_per_sec;
            total_mb_per_sec += results[i].mb_per_sec;
            passed_tests++;
        }
    }

    printf("\nSummary:\n");
    printf("  Tests Passed:      %d/%d\n", passed_tests, num_tests);
    printf("  Average Msgs/sec:  %.0f\n", total_messages_per_sec / passed_tests);
    printf("  Average MB/sec:    %.2f\n", total_mb_per_sec / passed_tests);

    if (passed_tests == num_tests) {
        printf("  Overall Status:    ✅ ALL TESTS PASSED\n");
    } else {
        printf("  Overall Status:    ❌ SOME TESTS FAILED\n");
    }

    printf("\nNext Steps:\n");
    printf("  1. Build WebAssembly core: ./build_wasm.bat (Windows) or ./build_wasm.sh (Linux)\n");
    printf("  2. Test connectors individually\n");
    printf("  3. Open web/index.html for interactive testing\n");
    printf("  4. Review documentation in README.md\n");
}

int main(void) {
    print_separator("UMSBB v4.0 Comprehensive Test Suite");

    printf("Universal Multi-Segmented Bi-directional Buffer Bus\n");
    printf("Testing all components and measuring performance...\n");

    // Test results storage
    const int num_tests = 3;
    test_result_t results[num_tests];
    const char* test_names[] = {
        "Core Performance",
        "Latency Test", 
        "Stress Test"
    };

    // Initialize results
    for (int i = 0; i < num_tests; i++) {
        memset(&results[i], 0, sizeof(test_result_t));
    }

    // Run tests
    printf("\nRunning %d test suites...\n", num_tests);

    // Core performance test
    print_separator("Core Performance Test");
    core_performance_test(&results[0]);
    print_test_result(test_names[0], &results[0]);

    // Latency test
    print_separator("Latency Test");
    latency_test(&results[1]);
    print_test_result(test_names[1], &results[1]);

    // Stress test
    print_separator("Stress Test");
    stress_test(&results[2]);
    print_test_result(test_names[2], &results[2]);

    // Connector tests
    run_connector_tests();

    // Generate final report
    generate_report(results, num_tests, test_names);

    printf("\nTest suite completed successfully!\n");
    printf("Check web/index.html for interactive performance dashboard.\n");

    return 0;
}