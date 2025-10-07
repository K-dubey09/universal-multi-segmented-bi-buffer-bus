#pragma once
#include <stddef.h>
#include <stdbool.h>

bool gpu_available();
bool try_gpu_execute(void* ptr, size_t size);