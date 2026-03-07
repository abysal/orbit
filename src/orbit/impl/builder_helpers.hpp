#pragma once
#include "func_info.hpp"
#include "orbit/query.hpp"
#include "type_hash.hpp"

#include <limits>
#include <unordered_map>
#include <vector>

namespace orb {
    struct ComponentAccess {
        TypeHash component_hash{};
        bool is_mut_access{};
    };

    struct SystemInfo {
        std::vector<ComponentAccess> system_accesses;
        uintptr_t raw_function_pointer{};
    };

    struct ScheduleBatch {
        std::vector<ComponentAccess> batch_accesses{};
        std::vector<SystemInfo> batch_systems{};
    };

    template <typename T>
    consteval ComponentAccess compute_access() {
        constexpr static auto hash = type_hash<stored_type_t<T>>();
        constexpr static auto is_mut = is_mut_v<T>;

        return ComponentAccess{ hash, is_mut };
    }

    template <typename T>
    consteval size_t query_start_index() {
        using fn_info = impl::FunctionInfo<T>;
        size_t index{ std::numeric_limits<size_t>::max() };

        using seq = std::make_index_sequence<fn_info::arg_count>;

        auto locate = [&]<size_t arg_index>() {
            using t = fn_info::template ArgType<arg_index>;
            if (is_query_v<t> && index == std::numeric_limits<size_t>::max()) {
                index = arg_index;
            }
        };

        auto iterate = [&]<size_t... Is>(std::index_sequence<Is...>) {
            (locate.template operator()<Is>(), ...);
        };

        iterate(seq{});

        return index;
    }

    namespace test {
        void fn(int, int, Query<int, float>) {
        }
        void fn2(Query<int, float>) {
        }

        static_assert(query_start_index<decltype(&fn)>() == 2);
        static_assert(query_start_index<decltype(&fn2)>() == 0);
    } // namespace test

    template <typename T>
    SystemInfo compute_system_info(T sys_pointer) {
        // From this index onwards, all args are queries
        using fn_info = impl::FunctionInfo<T>;
        constexpr auto start_index = query_start_index<T>();

        std::unordered_map<TypeHash, ComponentAccess> accesses{};

        using seq = std::make_index_sequence<fn_info::arg_count>();
        constexpr auto index_sequence = seq{};

        auto access_compute_lambda = [&]<size_t index>() {
            if (index < start_index) {
                return;
            }

            using q = fn_info::template ArgType<index>;
            // A std tuple of types the query consumes
            using comp = q::components;
            constexpr size_t comp_count = q::queried_types;
        };



    }

    template <typename... Args>
    ScheduleBatch compute_system_batch(Args... funcs) {
        constexpr static auto func_count = sizeof...(funcs);
        std::vector<ScheduleBatch> schedule_batches{};
        schedule_batches.resize(func_count);
    }
} // namespace orb