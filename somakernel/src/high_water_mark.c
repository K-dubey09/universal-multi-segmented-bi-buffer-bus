/* high_water_mark.c */
#include "../include/high_water_mark.h"
#include <stdlib.h>

struct HighWaterMark { size_t capacity; size_t current; };
HighWaterMark* hwm_create(size_t capacity){ HighWaterMark* h = (HighWaterMark*)malloc(sizeof(HighWaterMark)); if(!h) return NULL; h->capacity = capacity; h->current = 0; return h; }
void hwm_destroy(HighWaterMark* h){ free(h); }
int hwm_push(HighWaterMark* h, size_t n){ if(!h) return 0; h->current += n; return h->current <= h->capacity; }
void hwm_pop(HighWaterMark* h, size_t n){ if(!h) return; if(h->current > n) h->current -= n; else h->current = 0; }
int hwm_is_overflow(const HighWaterMark* h){ return h? (h->current > h->capacity) : 0; }
