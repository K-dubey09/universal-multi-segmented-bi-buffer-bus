#include "flow_control.h"
#include "portable_atomic.h"

void flow_control_init(FlowControl* ctrl, size_t threshold) {
    if (!ctrl) return;
    ctrl->threshold = threshold;
}

bool flow_should_throttle(BiBuffer* buf, FlowControl* ctrl) {
    if (!buf || !ctrl) return false;
    /* Use atomic read of the write index to estimate size. */
    size_t write = atomic_load_size(&buf->writeIndex);
    size_t read = atomic_load_size(&buf->readIndex);
    size_t used = (write >= read) ? (write - read) : 0;
    return used > ctrl->threshold;
}