#include "language_bindings.h"
#include "universal_multi_segmented_bi_buffer_bus.h"
#include "gpu_delegate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

// Global state
static language_runtime_t registered_runtimes[16];
static bool runtime_initialized[16] = {false};
static scaling_config_t current_scaling_config = {0};
static pthread_mutex_t scaling_mutex = PTHREAD_MUTEX_INITIALIZER;

// Performance monitoring
static struct {
    uint32_t active_producers;
    uint32_t active_consumers;
    uint64_t total_operations;
    uint64_t gpu_operations;
    double avg_latency_us;
    time_t last_scale_time;
} performance_stats = {0};

// Language runtime registration
bool register_language_runtime(language_type_t lang, const language_runtime_t* runtime) {
    if (lang >= 16 || !runtime) return false;
    
    memcpy(&registered_runtimes[lang], runtime, sizeof(language_runtime_t));
    runtime_initialized[lang] = true;
    
    printf("[FFI] Registered runtime for %s\n", runtime->lang_name);
    return true;
}

bool unregister_language_runtime(language_type_t lang) {
    if (lang >= 16) return false;
    
    runtime_initialized[lang] = false;
    memset(&registered_runtimes[lang], 0, sizeof(language_runtime_t));
    return true;
}

language_runtime_t* get_language_runtime(language_type_t lang) {
    if (lang >= 16 || !runtime_initialized[lang]) return NULL;
    return &registered_runtimes[lang];
}

// Universal data management
universal_data_t* create_universal_data(void* data, size_t size, uint32_t type_id, language_type_t lang) {
    universal_data_t* udata = malloc(sizeof(universal_data_t));
    if (!udata) return NULL;
    
    // Try to use language-specific allocator if available
    language_runtime_t* runtime = get_language_runtime(lang);
    if (runtime && runtime->allocator) {
        udata->data = runtime->allocator(size);
    } else {
        udata->data = malloc(size);
    }
    
    if (!udata->data) {
        free(udata);
        return NULL;
    }
    
    memcpy(udata->data, data, size);
    udata->size = size;
    udata->type_id = type_id;
    udata->source_lang = lang;
    
    return udata;
}

bool convert_data_for_language(const universal_data_t* src, universal_data_t* dst, language_type_t target_lang) {
    if (!src || !dst) return false;
    
    language_runtime_t* target_runtime = get_language_runtime(target_lang);
    if (!target_runtime) {
        // Direct copy if no runtime registered
        memcpy(dst, src, sizeof(universal_data_t));
        dst->data = malloc(src->size);
        if (!dst->data) return false;
        memcpy(dst->data, src->data, src->size);
        return true;
    }
    
    // Language-specific conversion
    dst->size = src->size;
    dst->type_id = src->type_id;
    dst->source_lang = target_lang;
    
    if (target_runtime->allocator) {
        dst->data = target_runtime->allocator(src->size);
    } else {
        dst->data = malloc(src->size);
    }
    
    if (!dst->data) return false;
    memcpy(dst->data, src->data, src->size);
    
    // Validate if validator available
    if (target_runtime->data_validator && !target_runtime->data_validator(dst)) {
        if (target_runtime->deallocator) {
            target_runtime->deallocator(dst->data);
        } else {
            free(dst->data);
        }
        return false;
    }
    
    return true;
}

void free_universal_data(universal_data_t* data) {
    if (!data) return;
    
    language_runtime_t* runtime = get_language_runtime(data->source_lang);
    if (runtime && runtime->deallocator) {
        runtime->deallocator(data->data);
    } else {
        free(data->data);
    }
    
    free(data);
}

// Auto-scaling implementation
bool configure_auto_scaling(const scaling_config_t* config) {
    if (!config) return false;
    
    pthread_mutex_lock(&scaling_mutex);
    memcpy(&current_scaling_config, config, sizeof(scaling_config_t));
    
    printf("[AutoScale] Configured: producers %u-%u, consumers %u-%u, threshold %u%%\n",
           config->min_producers, config->max_producers,
           config->min_consumers, config->max_consumers,
           config->scale_threshold_percent);
    
    pthread_mutex_unlock(&scaling_mutex);
    return true;
}

scaling_config_t get_scaling_config() {
    pthread_mutex_lock(&scaling_mutex);
    scaling_config_t config = current_scaling_config;
    pthread_mutex_unlock(&scaling_mutex);
    return config;
}

