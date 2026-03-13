#pragma once
#include <cstddef>
#include <tuple>

namespace orb::impl {
    enum class FuncType {
        Member,
        Free
    };

    template <typename T>
    struct FunctionInfo;

    template <typename Return, typename... Args>
    struct FunctionInfo<Return (*)(Args...)> {
        using return_type = Return;
        using arg_types = std::tuple<Args...>;
        constexpr static auto arg_count = sizeof...(Args);
        constexpr static auto fn_type = FuncType::Free;

        template <size_t Index>
        using ArgType = std::tuple_element_t<Index, arg_types>;
    };

#define FuncInfoDecl(POSTFIX)                                                           \
    template <typename Return, typename Class, typename... Args>                        \
    struct FunctionInfo<Return (Class::*)(Args...) POSTFIX> {                           \
        using return_type = Return;                                                     \
        using arg_types = std::tuple<Args...>;                                          \
        using class_type = Class;                                                       \
                                                                                        \
        constexpr static auto arg_count = std::tuple_size_v<arg_types>;                 \
        constexpr static auto fn_type = FuncType::Member;                               \
                                                                                        \
        template <size_t Index>                                                         \
        using ArgType = std::tuple_element_t<Index, arg_types>;                         \
    };

    FuncInfoDecl();
    FuncInfoDecl(const);
    FuncInfoDecl(noexcept);
    FuncInfoDecl(const noexcept);
    FuncInfoDecl(&);
    FuncInfoDecl(const&);
    FuncInfoDecl(& noexcept);
    FuncInfoDecl(const& noexcept);
    FuncInfoDecl(&&);
    FuncInfoDecl(const&&);
    FuncInfoDecl(&& noexcept);
    FuncInfoDecl(const&& noexcept);
#undef FuncInfoDecl

    template <typename T>
    constexpr static bool free_function = FunctionInfo<T>::fn_type == FuncType::Free;
} // namespace orb::impl