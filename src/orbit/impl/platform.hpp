#pragma once

#ifdef __linux__
#include <alloca.h>
#include <cassert>
#define ORB_STACK_ALLOC(size) alloca(size)
#define ORB_FORCE_INLINE inline __attribute__((always_inline))
#define ORB_ASSERT(...) assert(__VA_ARGS__)
// Needed to prevent stack leaks in release mode
#define ORB_NEVER_INLINE __attribute__((noinline))
#else
#error "Platform not supported, please add implementations of the platform specific macros"
#endif