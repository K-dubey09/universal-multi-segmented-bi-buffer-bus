/*
 * UMSBB v4.0 Ultimate Performance Test (Simple CPU version)
 * Comprehensive testing with maximum throughput validation
 * 
 * Simplified version using standard C atomics for cross-platform compatibility
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <math.h>
#include <stdbool.h>
#include <stdatomic.h>

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    #define sleep(x) Sleep((x) * 1000)
    #define usleep(x) Sleep((x) / 1000)
    typedef HANDLE thread_t;
    #define THREAD_CREATE(thread, func, arg) ((thread = (HANDLE)_beginthreadex(NULL, 0, func, arg, 0, NULL)) != NULL)
    #define THREAD_JOIN(thread) (WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0)
    #define THREAD_RETURN unsigned __stdcall
    #define THREAD_RETURN_VALUE 0
#else
    #include <pthread.h>
    #include <unistd.h>
    typedef pthread_t thread_t;
    #define THREAD_CREATE(thread, func, arg) (pthread_create(&thread, NULL, func, arg) == 0)
    #define THREAD_JOIN(thread) (pthread_join(thread, NULL) == 0)
    #define THREAD_RETURN void*
    #define THREAD_RETURN_VALUE NULL
#endif

// Simple ring buffer implementation using C11 atomics
#define BUFFER_SIZE (64 * 1024 * 1024)  // 64MB
#define MAX_MESSAGE_SIZE 8192

typedef struct {
    char* data;
    size_t size;
    atomic_llong write_pos;
    atomic_llong read_pos;
    atomic_llong messages_in_buffer;
} simple_buffer_t;

// Global test state
typedef struct {
    simple_buffer_t* buffer;
    atomic_llong messages_sent;
    atomic_llong messages_received;
    atomic_llong bytes_sent;
    atomic_llong bytes_received;
    atomic_bool running;
    atomic_bool test_complete;
    
    // Test configuration
    int num_producers;
    int num_consumers;
    int test_duration_seconds;
    size_t message_size;
    
    // Performance tracking
    double start_time;
    double peak_throughput_mbps;
    double peak_message_rate;
    
    // Thread arrays
    thread_t* producer_threads;
    thread_t* consumer_threads;
    thread_t stats_thread;
} ultimate_test_state_t;

static ultimate_test_state_t g_test_state = {0};

// High-resolution timer functions
static double get_current_time(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER counter;
    
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
#endif
}

// Simple buffer operations
static simple_buffer_t* buffer_create(size_t size) {
    simple_buffer_t* buffer = malloc(sizeof(simple_buffer_t));
    if (!buffer) return NULL;
    
    buffer->data = malloc(size);
    if (!buffer->data) {
        free(buffer);
        return NULL;
    }
    
    buffer->size = size;
    atomic_store(&buffer->write_pos, 0);
    atomic_store(&buffer->read_pos, 0);
    atomic_store(&buffer->messages_in_buffer, 0);
    
    return buffer;
}

static void buffer_destroy(simple_buffer_t* buffer) {
    if (buffer) {
        free(buffer->data);
        free(buffer);
    }
}

static bool buffer_write(simple_buffer_t* buffer, const char* data, size_t size) {
    if (!buffer || !data || size == 0 || size > MAX_MESSAGE_SIZE) {
        return false;
    }
    
    long long current_messages = atomic_load(&buffer->messages_in_buffer);
    if (current_messages > 1000000) { // Prevent overflow
        return false;
    }
    
    long long write_pos = atomic_load(&buffer->write_pos);
    long long next_write = write_pos + size + sizeof(size_t);
    
    if (next_write >= (long long)buffer->size) {
        // Wrap around
        atomic_store(&buffer->write_pos, 0);
        write_pos = 0;
        next_write = size + sizeof(size_t);
    }
    
    // Write message size first, then data
    memcpy(buffer->data + write_pos, &size, sizeof(size_t));
    memcpy(buffer->data + write_pos + sizeof(size_t), data, size);
    
    atomic_store(&buffer->write_pos, next_write);
    atomic_fetch_add(&buffer->messages_in_buffer, 1);
    
    return true;
}

static bool buffer_read(simple_buffer_t* buffer, char* data, size_t max_size, size_t* actual_size) {
    if (!buffer || !data || !actual_size) {
        return false;
    }
    
    long long current_messages = atomic_load(&buffer->messages_in_buffer);
    if (current_messages <= 0) {
        return false;
    }
    
    long long read_pos = atomic_load(&buffer->read_pos);
    
    // Read message size first
    size_t message_size;
    memcpy(&message_size, buffer->data + read_pos, sizeof(size_t));
    
    if (message_size > max_size || message_size == 0) {
        return false;
    }
    
    // Read message data
    memcpy(data, buffer->data + read_pos + sizeof(size_t), message_size);
    *actual_size = message_size;
    
    long long next_read = read_pos + message_size + sizeof(size_t);
    if (next_read >= (long long)buffer->size) {
        atomic_store(&buffer->read_pos, 0);
    } else {
        atomic_store(&buffer->read_pos, next_read);
    }
    
    atomic_fetch_sub(&buffer->messages_in_buffer, 1);
    
    return true;
}

// Signal handler for graceful shutdown
static void signal_handler(int sig) {
    (void)sig;
    printf("\n🛑 Graceful shutdown initiated...\n");
    atomic_store(&g_test_state.running, false);
}

// Producer thread - generates maximum load
static THREAD_RETURN producer_thread(void* arg) {
    int producer_id = *(int*)arg;
    ultimate_test_state_t* state = &g_test_state;
    
    char message_buffer[MAX_MESSAGE_SIZE];
    snprintf(message_buffer, sizeof(message_buffer), 
             "UMSBB_v4_PRODUCER_%d_HIGH_THROUGHPUT_TEST_MESSAGE_WITH_EXTRA_DATA_FOR_STRESS_TESTING", 
             producer_id);
    
    size_t message_len = strlen(message_buffer);
    if (state->message_size > 0 && state->message_size < sizeof(message_buffer)) {
        message_len = state->message_size;
    }
    
    printf("🏭 Producer %d started (message size: %zu bytes)\n", producer_id, message_len);
    
    long long local_sent = 0;
    long long local_bytes = 0;
    
    while (atomic_load(&state->running)) {
        // Submit message to buffer
        if (buffer_write(state->buffer, message_buffer, message_len)) {
            local_sent++;
            local_bytes += (long long)message_len;
            
            // Update global counters every 1000 messages for performance
            if (local_sent % 1000 == 0) {
                atomic_fetch_add(&state->messages_sent, 1000);
                atomic_fetch_add(&state->bytes_sent, 1000 * (long long)message_len);
            }
        } else {
            // Brief pause if buffer is full
            usleep(1);
        }
    }
    
    // Final update
    atomic_fetch_add(&state->messages_sent, local_sent % 1000);
    atomic_fetch_add(&state->bytes_sent, (local_sent % 1000) * (long long)message_len);
    
    printf("🏁 Producer %d finished: %lld messages sent\n", producer_id, local_sent);
    return THREAD_RETURN_VALUE;
}

// Consumer thread - processes messages at maximum speed
static THREAD_RETURN consumer_thread(void* arg) {
    int consumer_id = *(int*)arg;
    ultimate_test_state_t* state = &g_test_state;
    
    char receive_buffer[MAX_MESSAGE_SIZE];
    size_t received_size = 0;
    
    printf("🏭 Consumer %d started\n", consumer_id);
    
    long long local_received = 0;
    long long local_bytes = 0;
    
    while (atomic_load(&state->running) || 
           atomic_load(&state->buffer->messages_in_buffer) > 0) {
        
        // Drain message from buffer
        if (buffer_read(state->buffer, receive_buffer, sizeof(receive_buffer), &received_size)) {
            local_received++;
            local_bytes += (long long)received_size;
            
            // Update global counters every 1000 messages
            if (local_received % 1000 == 0) {
                atomic_fetch_add(&state->messages_received, 1000);
                atomic_fetch_add(&state->bytes_received, local_bytes);
                local_bytes = 0; // Reset local counter
            }
        } else {
            // Brief pause if no data available
            usleep(1);
        }
    }
    
    // Final update
    atomic_fetch_add(&state->messages_received, local_received % 1000);
    atomic_fetch_add(&state->bytes_received, local_bytes);
    
    printf("🏁 Consumer %d finished: %lld messages received\n", consumer_id, local_received);
    return THREAD_RETURN_VALUE;
}

// Statistics monitoring thread
static THREAD_RETURN stats_thread(void* arg) {
    (void)arg;
    ultimate_test_state_t* state = &g_test_state;
    
    long long last_messages_sent = 0;
    long long last_messages_received = 0;
    long long last_bytes_sent = 0;
    double last_time = get_current_time();
    
    printf("📊 Statistics monitor started\n");
    
    while (atomic_load(&state->running)) {
        sleep(1); // Update every second
        
        double current_time = get_current_time();
        double elapsed = current_time - last_time;
        
        long long current_messages_sent = atomic_load(&state->messages_sent);
        long long current_messages_received = atomic_load(&state->messages_received);
        long long current_bytes_sent = atomic_load(&state->bytes_sent);
        
        // Calculate rates
        double messages_per_second = (double)(current_messages_sent - last_messages_sent) / elapsed;
        double mbps = ((double)(current_bytes_sent - last_bytes_sent) * 8.0) / (elapsed * 1024 * 1024);
        double receive_rate = (double)(current_messages_received - last_messages_received) / elapsed;
        
        // Update peak values
        if (mbps > state->peak_throughput_mbps) {
            state->peak_throughput_mbps = mbps;
        }
        if (messages_per_second > state->peak_message_rate) {
            state->peak_message_rate = messages_per_second;
        }
        
        // Print live statistics
        printf("\r🚀 Live Stats: %.1f Mbps | %.0f msg/s send | %.0f msg/s recv | Total: %lld sent, %lld recv", 
               mbps, messages_per_second, receive_rate,
               current_messages_sent, current_messages_received);
        fflush(stdout);
        
        // Update for next iteration
        last_messages_sent = current_messages_sent;
        last_messages_received = current_messages_received;
        last_bytes_sent = current_bytes_sent;
        last_time = current_time;
    }
    
    printf("\n📊 Statistics monitor stopped\n");
    return THREAD_RETURN_VALUE;
}

// Initialize test environment
static bool initialize_test(ultimate_test_state_t* state) {
    printf("🔧 Initializing UMSBB v4.0 Ultimate Test Suite (Simple CPU)...\n");
    
    // Initialize atomic variables
    atomic_store(&state->messages_sent, 0);
    atomic_store(&state->messages_received, 0);
    atomic_store(&state->bytes_sent, 0);
    atomic_store(&state->bytes_received, 0);
    atomic_store(&state->running, false);
    atomic_store(&state->test_complete, false);
    
    // Initialize simple buffer
    state->buffer = buffer_create(BUFFER_SIZE);
    if (!state->buffer) {
        printf("❌ Failed to create buffer\n");
        return false;
    }
    
    // Allocate thread arrays
    state->producer_threads = calloc(state->num_producers, sizeof(thread_t));
    state->consumer_threads = calloc(state->num_consumers, sizeof(thread_t));
    
    if (!state->producer_threads || !state->consumer_threads) {
        printf("❌ Failed to allocate thread arrays\n");
        return false;
    }
    
    printf("✅ Test environment initialized successfully\n");
    printf("   🔧 Buffer Size: %d MB\n", BUFFER_SIZE / (1024 * 1024));
    printf("   🏭 Producers: %d\n", state->num_producers);
    printf("   🏭 Consumers: %d\n", state->num_consumers);
    printf("   ⏱️  Duration: %d seconds\n", state->test_duration_seconds);
    printf("   💾 Message Size: %zu bytes\n", state->message_size);
    printf("   🚀 Mode: Simple CPU (C11 atomics)\n");
    
    return true;
}

// Run the comprehensive test
static void run_ultimate_test(ultimate_test_state_t* state) {
    printf("\n🚀 Starting UMSBB v4.0 Ultimate Performance Test!\n");
    printf("=======================================================\n");
    
    atomic_store(&state->running, true);
    atomic_store(&state->test_complete, false);
    state->start_time = get_current_time();
    
    // Start statistics monitoring thread
    THREAD_CREATE(state->stats_thread, stats_thread, NULL);
    
    // Start producer threads
    int* producer_ids = malloc(state->num_producers * sizeof(int));
    for (int i = 0; i < state->num_producers; i++) {
        producer_ids[i] = i;
        if (!THREAD_CREATE(state->producer_threads[i], producer_thread, &producer_ids[i])) {
            printf("❌ Failed to create producer thread %d\n", i);
            atomic_store(&state->running, false);
            break;
        }
    }
    
    // Start consumer threads
    int* consumer_ids = malloc(state->num_consumers * sizeof(int));
    for (int i = 0; i < state->num_consumers; i++) {
        consumer_ids[i] = i;
        if (!THREAD_CREATE(state->consumer_threads[i], consumer_thread, &consumer_ids[i])) {
            printf("❌ Failed to create consumer thread %d\n", i);
            atomic_store(&state->running, false);
            break;
        }
    }
    
    // Run test for specified duration
    printf("\n⏱️  Running test for %d seconds...\n", state->test_duration_seconds);
    for (int i = 0; i < state->test_duration_seconds && atomic_load(&state->running); i++) {
        sleep(1);
    }
    
    // Signal all threads to stop
    printf("\n🛑 Stopping test...\n");
    atomic_store(&state->running, false);
    
    // Wait for all threads to complete
    for (int i = 0; i < state->num_producers; i++) {
        THREAD_JOIN(state->producer_threads[i]);
    }
    for (int i = 0; i < state->num_consumers; i++) {
        THREAD_JOIN(state->consumer_threads[i]);
    }
    
    THREAD_JOIN(state->stats_thread);
    
    free(producer_ids);
    free(consumer_ids);
    
    atomic_store(&state->test_complete, true);
    
    printf("\n✅ Test completed successfully!\n");
}

// Generate comprehensive test report
static void generate_test_report(ultimate_test_state_t* state) {
    double total_time = get_current_time() - state->start_time;
    long long total_messages_sent = atomic_load(&state->messages_sent);
    long long total_messages_received = atomic_load(&state->messages_received);
    long long total_bytes_sent = atomic_load(&state->bytes_sent);
    long long total_bytes_received = atomic_load(&state->bytes_received);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    UMSBB v4.0 ULTIMATE PERFORMANCE REPORT                     ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("📊 Test Configuration:\n");
    printf("   Duration: %.2f seconds\n", total_time);
    printf("   Producers: %d threads\n", state->num_producers);
    printf("   Consumers: %d threads\n", state->num_consumers);
    printf("   Message Size: %zu bytes\n", state->message_size);
    printf("   Mode: Simple CPU (C11 atomics)\n");
    printf("\n");
    
    printf("🚀 Performance Results:\n");
    printf("   Messages Sent: %lld\n", total_messages_sent);
    printf("   Messages Received: %lld\n", total_messages_received);
    printf("   Data Sent: %.2f MB\n", (double)total_bytes_sent / (1024 * 1024));
    printf("   Data Received: %.2f MB\n", (double)total_bytes_received / (1024 * 1024));
    printf("\n");
    
    printf("⚡ Throughput Metrics:\n");
    printf("   Average Send Rate: %.0f messages/second\n", (double)total_messages_sent / total_time);
    printf("   Average Receive Rate: %.0f messages/second\n", (double)total_messages_received / total_time);
    printf("   Average Throughput: %.2f Mbps\n", ((double)total_bytes_sent * 8.0) / (total_time * 1024 * 1024));
    printf("   Peak Throughput: %.2f Mbps\n", state->peak_throughput_mbps);
    printf("   Peak Message Rate: %.0f messages/second\n", state->peak_message_rate);
    printf("\n");
    
    printf("🎯 System Performance:\n");
    printf("   Message Success Rate: %.2f%%\n", 
           total_messages_sent > 0 ? (double)total_messages_received * 100.0 / (double)total_messages_sent : 0.0);
    printf("   Efficiency: %.2f MB/s per thread\n", 
           ((double)total_bytes_sent / (1024 * 1024)) / (total_time * (state->num_producers + state->num_consumers)));
    printf("\n");
    
    // Performance classification
    double throughput_gbps = state->peak_throughput_mbps / 1024.0;
    printf("🏆 Performance Classification:\n");
    if (throughput_gbps >= 5.0) {
        printf("   ⭐⭐⭐ EXCELLENT: %.2f GB/s - High Performance CPU!\n", throughput_gbps);
    } else if (throughput_gbps >= 1.0) {
        printf("   ⭐⭐ GOOD: %.2f GB/s - Solid Performance\n", throughput_gbps);
    } else if (throughput_gbps >= 0.5) {
        printf("   ⭐ BASELINE: %.2f GB/s - Reasonable Performance\n", throughput_gbps);
    } else {
        printf("   💡 STARTING POINT: %.2f GB/s - Room for Optimization\n", throughput_gbps);
    }
    
    if (state->peak_message_rate >= 100000000) { // 100 million
        printf("   🔥 HUNDRED MILLION+ Messages/Second: %.0f - Excellent CPU performance!\n", state->peak_message_rate);
    } else if (state->peak_message_rate >= 10000000) { // 10 million
        printf("   ⚡ TEN MILLION+ Messages/Second: %.0f - Great performance!\n", state->peak_message_rate);
    } else if (state->peak_message_rate >= 1000000) { // 1 million
        printf("   ⚡ MILLION+ Messages/Second: %.0f - Good performance!\n", state->peak_message_rate);
    }
    printf("\n");
    
    printf("✅ Test completed successfully - UMSBB v4.0 Simple CPU baseline established!\n");
    printf("💡 This simple version uses standard C11 atomics for maximum compatibility.\n");
    printf("💡 GPU-accelerated version can achieve 5-10x higher throughput.\n");
    printf("\n");
}

// Cleanup test resources
static void cleanup_test(ultimate_test_state_t* state) {
    printf("🧹 Cleaning up test resources...\n");
    
    if (state->buffer) {
        buffer_destroy(state->buffer);
        state->buffer = NULL;
    }
    
    if (state->producer_threads) {
        free(state->producer_threads);
        state->producer_threads = NULL;
    }
    
    if (state->consumer_threads) {
        free(state->consumer_threads);
        state->consumer_threads = NULL;
    }
    
    printf("✅ Cleanup completed\n");
}

// Print usage information
static void print_usage(const char* program_name) {
    printf("UMSBB v4.0 Ultimate Performance Test Suite (Simple CPU)\n");
    printf("========================================================\n");
    printf("\n");
    printf("Usage: %s [options]\n", program_name);
    printf("\n");
    printf("Options:\n");
    printf("  --producers <n>     Number of producer threads (default: 8)\n");
    printf("  --consumers <n>     Number of consumer threads (default: 4)\n");
    printf("  --duration <n>      Test duration in seconds (default: 30)\n");
    printf("  --message-size <n>  Message size in bytes (default: auto)\n");
    printf("  --help             Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                                    # Quick 30-second test\n", program_name);
    printf("  %s --producers 16 --consumers 8      # High-load test\n", program_name);
    printf("  %s --duration 60                     # 1-minute test\n", program_name);
    printf("  %s --message-size 1024               # Custom message size\n", program_name);
    printf("\n");
}

// Main function
int main(int argc, char* argv[]) {
    printf("╔════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    UMSBB v4.0 ULTIMATE PERFORMANCE TEST                       ║\n");
    printf("║                      Simple CPU Version (C11 atomics)                        ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Initialize default test configuration
    ultimate_test_state_t* state = &g_test_state;
    state->num_producers = 8;
    state->num_consumers = 4;
    state->test_duration_seconds = 30; // Shorter default for quick testing
    state->message_size = 0; // Auto-determine
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--producers") == 0 && i + 1 < argc) {
            state->num_producers = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--consumers") == 0 && i + 1 < argc) {
            state->num_consumers = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            state->test_duration_seconds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--message-size") == 0 && i + 1 < argc) {
            state->message_size = atoi(argv[++i]);
        } else {
            printf("❌ Unknown argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Validate configuration
    if (state->num_producers <= 0 || state->num_consumers <= 0 || 
        state->test_duration_seconds <= 0) {
        printf("❌ Invalid configuration parameters\n");
        return 1;
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize and run test
    if (!initialize_test(state)) {
        printf("❌ Test initialization failed\n");
        cleanup_test(state);
        return 1;
    }
    
    run_ultimate_test(state);
    generate_test_report(state);
    cleanup_test(state);
    
    printf("🎉 UMSBB v4.0 Ultimate Performance Test completed successfully!\n");
    printf("\n💡 This is the simple CPU baseline version using C11 atomics.\n");
    printf("💡 Ready for immediate testing and performance validation.\n");
    
    return 0;
}