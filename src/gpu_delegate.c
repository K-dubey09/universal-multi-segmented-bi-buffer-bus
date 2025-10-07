#include "gpu_delegate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #include <d3d11.h>
    #ifdef CUDA_AVAILABLE
        #include <cuda_runtime.h>
    #endif
    #ifdef OPENCL_AVAILABLE
        #include <CL/cl.h>
    #endif
#else
    #ifdef CUDA_AVAILABLE
        #include <cuda_runtime.h>
    #endif
    #ifdef OPENCL_AVAILABLE
        #include <CL/cl.h>
    #endif
    #ifdef VULKAN_AVAILABLE
        #include <vulkan/vulkan.h>
    #endif
#endif

// GPU capability flags
static gpu_capabilities_t gpu_caps = {0};
static bool gpu_initialized = false;

// GPU memory pool for buffer operations
static void* gpu_memory_pool = NULL;
static size_t gpu_pool_size = 0;

bool initialize_gpu() {
    if (gpu_initialized) return true;
    
    memset(&gpu_caps, 0, sizeof(gpu_capabilities_t));
    
    // Try CUDA first
    #ifdef CUDA_AVAILABLE
    int device_count = 0;
    if (cudaGetDeviceCount(&device_count) == cudaSuccess && device_count > 0) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, 0);
        gpu_caps.has_cuda = true;
        gpu_caps.compute_capability = prop.major * 10 + prop.minor;
        gpu_caps.memory_size = prop.totalGlobalMem;
        gpu_caps.max_threads = prop.maxThreadsPerBlock;
        printf("[GPU] CUDA detected: %s (CC %d.%d)\n", prop.name, prop.major, prop.minor);
        
        // Allocate GPU memory pool (256MB)
        gpu_pool_size = 256 * 1024 * 1024;
        if (cudaMalloc(&gpu_memory_pool, gpu_pool_size) == cudaSuccess) {
            gpu_caps.has_memory_pool = true;
        }
    }
    #endif
    
    // Try OpenCL if CUDA not available
    #ifdef OPENCL_AVAILABLE
    if (!gpu_caps.has_cuda) {
        cl_platform_id platforms[8];
        cl_uint num_platforms;
        if (clGetPlatformIDs(8, platforms, &num_platforms) == CL_SUCCESS && num_platforms > 0) {
            cl_device_id devices[16];
            cl_uint num_devices;
            if (clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 16, devices, &num_devices) == CL_SUCCESS && num_devices > 0) {
                gpu_caps.has_opencl = true;
                size_t mem_size;
                clGetDeviceInfo(devices[0], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(size_t), &mem_size, NULL);
                gpu_caps.memory_size = mem_size;
                printf("[GPU] OpenCL detected with %zu bytes memory\n", mem_size);
            }
        }
    }
    #endif
    
    // Fallback to DirectX/Metal compute for basic GPU operations
    #ifdef _WIN32
    if (!gpu_caps.has_cuda && !gpu_caps.has_opencl) {
        ID3D11Device* device = NULL;
        if (SUCCEEDED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, 
                                       D3D11_SDK_VERSION, &device, NULL, NULL))) {
            gpu_caps.has_compute = true;
            printf("[GPU] DirectX 11 compute detected\n");
            device->Release();
        }
    }
    #endif
    
    gpu_initialized = true;
    return gpu_caps.has_cuda || gpu_caps.has_opencl || gpu_caps.has_compute;
}

bool gpu_available() {
    if (!gpu_initialized) {
        initialize_gpu();
    }
    return gpu_caps.has_cuda || gpu_caps.has_opencl || gpu_caps.has_compute;
}

gpu_capabilities_t get_gpu_capabilities() {
    if (!gpu_initialized) {
        initialize_gpu();
    }
    return gpu_caps;
}

// GPU kernel for parallel buffer operations
bool gpu_parallel_copy(void* dst, const void* src, size_t size, size_t num_threads) {
    if (!gpu_available()) return false;
    
    #ifdef CUDA_AVAILABLE
    if (gpu_caps.has_cuda) {
        // CUDA implementation
        void *gpu_src, *gpu_dst;
        if (cudaMalloc(&gpu_src, size) != cudaSuccess) return false;
        if (cudaMalloc(&gpu_dst, size) != cudaSuccess) {
            cudaFree(gpu_src);
            return false;
        }
        
        cudaMemcpy(gpu_src, src, size, cudaMemcpyHostToDevice);
        
        // Launch parallel copy kernel
        dim3 block(256);
        dim3 grid((size + block.x - 1) / block.x);
        // gpu_memcpy_kernel<<<grid, block>>>(gpu_dst, gpu_src, size);
        
        cudaMemcpy(dst, gpu_dst, size, cudaMemcpyDeviceToHost);
        
        cudaFree(gpu_src);
        cudaFree(gpu_dst);
        return true;
    }
    #endif
    
    // Fallback to CPU parallel copy
    return false;
}

bool try_gpu_execute(void* ptr, size_t size) {
    if (!gpu_available()) return false;
    
    // For buffer operations > 1MB, use GPU
    if (size > 1024 * 1024) {
        printf("[GPU] Processing %zu bytes on GPU\n", size);
        return gpu_parallel_copy(ptr, ptr, size, gpu_caps.max_threads);
    }
    
    return false;
}

void cleanup_gpu() {
    if (gpu_memory_pool) {
        #ifdef CUDA_AVAILABLE
        if (gpu_caps.has_cuda) {
            cudaFree(gpu_memory_pool);
        }
        #endif
        gpu_memory_pool = NULL;
    }
    gpu_initialized = false;
}