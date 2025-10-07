/* high_water_mark.h - backpressure control */
#ifndef HIGH_WATER_MARK_H
#define HIGH_WATER_MARK_H

#include <stddef.h>

typedef struct HighWaterMark HighWaterMark;

HighWaterMark* hwm_create(size_t capacity);
void hwm_destroy(HighWaterMark* h);
int hwm_push(HighWaterMark* h, size_t n);
void hwm_pop(HighWaterMark* h, size_t n);
int hwm_is_overflow(const HighWaterMark* h);

#endif /* HIGH_WATER_MARK_H */
