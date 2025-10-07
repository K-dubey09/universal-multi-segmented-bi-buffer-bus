#include "adaptive_batch.h"

void batch_init(AdaptiveBatch* batch, size_t min, size_t max, size_t step) {
    batch->min = min;
    batch->max = max;
    batch->step = step;
    batch->current = min;
}

size_t batch_next(AdaptiveBatch* batch, size_t feedbackScore) {
    if (feedbackScore > 80 && batch->current + batch->step <= batch->max) {
        batch->current += batch->step;
    } else if (feedbackScore < 30 && batch->current > batch->min + batch->step) {
        batch->current -= batch->step;
    }
    return batch->current;
}

void batch_reset(AdaptiveBatch* batch) {
    batch->current = batch->min;
}