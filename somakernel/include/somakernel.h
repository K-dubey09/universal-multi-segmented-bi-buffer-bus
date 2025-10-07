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
Somakernel Bus Interface

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
} SomakernelBus;

SomakernelBus* somakernel_init(size_t bufCap, size_t arenaCap);
void somakernel_submit(SomakernelBus* bus, const char* msg, size_t size);
void somakernel_drain(SomakernelBus* bus);
void somakernel_free(SomakernelBus* bus);
void somakernel_submit_to(SomakernelBus* bus, size_t laneIndex, const char* msg, size_t size);
void somakernel_drain_from(SomakernelBus* bus, size_t laneIndex);

// 🧾 Feedback accessor for JS polling
FeedbackEntry* somakernel_get_feedback(SomakernelBus* bus, size_t* count);