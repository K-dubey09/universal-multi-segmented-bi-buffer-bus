#pragma once
#include <stddef.h>
#include <stdint.h>

/*
Feedback Stream

Narrates mutation flow and doctrine alignment.

Triggers:
- Capsule validated → FEEDBACK_OK
- Checksum mismatch → FEEDBACK_CORRUPTED
- GPU execution → FEEDBACK_GPU_EXECUTED
- CPU fallback → FEEDBACK_CPU_EXECUTED
- Throttled → FEEDBACK_THROTTLED
- Skipped → FEEDBACK_SKIPPED
- Idle → FEEDBACK_IDLE
*/

typedef enum {
    FEEDBACK_OK,
    FEEDBACK_CORRUPTED,
    FEEDBACK_GPU_EXECUTED,
    FEEDBACK_CPU_EXECUTED,
    FEEDBACK_THROTTLED,
    FEEDBACK_SKIPPED,
    FEEDBACK_IDLE
} FeedbackType;

typedef struct {
    uint32_t sequence;
    FeedbackType type;
    const char* note;
    uint64_t timestamp;
} FeedbackEntry;

typedef struct {
    FeedbackEntry entries[128];
    size_t count;
} FeedbackStream;

void feedback_push(FeedbackStream* stream, FeedbackEntry entry);
void feedback_render(const FeedbackStream* stream);
void feedback_clear(FeedbackStream* stream);