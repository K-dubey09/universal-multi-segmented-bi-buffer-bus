#include "arena_allocator.h"
#include <stdlib.h>
#include <string.h>

void arena_init(ArenaAllocator* arena, size_t cap) {
    arena->memory = malloc(cap);
    arena->capacity = cap;
    arena->offset = 0;
}

void* arena_alloc(ArenaAllocator* arena, size_t size) {
    if (arena->offset + size > arena->capacity) return NULL;
    void* ptr = arena->memory + arena->offset;
    arena->offset += size;
    return ptr;
}

void arena_reset(ArenaAllocator* arena) {
    arena->offset = 0;
}