void trigger_scale_evaluation() {
    pthread_mutex_lock(&scaling_mutex);
    
    time_t now = time(NULL);
    if (now - performance_stats.last_scale_time < current_scaling_config.scale_cooldown_ms / 1000) {
        pthread_mutex_unlock(&scaling_mutex);
        return; // Too soon to scale again
    }
    
    // Calculate load metrics
    double gpu_ratio = performance_stats.total_operations > 0 ? 
                      (double)performance_stats.gpu_operations / performance_stats.total_operations : 0.0;
    
    // Scale producers based on latency
    if (performance_stats.avg_latency_us > 100.0 && 
        performance_stats.active_producers < current_scaling_config.max_producers) {
        performance_stats.active_producers++;
        printf("[AutoScale] Scaled up producers to %u (latency: %.2f μs)\n", 
               performance_stats.active_producers, performance_stats.avg_latency_us);
    } else if (performance_stats.avg_latency_us < 10.0 && 
               performance_stats.active_producers > current_scaling_config.min_producers) {
        performance_stats.active_producers--;
        printf("[AutoScale] Scaled down producers to %u (latency: %.2f μs)\n", 
               performance_stats.active_producers, performance_stats.avg_latency_us);
    }
    
    // Scale consumers based on throughput
    uint32_t target_consumers = performance_stats.active_producers;
    if (current_scaling_config.gpu_preferred && gpu_available()) {
        target_consumers = (target_consumers + 1) / 2; // Fewer consumers needed with GPU
    }
    
    target_consumers = (target_consumers < current_scaling_config.min_consumers) ? 
                      current_scaling_config.min_consumers : target_consumers;
    target_consumers = (target_consumers > current_scaling_config.max_consumers) ? 
                      current_scaling_config.max_consumers : target_consumers;
    
    if (target_consumers != performance_stats.active_consumers) {
        performance_stats.active_consumers = target_consumers;
        printf("[AutoScale] Adjusted consumers to %u (GPU ratio: %.2f)\n", 
               target_consumers, gpu_ratio);
    }
    
    performance_stats.last_scale_time = now;
    pthread_mutex_unlock(&scaling_mutex);
}

uint32_t get_optimal_producer_count() {
    pthread_mutex_lock(&scaling_mutex);
    uint32_t count = performance_stats.active_producers;
    pthread_mutex_unlock(&scaling_mutex);
    return count > 0 ? count : 1;
}

uint32_t get_optimal_consumer_count() {
    pthread_mutex_lock(&scaling_mutex);
    uint32_t count = performance_stats.active_consumers;
    pthread_mutex_unlock(&scaling_mutex);
    return count > 0 ? count : 1;
}

// Direct language bindings (no API wrapper)
void* umsbb_create_direct(size_t buffer_size, uint32_t segment_count, language_type_t lang) {
    // Initialize GPU if configured for GPU preference
    if (current_scaling_config.gpu_preferred) {
        initialize_gpu();
    }
    
    // Create bus with auto-scaling consideration
    uint32_t optimal_segments = segment_count;
    if (optimal_segments == 0) {
        optimal_segments = get_optimal_producer_count() + get_optimal_consumer_count();
    }
    
    UniversalMultiSegmentedBiBufferBus* bus = umsbb_init(buffer_size, optimal_segments);
    if (!bus) return NULL;
    
    printf("[Direct] Created bus for %s with %u segments (%zu bytes each)\n",
           lang < 16 && runtime_initialized[lang] ? registered_runtimes[lang].lang_name : "Unknown",
           optimal_segments, buffer_size);
    
    return bus;
}

bool umsbb_submit_direct(void* bus_handle, const universal_data_t* data) {
    if (!bus_handle || !data) return false;
    
    UniversalMultiSegmentedBiBufferBus* bus = (UniversalMultiSegmentedBiBufferBus*)bus_handle;
    
    // Try GPU execution for large data
    bool gpu_used = false;
    if (current_scaling_config.gpu_preferred && data->size > 1024 * 1024) {
        gpu_used = try_gpu_execute(data->data, data->size);
        if (gpu_used) {
            performance_stats.gpu_operations++;
        }
    }
    
    // Submit to appropriate segment
    uint32_t segment_id = data->type_id % bus->segment_count;
    bool result = umsbb_submit_to(bus, segment_id, data->data, data->size);
    
    if (result) {
        performance_stats.total_operations++;
        // Update performance stats for auto-scaling
        trigger_scale_evaluation();
    }
    
    return result;
}

universal_data_t* umsbb_drain_direct(void* bus_handle, language_type_t target_lang) {
    if (!bus_handle) return NULL;
    
    UniversalMultiSegmentedBiBufferBus* bus = (UniversalMultiSegmentedBiBufferBus*)bus_handle;
    
    // Try draining from multiple segments
    for (uint32_t i = 0; i < bus->segment_count; i++) {
        size_t size;
        void* data = umsbb_drain_from(bus, i, &size);
        if (data && size > 0) {
            // Create universal data structure
            universal_data_t* udata = create_universal_data(data, size, i, target_lang);
            free(data); // Free original data
            
            performance_stats.total_operations++;
            return udata;
        }
    }
    
    return NULL;
}

void umsbb_destroy_direct(void* bus_handle) {
    if (!bus_handle) return;
    
    UniversalMultiSegmentedBiBufferBus* bus = (UniversalMultiSegmentedBiBufferBus*)bus_handle;
    umsbb_free(bus);
    
    printf("[Direct] Bus destroyed\n");
}