#include "somakernel.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

SomakernelBus* somakernel_init(size_t bufCap, size_t arenaCap) {
    SomakernelBus* bus = malloc(sizeof(SomakernelBus));
    segment_ring_init(&bus->ring, 4, bufCap);
    arena_init(&bus->arena, arenaCap);
    feedback_clear(&bus->feedback);
    batch_init(&bus->batch, 1, 8, 1);
    flow_control_init(&bus->flow, bufCap * 0.8);
    event_init(&bus->scheduler);
    bus->sequence = 0;
    return bus;
}

void somakernel_submit(SomakernelBus* bus, const char* msg, size_t size) {
    size_t lane = bus->ring.currentIndex;
    somakernel_submit_to(bus, lane, msg, size);
    bus->ring.currentIndex = (lane + 1) % bus->ring.activeCount;
}

void somakernel_submit_to(SomakernelBus* bus, size_t laneIndex, const char* msg, size_t size) {
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

    MessageCapsule* cap = arena_alloc(&bus->arena, sizeof(MessageCapsule));
    capsule_wrap(cap, bus->sequence++, msg, size);

    void* ptr = bi_buffer_claim(target, sizeof(MessageCapsule));
    if (ptr) {
        memcpy(ptr, cap, sizeof(MessageCapsule));
        bi_buffer_commit(target, ptr, sizeof(MessageCapsule));
        event_signal(&bus->scheduler);
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

void somakernel_drain(SomakernelBus* bus) {
    size_t lane = bus->ring.currentIndex;
    somakernel_drain_from(bus, lane);
    bus->ring.currentIndex = (lane + 1) % bus->ring.activeCount;
}

void somakernel_drain_from(SomakernelBus* bus, size_t laneIndex) {
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
        fb.note = "Checksum mismatch";
    } else if (try_gpu_execute(cap->payload, cap->size)) {
        fb.type = FEEDBACK_GPU_EXECUTED;
        fb.note = "Executed on GPU";
    } else {
        fb.type = FEEDBACK_CPU_EXECUTED;
        fb.note = "Executed on CPU";
    }

    feedback_push(&bus->feedback, fb);
    bi_buffer_release(buf);
    
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

FeedbackEntry* somakernel_get_feedback(SomakernelBus* bus, size_t* count) {
    *count = bus->feedback.count;
    return bus->feedback.entries;
}

void somakernel_free(SomakernelBus* bus) {
    for (size_t i = 0; i < bus->ring.activeCount; ++i) {
        bi_buffer_destroy(&bus->ring.buffers[i]);
    }
    if (bus->arena.memory) free(bus->arena.memory);
    free(bus);
}