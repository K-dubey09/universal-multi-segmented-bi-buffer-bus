#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t* memory;
    size_t capacity;
    size_t offset;
} ArenaAllocator;

void arena_init(ArenaAllocator* arena, size_t cap);
void* arena_alloc(ArenaAllocator* arena, size_t size);
void arena_reset(ArenaAllocator* arena);