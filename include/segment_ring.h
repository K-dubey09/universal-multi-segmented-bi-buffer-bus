#pragma once
#include <stddef.h>
#include "bi_buffer.h"

#define MAX_AGENTS 16

typedef struct {
    BiBuffer buffers[MAX_AGENTS];
    size_t activeCount;
    size_t currentIndex;
} SegmentRing;

void segment_ring_init(SegmentRing* ring, size_t agentCount, size_t bufferCap);
BiBuffer* segment_ring_next(SegmentRing* ring);
void segment_ring_reset(SegmentRing* ring);