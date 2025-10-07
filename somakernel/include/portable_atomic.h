#pragma once
#include <stddef.h>
#include <stdbool.h>

/* Portable, minimal atomic helpers used by somakernel.
 * On compilers with <stdatomic.h> we alias to the standard API.
 * On MSVC C builds we provide simple fallbacks using Windows
 * interlocked primitives where possible.
 */

#if defined(_MSC_VER)
#  include <windows.h>
#  include <intrin.h>

typedef volatile size_t atomic_size_t;
typedef volatile char atomic_bool;

static inline void atomic_init_bool(atomic_bool* p, bool v) { *p = v ? 1 : 0; }
static inline void atomic_store_bool(atomic_bool* p, bool v) { *p = v ? 1 : 0; }
static inline bool atomic_load_bool(atomic_bool* p) { return *p != 0; }

static inline size_t atomic_load_size(atomic_size_t* p) { return *p; }
static inline void atomic_store_size(atomic_size_t* p, size_t v) { *p = v; }
static inline size_t atomic_fetch_add_size(atomic_size_t* p, size_t v) {
#if defined(_M_X64) || defined(_M_ARM64)
    return (size_t)InterlockedExchangeAdd64((volatile LONGLONG*)p, (LONGLONG)v);
#else
    return (size_t)InterlockedExchangeAdd((volatile LONG*)p, (LONG)v);
#endif
}

#else
#  include <stdatomic.h>

typedef _Atomic size_t atomic_size_t;
typedef _Atomic _Bool atomic_bool;

static inline void atomic_init_bool(atomic_bool* p, bool v) { atomic_init(p, v); }
static inline void atomic_store_bool(atomic_bool* p, bool v) { atomic_store(p, v); }
static inline bool atomic_load_bool(atomic_bool* p) { return atomic_load(p); }

static inline size_t atomic_load_size(atomic_size_t* p) { return atomic_load(p); }
static inline void atomic_store_size(atomic_size_t* p, size_t v) { atomic_store(p, v); }
static inline size_t atomic_fetch_add_size(atomic_size_t* p, size_t v) { return atomic_fetch_add(p, v); }

#endif
