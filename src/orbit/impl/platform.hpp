#pragma once

#ifdef __linux__
#include <alloca.h>
#include <cassert>
#include <string_view>

#define ORB_STACK_ALLOC(size) alloca(size)
#define ORB_FORCE_INLINE inline __attribute__((always_inline))
#define ORB_ASSERT(...) assert(__VA_ARGS__)
// Needed to prevent stack leaks in release mode
#define ORB_NEVER_INLINE __attribute__((noinline))

namespace orb {
    template<auto Fn>
    constexpr std::string_view func_name() {
        constexpr static auto name = std::string_view{__PRETTY_FUNCTION__};
        constexpr static auto prefix_len = sizeof("[Fn = ");
        constexpr static auto loc = name.find("[Fn = ");
        constexpr static auto end = name.rfind("]");
        return name.substr(loc + prefix_len, end - loc - prefix_len);
    }
}
#else
#error "Platform not supported, please add implementations of the platform specific macros"
#endif