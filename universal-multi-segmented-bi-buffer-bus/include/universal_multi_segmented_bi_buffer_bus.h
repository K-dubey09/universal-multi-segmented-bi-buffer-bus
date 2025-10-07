#pragma once
#include "bi_buffer.h"
#include "arena_allocator.h"
#include "capsule.h"
#include "feedback_stream.h"
#include "adaptive_batch.h"
#include "segment_ring.h"
#include "gpu_delegate.h"
#include "flow_control.h"
#include "event_scheduler.h"

/*
Universal Multi-Segmented Bi-Buffer Bus Interface

Mutation-grade conductor for capsule transport.

Architecture:
- Region A/B/C for contiguous memory
- Lock-free atomics (FREE → READY → CONSUMING → FEEDBACK)
- Zero-copy messaging (pointer + size)
- Batching for reduced interop overhead
- 64-byte cache-line alignment
- Dynamic sizing and agent attach/detach
- Event-driven scheduling (wake/drain/idle)
- Backpressure via high-water marks
- Robustness via sequence headers and checksums
- Multi-segment scalability and adaptive batching
*/

typedef struct {
    SegmentRing ring;
    ArenaAllocator arena;
    FeedbackStream feedback;
    AdaptiveBatch batch;
    FlowControl flow;
    EventScheduler scheduler;
    size_t sequence;
} UniversalMultiSegmentedBiBufferBus;

UniversalMultiSegmentedBiBufferBus* umsbb_init(size_t bufCap, size_t arenaCap);
void umsbb_submit(UniversalMultiSegmentedBiBufferBus* bus, const char* msg, size_t size);
void umsbb_drain(UniversalMultiSegmentedBiBufferBus* bus);
void umsbb_free(UniversalMultiSegmentedBiBufferBus* bus);
void umsbb_submit_to(UniversalMultiSegmentedBiBufferBus* bus, size_t laneIndex, const char* msg, size_t size);
void umsbb_drain_from(UniversalMultiSegmentedBiBufferBus* bus, size_t laneIndex);

// 🧾 Feedback accessor for JS polling
FeedbackEntry* umsbb_get_feedback(UniversalMultiSegmentedBiBufferBus* bus, size_t* count);