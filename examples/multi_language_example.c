#include "language_bindings.h"
#include "universal_multi_segmented_bi_buffer_bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Simulated language runtime implementations
void python_error_handler(universal_data_t* data) {
    printf("[Python Runtime] Error processing data of size %zu\n", data->size);
}

bool python_validator(const universal_data_t* data) {
    return data && data->data && data->size > 0;
}

void* python_allocator(size_t size) {
    void* ptr = malloc(size);
    printf("[Python Runtime] Allocated %zu bytes at %p\n", size, ptr);
    return ptr;
}

void python_deallocator(void* ptr) {
    printf("[Python Runtime] Deallocating %p\n", ptr);
    free(ptr);
}

// Simulated data generators for different languages
const char* generate_python_data(int index) {
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "Python message #%d: {'data': %d, 'timestamp': %ld}", 
             index, index * 42, time(NULL));
    return buffer;
}

const char* generate_js_data(int index) {
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "JavaScript message #%d: {\"type\": \"event\", \"value\": %d}", 
             index, index * 7);
    return buffer;
}

const char* generate_rust_data(int index) {
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "Rust message #%d: struct Data { id: %d, active: true }", 
             index, index);
    return buffer;
}

const char* generate_go_data(int index) {
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "Go message #%d: type Data struct { ID int; Value int } { %d, %d }", 
             index, index, index * 3);
    return buffer;
}

