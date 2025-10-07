#pragma once
#include <stddef.h>
#include <stdint.h>

#define SOMA_ALIGNMENT 64

/*
 * MSVC's C compiler doesn't fully support C11 <stdatomic.h> or _Alignas in
 * the same way as clang/gcc. Provide portable fallbacks so the project builds
 * with MSVC and with clang/gcc (including emscripten).
 */
#include "portable_atomic.h"

#if defined(_MSC_VER)
#  define SOMA_ALIGNED_TYPE(type, bytes) __declspec(align(bytes)) type
#else
#  define SOMA_ALIGNED_TYPE(type, bytes) _Alignas(bytes) type
#endif

/* Message State Machine: FREE → READY → CONSUMING → FEEDBACK */
typedef enum {
    MSG_STATE_FREE = 0,      // Available for new message
    MSG_STATE_READY = 1,     // Message written, ready to consume
    MSG_STATE_CONSUMING = 2, // Being processed
    MSG_STATE_FEEDBACK = 3   // Processing complete, ready for feedback
} MessageState;

typedef struct {
        void* regionA;      // Primary buffer
        void* regionB;      // Secondary buffer  
        void* regionC;      // Feedback/metadata region
        size_t capacity;

        SOMA_ALIGNED_TYPE(atomic_size_t, SOMA_ALIGNMENT) writeIndex;
        SOMA_ALIGNED_TYPE(atomic_size_t, SOMA_ALIGNMENT) readIndex;
        SOMA_ALIGNED_TYPE(atomic_size_t, SOMA_ALIGNMENT) commitIndex;  // Track committed messages
        SOMA_ALIGNED_TYPE(atomic_size_t, SOMA_ALIGNMENT) feedbackIndex; // Track feedback processed
} BiBuffer;

void bi_buffer_init(BiBuffer* buf, size_t cap);
void* bi_buffer_claim(BiBuffer* buf, size_t size);
void bi_buffer_commit(BiBuffer* buf, void* ptr, size_t size);
void* bi_buffer_read(BiBuffer* buf, size_t* size);
void bi_buffer_release(BiBuffer* buf);
void bi_buffer_resize(BiBuffer* buf, size_t newCap);
void bi_buffer_destroy(BiBuffer* buf);

/* State machine operations */
MessageState bi_buffer_get_message_state(BiBuffer* buf, size_t offset);
void bi_buffer_set_message_state(BiBuffer* buf, size_t offset, MessageState state);
bool bi_buffer_can_claim(BiBuffer* buf, size_t size);
void bi_buffer_advance_feedback(BiBuffer* buf);