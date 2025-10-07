#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "bi_buffer.h"

typedef struct {
    size_t threshold;
} FlowControl;

/* Initialize the flow control with a capacity threshold. */
void flow_control_init(FlowControl* ctrl, size_t threshold);

/* Return true if the buffer should be throttled according to the control. */
bool flow_should_throttle(BiBuffer* buf, FlowControl* ctrl);