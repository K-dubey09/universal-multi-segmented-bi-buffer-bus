#include "gpu_delegate.h"
#include <stdio.h>

bool gpu_available() {
    // Stub: replace with actual GPU detection logic
    return false;
}

bool try_gpu_execute(void* ptr, size_t size) {
    if (!gpu_available()) return false;
    printf("GPU executed %zu bytes\n", size);
    return true;
}