#include "event_scheduler.h"
#include "portable_atomic.h"

void event_init(EventScheduler* e) {
    atomic_init_bool(&e->signal, false);
}

void event_signal(EventScheduler* e) {
    atomic_store_bool(&e->signal, true);
}

bool event_check(EventScheduler* e) {
    return atomic_load_bool(&e->signal);
}

void event_clear(EventScheduler* e) {
    atomic_store_bool(&e->signal, false);
}