int main() {
    printf("=== Universal Multi-Segmented Bi-Buffer Bus ===\n");
    printf("Multi-Language Direct Binding Example\n");
    printf("GPU Acceleration & Auto-Scaling Demo\n\n");

    // Configure auto-scaling for high-performance workload
    scaling_config_t config = {
        .min_producers = 2,
        .max_producers = 8,
        .min_consumers = 2,
        .max_consumers = 6,
        .scale_threshold_percent = 80,
        .scale_cooldown_ms = 500,
        .gpu_preferred = true,
        .auto_balance_load = true
    };
    
    if (!configure_auto_scaling(&config)) {
        printf("Failed to configure auto-scaling\n");
        return 1;
    }

    // Register Python runtime
    language_runtime_t python_runtime = {
        .lang_type = LANG_PYTHON,
        .lang_name = "Python",
        .allocator = python_allocator,
        .deallocator = python_deallocator,
        .error_handler = python_error_handler,
        .data_validator = python_validator,
        .runtime_context = NULL
    };
    register_language_runtime(LANG_PYTHON, &python_runtime);

    // Create direct bus instances for each language
    void* python_bus = umsbb_create_direct(1024 * 1024, 0, LANG_PYTHON);
    void* js_bus = umsbb_create_direct(512 * 1024, 0, LANG_JAVASCRIPT);
    void* rust_bus = umsbb_create_direct(2 * 1024 * 1024, 0, LANG_RUST);
    void* go_bus = umsbb_create_direct(1024 * 1024, 0, LANG_GO);

    if (!python_bus || !js_bus || !rust_bus || !go_bus) {
        printf("Failed to create one or more bus instances\n");
        return 1;
    }

    printf("✓ Created direct bus instances for Python, JavaScript, Rust, and Go\n");
    printf("✓ Auto-scaling configured: 2-8 producers, 2-6 consumers\n");
    printf("✓ GPU acceleration enabled\n\n");

    // Simulate multi-language data exchange
    printf("=== Multi-Language Data Exchange Simulation ===\n");
    
    for (int round = 0; round < 5; round++) {
        printf("\n--- Round %d ---\n", round + 1);
        
        // Python produces data for JavaScript
        const char* py_data = generate_python_data(round);
        universal_data_t* py_udata = create_universal_data(
            (void*)py_data, strlen(py_data), 1, LANG_PYTHON);
        
        if (umsbb_submit_direct(js_bus, py_udata)) {
            printf("Python → JavaScript: %s\n", py_data);
        }
        free_universal_data(py_udata);

        // JavaScript produces data for Rust
        const char* js_data = generate_js_data(round);
        universal_data_t* js_udata = create_universal_data(
            (void*)js_data, strlen(js_data), 2, LANG_JAVASCRIPT);
        
        if (umsbb_submit_direct(rust_bus, js_udata)) {
            printf("JavaScript → Rust: %s\n", js_data);
        }
        free_universal_data(js_udata);

        // Rust produces data for Go
        const char* rust_data = generate_rust_data(round);
        universal_data_t* rust_udata = create_universal_data(
            (void*)rust_data, strlen(rust_data), 3, LANG_RUST);
        
        if (umsbb_submit_direct(go_bus, rust_udata)) {
            printf("Rust → Go: %s\n", rust_data);
        }
        free_universal_data(rust_udata);

        // Go produces data for Python
        const char* go_data = generate_go_data(round);
        universal_data_t* go_udata = create_universal_data(
            (void*)go_data, strlen(go_data), 4, LANG_GO);
        
        if (umsbb_submit_direct(python_bus, go_udata)) {
            printf("Go → Python: %s\n", go_data);
        }
        free_universal_data(go_udata);

        // Consume data from each bus
        printf("\nConsuming data:\n");

        // Consume from JavaScript bus (Python data)
        universal_data_t* js_received = umsbb_drain_direct(js_bus, LANG_JAVASCRIPT);
        if (js_received) {
            printf("JavaScript received: %.*s\n", (int)js_received->size, (char*)js_received->data);
            free_universal_data(js_received);
        }

        // Consume from Rust bus (JavaScript data)
        universal_data_t* rust_received = umsbb_drain_direct(rust_bus, LANG_RUST);
        if (rust_received) {
            printf("Rust received: %.*s\n", (int)rust_received->size, (char*)rust_received->data);
            free_universal_data(rust_received);
        }

        // Consume from Go bus (Rust data)
        universal_data_t* go_received = umsbb_drain_direct(go_bus, LANG_GO);
        if (go_received) {
            printf("Go received: %.*s\n", (int)go_received->size, (char*)go_received->data);
            free_universal_data(go_received);
        }

        // Consume from Python bus (Go data)
        universal_data_t* py_received = umsbb_drain_direct(python_bus, LANG_PYTHON);
        if (py_received) {
            printf("Python received: %.*s\n", (int)py_received->size, (char*)py_received->data);
            free_universal_data(py_received);
        }

        // Show scaling status
        printf("\nScaling Status: Producers=%u, Consumers=%u\n", 
               get_optimal_producer_count(), get_optimal_consumer_count());
        
        // Trigger scale evaluation
        trigger_scale_evaluation();
    }

    // Demonstrate large data GPU processing
    printf("\n=== GPU Acceleration Test ===\n");
    char* large_data = malloc(2 * 1024 * 1024); // 2MB
    memset(large_data, 'A', 2 * 1024 * 1024);
    large_data[2 * 1024 * 1024 - 1] = '\0';

    universal_data_t* large_udata = create_universal_data(
        large_data, 2 * 1024 * 1024, 999, LANG_PYTHON);
    
    clock_t start = clock();
    bool gpu_result = umsbb_submit_direct(python_bus, large_udata);
    clock_t end = clock();
    
    printf("Large data (2MB) processing: %s\n", gpu_result ? "SUCCESS" : "FAILED");
    printf("Processing time: %.2f ms\n", ((double)(end - start) / CLOCKS_PER_SEC) * 1000);
    
    free_universal_data(large_udata);
    free(large_data);

    // Performance summary
    printf("\n=== Performance Summary ===\n");
    scaling_config_t final_config = get_scaling_config();
    printf("Final scaling configuration:\n");
    printf("  Producers: %u-%u (GPU preferred: %s)\n", 
           final_config.min_producers, final_config.max_producers,
           final_config.gpu_preferred ? "Yes" : "No");
    printf("  Consumers: %u-%u (Auto balance: %s)\n", 
           final_config.min_consumers, final_config.max_consumers,
           final_config.auto_balance_load ? "Yes" : "No");

    // Cleanup
    umsbb_destroy_direct(python_bus);
    umsbb_destroy_direct(js_bus);
    umsbb_destroy_direct(rust_bus);
    umsbb_destroy_direct(go_bus);

    unregister_language_runtime(LANG_PYTHON);

    printf("\n✓ All buses destroyed and runtimes unregistered\n");
    printf("✓ Multi-language direct binding demo completed successfully\n");

    return 0;
}