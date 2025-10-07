#include "feedback_stream.h"
#include <stdio.h>

void feedback_push(FeedbackStream* stream, FeedbackEntry entry) {
    if (stream->count < 128) {
        stream->entries[stream->count++] = entry;
    }
}

void feedback_render(const FeedbackStream* stream) {
    for (size_t i = 0; i < stream->count; ++i) {
    const FeedbackEntry* e = &stream->entries[i];
        const char* typeStr =
            (e->type == FEEDBACK_OK) ? "OK" :
            (e->type == FEEDBACK_CORRUPTED) ? "CORRUPTED" :
            (e->type == FEEDBACK_GPU_EXECUTED) ? "GPU" :
            (e->type == FEEDBACK_CPU_EXECUTED) ? "CPU" :
            (e->type == FEEDBACK_THROTTLED) ? "THROTTLED" :
            (e->type == FEEDBACK_SKIPPED) ? "SKIPPED" :
            (e->type == FEEDBACK_IDLE) ? "IDLE" : "UNKNOWN";

        printf("Capsule %u [%s]: %s @ %llu\n", e->sequence, typeStr, e->note, e->timestamp);
    }
}

void feedback_clear(FeedbackStream* stream) {
    stream->count = 0;
}