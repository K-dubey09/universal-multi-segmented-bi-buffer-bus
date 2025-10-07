#pragma once
#include "portable_atomic.h"
#include <stdbool.h>

/*
Event Scheduler

Avoids busy polling. Consumers wake only when signaled.

Usage:
- Producers call event_signal() after commit
- Consumers call event_check() before drain
- If true → drain and event_clear()
*/

typedef struct {
    atomic_bool signal;
} EventScheduler;

void event_init(EventScheduler* e);
void event_signal(EventScheduler* e);
bool event_check(EventScheduler* e);
void event_clear(EventScheduler* e);