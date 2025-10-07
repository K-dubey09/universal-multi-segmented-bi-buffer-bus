#include "universal_multi_segmented_bi_buffer_bus.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

UniversalMultiSegmentedBiBufferBus* umsbb_init(size_t bufCap, size_t arenaCap) {
    UniversalMultiSegmentedBiBufferBus* bus = malloc(sizeof(UniversalMultiSegmentedBiBufferBus));
    segment_ring_init(&bus->ring, 4, bufCap);
    arena_init(&bus->arena, arenaCap);
    feedback_clear(&bus->feedback);
    batch_init(&bus->batch, 1, 8, 1);
    flow_control_init(&bus->flow, bufCap * 0.8);
    event_init(&bus->scheduler);
    bus->sequence = 0;
    return bus;
}

void umsbb_submit(UniversalMultiSegmentedBiBufferBus* bus, const char* msg, size_t size) {
    size_t lane = bus->ring.currentIndex;
    umsbb_submit_to(bus, lane, msg, size);
    bus->ring.currentIndex = (lane + 1) % bus->ring.activeCount;
}

void umsbb_submit_to(UniversalMultiSegmentedBiBufferBus* bus, size_t laneIndex, const char* msg, size_t size) {
    if (laneIndex >= bus->ring.activeCount) return;
    BiBuffer* target = &bus->ring.buffers[laneIndex];

    if (flow_should_throttle(target, &bus->flow)) {
        FeedbackEntry fb = {
            .sequence = bus->sequence,
            .type = FEEDBACK_THROTTLED,
            .note = "High-water mark reached",
            .timestamp = (uint64_t)time(NULL)
        };
        feedback_push(&bus->feedback, fb);
        return;
    }

    // Check adaptive batching - accumulate multiple messages if beneficial
    size_t batchSize = batch_next(&bus->batch, 85); // Assume 85% success rate for now
    
    MessageCapsule* cap = arena_alloc(&bus->arena, sizeof(MessageCapsule));
    if (!cap) {
        FeedbackEntry fb = {
            .sequence = bus->sequence,
            .type = FEEDBACK_SKIPPED,
            .note = "Arena allocation failed",
            .timestamp = (uint64_t)time(NULL)
        };
        feedback_push(&bus->feedback, fb);
        return;
    }
    
    capsule_wrap(cap, bus->sequence++, msg, size);

    void* ptr = bi_buffer_claim(target, sizeof(MessageCapsule));
    if (ptr) {
        memcpy(ptr, cap, sizeof(MessageCapsule));
        bi_buffer_commit(target, ptr, sizeof(MessageCapsule));
        event_signal(&bus->scheduler);
        
        // Track successful submission for batching feedback
        FeedbackEntry fb = {
            .sequence = cap->header.sequence,
            .type = FEEDBACK_OK,
            .note = "Message submitted successfully",
            .timestamp = (uint64_t)time(NULL)
        };
        feedback_push(&bus->feedback, fb);
    } else {
        FeedbackEntry fb = {
            .sequence = cap->header.sequence,
            .type = FEEDBACK_SKIPPED,
            .note = "Buffer claim failed",
            .timestamp = (uint64_t)time(NULL)
        };
        feedback_push(&bus->feedback, fb);
    }
}

void umsbb_drain(UniversalMultiSegmentedBiBufferBus* bus) {
    size_t lane = bus->ring.currentIndex;
    umsbb_drain_from(bus, lane);
    bus->ring.currentIndex = (lane + 1) % bus->ring.activeCount;
}

void umsbb_drain_from(UniversalMultiSegmentedBiBufferBus* bus, size_t laneIndex) {
    if (laneIndex >= bus->ring.activeCount) return;
    BiBuffer* buf = &bus->ring.buffers[laneIndex];

    size_t size;
    void* ptr = bi_buffer_read(buf, &size);
    if (!ptr) {
        FeedbackEntry fb = {
            .sequence = bus->sequence,
            .type = FEEDBACK_IDLE,
            .note = "No data to drain",
            .timestamp = (uint64_t)time(NULL)
        };
        feedback_push(&bus->feedback, fb);
        return;
    }

    MessageCapsule* cap = (MessageCapsule*)ptr;
    FeedbackEntry fb = {
        .sequence = cap->header.sequence,
        .timestamp = (uint64_t)time(NULL),
        .note = "",
        .type = FEEDBACK_OK
    };

    if (!capsule_validate(cap)) {
        fb.type = FEEDBACK_CORRUPTED;
        fb.note = "Checksum mismatch - state machine integrity failure";
    } else if (try_gpu_execute(cap->payload, cap->size)) {
        fb.type = FEEDBACK_GPU_EXECUTED;
        fb.note = "GPU acceleration successful";
        // Update batch performance metrics
        batch_next(&bus->batch, 95); // High success rate for GPU
    } else {
        fb.type = FEEDBACK_CPU_EXECUTED;
        fb.note = "CPU fallback execution";
        // Update batch performance metrics  
        batch_next(&bus->batch, 75); // Lower success rate for CPU
    }

    feedback_push(&bus->feedback, fb);
    bi_buffer_release(buf); // This transitions through FEEDBACK → FREE
    
    // Only clear event if no more data is available across all buffers
    bool hasMoreData = false;
    for (size_t i = 0; i < bus->ring.activeCount; ++i) {
        size_t testSize;
        if (bi_buffer_read(&bus->ring.buffers[i], &testSize)) {
            hasMoreData = true;
            break;
        }
    }
    if (!hasMoreData) {
        event_clear(&bus->scheduler);
    }
}

FeedbackEntry* umsbb_get_feedback(UniversalMultiSegmentedBiBufferBus* bus, size_t* count) {
    *count = bus->feedback.count;
    return bus->feedback.entries;
}

void umsbb_free(UniversalMultiSegmentedBiBufferBus* bus) {
    for (size_t i = 0; i < bus->ring.activeCount; ++i) {
        bi_buffer_destroy(&bus->ring.buffers[i]);
    }
    if (bus->arena.memory) free(bus->arena.memory);
    free(bus);
}