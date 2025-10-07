#pragma once
#include <stddef.h>

typedef struct {
    size_t current;
    size_t min;
    size_t max;
    size_t step;
} AdaptiveBatch;

void batch_init(AdaptiveBatch* batch, size_t min, size_t max, size_t step);
size_t batch_next(AdaptiveBatch* batch, size_t feedbackScore);
void batch_reset(AdaptiveBatch* batch);