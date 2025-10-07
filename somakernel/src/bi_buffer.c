#include "bi_buffer.h"
#include "portable_atomic.h"
#include "capsule.h"
#include <stdlib.h>
#include <string.h>

/* aligned allocation compatibility */
#if defined(_MSC_VER)
#  include <malloc.h>
#  define soma_aligned_alloc(align, sz) _aligned_malloc(sz, align)
#  define soma_aligned_free(ptr) _aligned_free(ptr)
#elif defined(__MINGW32__) || defined(__MINGW64__) || defined(_WIN32)
/* MinGW/Windows doesn't have aligned_alloc, use simple malloc for now */
#  include <stdlib.h>
#  define soma_aligned_alloc(align, sz) malloc(sz)
#  define soma_aligned_free(ptr) free(ptr)
#else
#  include <stdlib.h>
#  define soma_aligned_alloc(align, sz) aligned_alloc(align, sz)
#  define soma_aligned_free(ptr) free(ptr)
#endif

void bi_buffer_init(BiBuffer* buf, size_t cap) {
    buf->capacity = cap;
    buf->regionA = soma_aligned_alloc(SOMA_ALIGNMENT, cap);
    buf->regionB = soma_aligned_alloc(SOMA_ALIGNMENT, cap);
    buf->regionC = soma_aligned_alloc(SOMA_ALIGNMENT, cap / 4); // Metadata region (smaller)
    atomic_store_size(&buf->writeIndex, 0);
    atomic_store_size(&buf->readIndex, 0);
    atomic_store_size(&buf->commitIndex, 0);
    atomic_store_size(&buf->feedbackIndex, 0);
    
    // Initialize all message states to FREE
    memset(buf->regionC, MSG_STATE_FREE, cap / 4);
}

void* bi_buffer_claim(BiBuffer* buf, size_t size) {
    size_t writeIndex = atomic_load_size(&buf->writeIndex);
    size_t readIndex = atomic_load_size(&buf->readIndex);
    
    // Check if we have space (leave some buffer to prevent wrap issues)
    if (writeIndex + size > buf->capacity || 
        (writeIndex >= readIndex && writeIndex + size > buf->capacity) ||
        (writeIndex < readIndex && writeIndex + size > readIndex)) {
        return NULL;
    }
    
    // Set state to READY (claiming the space)
    bi_buffer_set_message_state(buf, writeIndex, MSG_STATE_READY);
    return (uint8_t*)buf->regionA + writeIndex;
}

void bi_buffer_commit(BiBuffer* buf, void* ptr, size_t size) {
    size_t writeIndex = atomic_load_size(&buf->writeIndex);
    atomic_fetch_add_size(&buf->writeIndex, size);
    atomic_store_size(&buf->commitIndex, writeIndex + size);
    
    // Transition state: READY → CONSUMING (ready for consumption)
    bi_buffer_set_message_state(buf, writeIndex, MSG_STATE_CONSUMING);
}

void* bi_buffer_read(BiBuffer* buf, size_t* size) {
    size_t readIndex = atomic_load_size(&buf->readIndex);
    size_t commitIndex = atomic_load_size(&buf->commitIndex);
    
    if (readIndex >= commitIndex) return NULL;
    
    // Check if message is in CONSUMING state (ready to read)
    MessageState state = bi_buffer_get_message_state(buf, readIndex);
    if (state != MSG_STATE_CONSUMING) return NULL;
    
    // For message-based reading, return the next complete message
    size_t availableBytes = commitIndex - readIndex;
    if (availableBytes < sizeof(MessageCapsule)) return NULL;
    
    *size = sizeof(MessageCapsule);
    return (uint8_t*)buf->regionA + readIndex;
}

void bi_buffer_release(BiBuffer* buf) {
    size_t readIndex = atomic_load_size(&buf->readIndex);
    
    // Transition: CONSUMING → FEEDBACK
    bi_buffer_set_message_state(buf, readIndex, MSG_STATE_FEEDBACK);
    atomic_store_size(&buf->readIndex, readIndex + sizeof(MessageCapsule));
    
    // After feedback is processed, transition back to FREE for recycling
    bi_buffer_advance_feedback(buf);
    bi_buffer_set_message_state(buf, readIndex, MSG_STATE_FREE);
}

void bi_buffer_resize(BiBuffer* buf, size_t newCap) {
    soma_aligned_free(buf->regionA);
    soma_aligned_free(buf->regionB);
    soma_aligned_free(buf->regionC);
    buf->capacity = newCap;
    buf->regionA = soma_aligned_alloc(SOMA_ALIGNMENT, newCap);
    buf->regionB = soma_aligned_alloc(SOMA_ALIGNMENT, newCap);
    buf->regionC = soma_aligned_alloc(SOMA_ALIGNMENT, newCap / 4);
    atomic_store_size(&buf->writeIndex, 0);
    atomic_store_size(&buf->readIndex, 0);
    atomic_store_size(&buf->commitIndex, 0);
    atomic_store_size(&buf->feedbackIndex, 0);
    memset(buf->regionC, MSG_STATE_FREE, newCap / 4);
}

void bi_buffer_destroy(BiBuffer* buf) {
    if (!buf) return;
    if (buf->regionA) soma_aligned_free(buf->regionA);
    if (buf->regionB) soma_aligned_free(buf->regionB);
    if (buf->regionC) soma_aligned_free(buf->regionC);
    buf->regionA = NULL;
    buf->regionB = NULL;
    buf->regionC = NULL;
    buf->capacity = 0;
}

/* State machine operations */
MessageState bi_buffer_get_message_state(BiBuffer* buf, size_t offset) {
    size_t stateIndex = offset / sizeof(MessageCapsule);
    if (stateIndex >= buf->capacity / 4) return MSG_STATE_FREE;
    return ((MessageState*)buf->regionC)[stateIndex];
}

void bi_buffer_set_message_state(BiBuffer* buf, size_t offset, MessageState state) {
    size_t stateIndex = offset / sizeof(MessageCapsule);
    if (stateIndex < buf->capacity / 4) {
        ((MessageState*)buf->regionC)[stateIndex] = state;
    }
}

bool bi_buffer_can_claim(BiBuffer* buf, size_t size) {
    size_t writeIndex = atomic_load_size(&buf->writeIndex);
    return (writeIndex + size <= buf->capacity);
}

void bi_buffer_advance_feedback(BiBuffer* buf) {
    size_t readIndex = atomic_load_size(&buf->readIndex);
    bi_buffer_set_message_state(buf, readIndex, MSG_STATE_FEEDBACK);
    atomic_store_size(&buf->feedbackIndex, readIndex + sizeof(MessageCapsule));
}