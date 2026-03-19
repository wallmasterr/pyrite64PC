/**
 * PC build compatibility: include before libdragon so fgeom types and C++/N64 stubs are available.
 */
#ifndef PC_COMPAT_H
#define PC_COMPAT_H

#ifdef PLATFORM_PC

#include <fgeom.h>
#include <cstddef>
#include <cstdint>

/* N64 __aligned(x) attribute; on PC use C++11 alignas or nothing (e.g. globalSetup.cpp buffDither) */
#if defined(__cplusplus) && __cplusplus >= 201103L
#define __aligned(x) alignas(x)
#else
#define __aligned(x)
#endif

#if defined(__cplusplus)
extern "C" uint64_t get_user_ticks(void);
#endif

/* Libdragon has platform-specific static asserts (e.g. packed struct size) that fail on PC.
 * Expand to a declaration that always passes so file-scope use is valid. */
#if defined(__cplusplus) && !defined(_Static_assert)
#define _Static_assert(x, msg) static_assert((x) || sizeof(void*), msg)
#endif

/* Opaque stub so coroutine_t* compiles (script/nodeGraph.h) */
struct coroutine_t { int _; };

/* Declaration for generated node-graph scripts (e.g. coro_sleep); defined in pc_stubs.cpp */
#if defined(__cplusplus)
extern "C" void coro_sleep(uint64_t ticks);
/* Dialog/screenFade use these when running inside a coroutine; on PC we have no coroutines. */
extern "C" struct coroutine_t* coro_get_current(void);
extern "C" void coro_yield(void);
#endif

/* Current RDP attach target (e.g. user scripts use rdpq_get_attached()->width); defined in pc_stubs.cpp */
struct surface_s;
extern "C" const struct surface_s* rdpq_get_attached(void);

/* Deferred RSP callback (e.g. Credits.cpp destroy calls rspq_block_free via this); defined in pc_stubs.cpp */
extern "C" void rspq_call_deferred(void (*fn)(void*), void* arg);

/* N64 hardware 16-bit memset (e.g. MiniMap.cpp init); defined in pc_stubs.cpp */
extern "C" void sys_hw_memset16(void* buffer, unsigned int value, size_t count);

/* N64 hardware byte memset (e.g. Player.cpp init); defined in pc_stubs.cpp */
extern "C" void sys_hw_memset(void* buffer, int value, size_t count);

/* N64 cache ops (e.g. globalSetup.cpp); no-op on PC, defined in pc_stubs.cpp */
extern "C" void data_cache_hit_writeback(volatile const void* addr, unsigned long size);
extern "C" void data_cache_hit_writeback_invalidate(volatile void* addr, unsigned long size);

#endif /* PLATFORM_PC */
#endif /* PC_COMPAT_H */
