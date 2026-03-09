#pragma once
#include "func_info.hpp"
#include "orbit/query.hpp"
#include "type_hash.hpp"
#include "print"

#include <limits>
#include <ranges>
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

        static_assert(fn_info::arg_count > 0, "Cannot have 0 arg systems!");

        using seq = std::make_index_sequence<fn_info::arg_count>;

        auto access_compute_lambda = [&]<size_t index>() {
            if (index < start_index) {
                return;
            }

            using q = fn_info::template ArgType<index>;
            // A std tuple of types the query consumes
            using comp = q::components;
            constexpr size_t comp_count = q::queried_types;

            using copm_seq = std::make_integer_sequence<size_t, comp_count>;

            auto query_hasher = [&]<size_t I>() {
                using t = std::tuple_element_t<I, comp>;
                constexpr static auto hash = type_hash<stored_type_t<t>>();
                if (accesses.contains(hash)) {
                    return;
                }

                constexpr static auto access = compute_access<t>();
                accesses.emplace(hash, access);
            };

            auto iterate = [&]<size_t... Is>(std::index_sequence<Is...>) {
                (query_hasher.template operator()<Is>(), ...);
            };

            iterate(copm_seq{});
        };

        auto iterate = [&]<size_t... Is>(std::index_sequence<Is...>) {
            (access_compute_lambda.template operator()<Is>(), ...);
        };

        iterate(seq());

        SystemInfo info{};
        info.raw_function_pointer = std::bit_cast<uintptr_t>(sys_pointer);

        std::vector<ComponentAccess> flattened_accesses{};
        flattened_accesses.reserve(accesses.size());

        for (auto access : accesses | std::ranges::views::values) {
            flattened_accesses.emplace_back(access);
        }

        info.system_accesses = std::move(flattened_accesses);

        return info;
    }

    template <typename... Args>
    ScheduleBatch compute_schedule_batch(Args... funcs) {
        constexpr static auto func_count = sizeof...(funcs);
        std::vector<SystemInfo> system_batches{};
        system_batches.reserve(func_count);

        (system_batches.emplace_back(std::move(compute_system_info<Args>(funcs))), ...);

        std::unordered_map<TypeHash, bool> schedule_accesses{};

        for (const auto& system : system_batches) {
            for (const auto& access : system.system_accesses) {
                if (schedule_accesses.contains(access.component_hash) && schedule_accesses.at(access.component_hash)) {
                    continue;
                }

                schedule_accesses[access.component_hash] = access.is_mut_access;
            }
        }
        std::vector<ComponentAccess> flattened_accesses{};
        flattened_accesses.reserve(schedule_accesses.size());

        for (const auto& [hash, mut] : schedule_accesses ) {
            flattened_accesses.emplace_back(hash, mut);
        }

        ScheduleBatch batch{};
        batch.batch_accesses = std::move(flattened_accesses);
        batch.batch_systems = std::move(system_batches);
        return batch;
    }
} // namespace orb
