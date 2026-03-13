#pragma once

#ifdef __linux__
#include <alloca.h>
#define ORB_STACK_ALLOC(size) alloca(size)
#define ORB_FORCE_INLINE inline __attribute__((always_inline))
#else
#error "Platform not supported, please add implementations of the platform specific macros"
#endif