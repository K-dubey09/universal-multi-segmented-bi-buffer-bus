#include "segment_ring.h"

void segment_ring_init(SegmentRing* ring, size_t agentCount, size_t bufferCap) {
    if (agentCount > MAX_AGENTS) agentCount = MAX_AGENTS;
    ring->activeCount = agentCount;
    ring->currentIndex = 0;

    for (size_t i = 0; i < agentCount; ++i) {
        bi_buffer_init(&ring->buffers[i], bufferCap);
    }
}

BiBuffer* segment_ring_next(SegmentRing* ring) {
    BiBuffer* buf = &ring->buffers[ring->currentIndex];
    ring->currentIndex = (ring->currentIndex + 1) % ring->activeCount;
    return buf;
}

void segment_ring_reset(SegmentRing* ring) {
    ring->currentIndex = 0;
}