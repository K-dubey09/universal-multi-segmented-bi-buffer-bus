#include "bi_buffer.h"
#include "portable_atomic.h"
#include <stdlib.h>
#include <string.h>

/* aligned allocation compatibility */
#if defined(_MSC_VER)
#  include <malloc.h>
#  define soma_aligned_alloc(align, sz) _aligned_malloc(sz, align)
#  define soma_aligned_free(ptr) _aligned_free(ptr)
#else
#  include <stdlib.h>
#  define soma_aligned_alloc(align, sz) aligned_alloc(align, sz)
#  define soma_aligned_free(ptr) free(ptr)
#endif

void bi_buffer_init(BiBuffer* buf, size_t cap) {
    buf->capacity = cap;
    buf->regionA = soma_aligned_alloc(SOMA_ALIGNMENT, cap);
    buf->regionB = soma_aligned_alloc(SOMA_ALIGNMENT, cap);
    atomic_store_size(&buf->writeIndex, 0);
    atomic_store_size(&buf->readIndex, 0);
}

void* bi_buffer_claim(BiBuffer* buf, size_t size) {
    size_t index = atomic_load_size(&buf->writeIndex);
    if (index + size > buf->capacity) return NULL;
    return (uint8_t*)buf->regionA + index;
}

void bi_buffer_commit(BiBuffer* buf, void* ptr, size_t size) {
    atomic_fetch_add_size(&buf->writeIndex, size);
}

void* bi_buffer_read(BiBuffer* buf, size_t* size) {
    size_t index = atomic_load_size(&buf->readIndex);
    if (index >= atomic_load_size(&buf->writeIndex)) return NULL;
    *size = atomic_load_size(&buf->writeIndex) - index;
    return (uint8_t*)buf->regionA + index;
}

void bi_buffer_release(BiBuffer* buf) {
    atomic_store_size(&buf->readIndex, atomic_load_size(&buf->writeIndex));
}

void bi_buffer_resize(BiBuffer* buf, size_t newCap) {
    soma_aligned_free(buf->regionA);
    soma_aligned_free(buf->regionB);
    buf->capacity = newCap;
    buf->regionA = soma_aligned_alloc(SOMA_ALIGNMENT, newCap);
    buf->regionB = soma_aligned_alloc(SOMA_ALIGNMENT, newCap);
    atomic_store_size(&buf->writeIndex, 0);
    atomic_store_size(&buf->readIndex, 0);
}

void bi_buffer_destroy(BiBuffer* buf) {
    if (!buf) return;
    if (buf->regionA) soma_aligned_free(buf->regionA);
    if (buf->regionB) soma_aligned_free(buf->regionB);
    buf->regionA = NULL;
    buf->regionB = NULL;
    buf->capacity = 0;
}