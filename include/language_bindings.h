#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Language binding types
typedef enum {
    LANG_C = 0,
    LANG_CPP,
    LANG_PYTHON,
    LANG_JAVASCRIPT,
    LANG_RUST,
    LANG_GO,
    LANG_JAVA,
    LANG_CSHARP,
    LANG_KOTLIN,
    LANG_SWIFT
} language_type_t;

// Universal data structure for cross-language compatibility
typedef struct {
    void* data;
    size_t size;
    uint32_t type_id;
    language_type_t source_lang;
} universal_data_t;

// Function pointer types for language callbacks
typedef void (*lang_callback_t)(universal_data_t* data);
typedef bool (*lang_validator_t)(const universal_data_t* data);
typedef void* (*lang_allocator_t)(size_t size);
typedef void (*lang_deallocator_t)(void* ptr);

// Language runtime structure
typedef struct {
    language_type_t lang_type;
    const char* lang_name;
    lang_allocator_t allocator;
    lang_deallocator_t deallocator;
    lang_callback_t error_handler;
    lang_validator_t data_validator;
    void* runtime_context;
} language_runtime_t;

// Auto-scaling configuration
typedef struct {
    uint32_t min_producers;
    uint32_t max_producers;
    uint32_t min_consumers;
    uint32_t max_consumers;
    uint32_t scale_threshold_percent;
    uint32_t scale_cooldown_ms;
    bool gpu_preferred;
    bool auto_balance_load;
} scaling_config_t;

// Core FFI functions
bool register_language_runtime(language_type_t lang, const language_runtime_t* runtime);
bool unregister_language_runtime(language_type_t lang);
language_runtime_t* get_language_runtime(language_type_t lang);

// Universal data conversion
universal_data_t* create_universal_data(void* data, size_t size, uint32_t type_id, language_type_t lang);
bool convert_data_for_language(const universal_data_t* src, universal_data_t* dst, language_type_t target_lang);
void free_universal_data(universal_data_t* data);

// Auto-scaling functions
bool configure_auto_scaling(const scaling_config_t* config);
scaling_config_t get_scaling_config();
void trigger_scale_evaluation();
uint32_t get_optimal_producer_count();
uint32_t get_optimal_consumer_count();

// Direct language bindings (no API wrapper)
void* umsbb_create_direct(size_t buffer_size, uint32_t segment_count, language_type_t lang);
bool umsbb_submit_direct(void* bus_handle, const universal_data_t* data);
universal_data_t* umsbb_drain_direct(void* bus_handle, language_type_t target_lang);
void umsbb_destroy_direct(void* bus_handle);

#ifdef __cplusplus
}
#endif