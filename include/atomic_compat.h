#ifndef ATOMIC_COMPAT_H
#define ATOMIC_COMPAT_H

// Platform-specific atomic operations compatibility layer
#include <stdint.h>

#ifdef _MSC_VER
    // MSVC specific includes and definitions
    // Prevent windows.h from pulling in winsock.h (which conflicts with winsock2.h)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <intrin.h>
    
    // Use MSVC intrinsics instead of C11 atomics
    #define _Atomic(T) volatile T
    #define _Alignas(x) __declspec(align(x))

    // Public macro used across the project for annotating atomic fields
    #ifndef UMSBB_ATOMIC
    #define UMSBB_ATOMIC(T) volatile T
    #endif
    
    typedef volatile long atomic_int;
    typedef volatile unsigned long atomic_uint;
    typedef volatile unsigned int atomic_uint32_t;
    typedef volatile unsigned long long atomic_uint64_t;
    typedef volatile long long atomic_llong;
    typedef volatile unsigned long long atomic_ullong;
    typedef volatile void* atomic_ptr;
    
    // Memory ordering (simplified for MSVC)
    typedef enum {
        memory_order_relaxed,
        memory_order_consume,
        memory_order_acquire,
        memory_order_release,
        memory_order_acq_rel,
        memory_order_seq_cst
    } memory_order;
    
    // Atomic operations using MSVC intrinsics
    #define atomic_load(ptr) (*(ptr))
    #define atomic_store(ptr, val) (*(ptr) = (val))
    #define atomic_fetch_add(ptr, val) (_InterlockedExchangeAdd((volatile long*)(ptr), (val)))
    #define atomic_fetch_sub(ptr, val) (_InterlockedExchangeAdd((volatile long*)(ptr), -(val)))
    #define atomic_compare_exchange_weak(ptr, expected, desired) \
        (_InterlockedCompareExchange((volatile long*)(ptr), (desired), *(expected)) == *(expected))
    #define atomic_compare_exchange_strong(ptr, expected, desired) \
        (_InterlockedCompareExchange((volatile long*)(ptr), (desired), *(expected)) == *(expected))
    #define atomic_exchange(ptr, val) (_InterlockedExchange((volatile long*)(ptr), (val)))
    
    // Memory barriers
    #define atomic_thread_fence(order) MemoryBarrier()
    
#elif defined(__GNUC__) || defined(__clang__)
    // GCC/Clang - use C11 atomics or GCC builtins
    #if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
        // C11 atomics available
        #include <stdatomic.h>
        #define _Alignas(x) _Alignas(x)
        // Additional type definitions for compatibility
        typedef _Atomic(uint32_t) atomic_uint32_t;
        typedef _Atomic(uint64_t) atomic_uint64_t;
    #else
        // Fallback to GCC builtins
        #define _Atomic(T) volatile T
        #define _Alignas(x) __attribute__((aligned(x)))
        
        typedef volatile int atomic_int;
        typedef volatile unsigned int atomic_uint;
        typedef volatile uint32_t atomic_uint32_t;
        typedef volatile uint64_t atomic_uint64_t;
        typedef volatile long long atomic_llong;
        typedef volatile unsigned long long atomic_ullong;
        typedef volatile void* atomic_ptr;
        
        typedef enum {
            memory_order_relaxed,
            memory_order_consume,
            memory_order_acquire,
            memory_order_release,
            memory_order_acq_rel,
            memory_order_seq_cst
        } memory_order;
        
        #define atomic_load(ptr) (*(ptr))
        #define atomic_store(ptr, val) (*(ptr) = (val))
        #define atomic_fetch_add(ptr, val) __sync_fetch_and_add((ptr), (val))
        #define atomic_fetch_sub(ptr, val) __sync_fetch_and_sub((ptr), (val))
        #define atomic_compare_exchange_weak(ptr, expected, desired) \
            __sync_bool_compare_and_swap((ptr), *(expected), (desired))
        #define atomic_compare_exchange_strong(ptr, expected, desired) \
            __sync_bool_compare_and_swap((ptr), *(expected), (desired))
        #define atomic_exchange(ptr, val) __sync_lock_test_and_set((ptr), (val))
        
        #define atomic_thread_fence(order) __sync_synchronize()
    #endif
    
#else
    // Fallback for unknown compilers - basic volatile operations
    #warning "Unknown compiler, using basic volatile operations (not thread-safe)"
    #define _Atomic(T) volatile T
    #define _Alignas(x) 
    
    typedef volatile int atomic_int;
    typedef volatile unsigned int atomic_uint;
    typedef volatile unsigned int atomic_uint32_t;
    typedef volatile unsigned long long atomic_uint64_t;
    typedef volatile long long atomic_llong;
    typedef volatile unsigned long long atomic_ullong;
    typedef volatile void* atomic_ptr;
    
    typedef enum {
        memory_order_relaxed,
        memory_order_consume,
        memory_order_acquire,
        memory_order_release,
        memory_order_acq_rel,
        memory_order_seq_cst
    } memory_order;
    
    #define atomic_load(ptr) (*(ptr))
    #define atomic_store(ptr, val) (*(ptr) = (val))
    #define atomic_fetch_add(ptr, val) (*(ptr) += (val), *(ptr) - (val))
    #define atomic_fetch_sub(ptr, val) (*(ptr) -= (val), *(ptr) + (val))
    #define atomic_compare_exchange_weak(ptr, expected, desired) \
        (*(ptr) == *(expected) ? (*(ptr) = (desired), 1) : (*(expected) = *(ptr), 0))
    #define atomic_compare_exchange_strong(ptr, expected, desired) \
        (*(ptr) == *(expected) ? (*(ptr) = (desired), 1) : (*(expected) = *(ptr), 0))
    #define atomic_exchange(ptr, val) (*(ptr) = (val))
    
    #define atomic_thread_fence(order) 
    
#endif

#endif // ATOMIC_COMPAT_H