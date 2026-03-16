#pragma once
#include "func_info.hpp"
#include "orbit/event.hpp"
#include "orbit/query.hpp"
#include "orbit/resource.hpp"
#include "orbit/world.hpp"
#include "platform.hpp"
#include "type_hash.hpp"

#include <entt/entity/registry.hpp>
#include <limits>
#include <ranges>
#include <unordered_map>
#include <vector>

namespace orb {
    // TODO: Handle exclusions

    struct ScheduleBatch;
    using schedule_batch_invoke_function =
        void (*)(SystemInvokeContext& context, void* raw_memory, ScheduleBatch& batch);
    using schedule_batch_allocate_views =
        void (*)(SystemInvokeContext&, void* raw_memory);
    using generic_deleter = void (*)(void*);
    using query_deleter = generic_deleter;

    template <typename>
    struct is_world_access : std::false_type {};

    template <typename T>
        requires std::same_as<World&, T>
    struct is_world_access<T> : std::true_type {};

    template <typename T>
        requires std::same_as<const World&, T>
    struct is_world_access<T> : std::true_type {};

    template <typename T>
    constexpr static bool is_world_access_v = is_world_access<T>::value;

    template <typename>
    struct print_type;

    struct ComputeContext {
        EventManager& event_manager;
        Schedule schedule{};
    };

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
        schedule_batch_invoke_function invoke_function{ nullptr };
        schedule_batch_allocate_views view_allocation_function{ nullptr };
        query_deleter query_deleter{ nullptr };
        size_t query_reserve_size{};
    };

    template <typename... Queries>
    struct SystemQueryInfo {
        using qs = std::tuple<Queries...>;
        constexpr static auto queries_count = sizeof...(Queries);

        using entt_views = std::tuple<typename Queries::view_type...>;

        // raw_location must be a pointer to UNINITIALIZED MEMORY of the correct size to
        // store the views.
        ORB_FORCE_INLINE static void perform_view_generation(
            const SystemInvokeContext& context, entt_views* raw_location
        ) {
            auto* views = new (raw_location) entt_views;

            [&]<size_t... Is>(std::index_sequence<Is...>) {
                ((std::get<Is>(*views) = context.world.view<std::tuple_element_t<Is, qs>>(
                      std::tuple_element_t<Is, qs>::exclude_mask::exclusion
                  )),
                 ...);
            }(std::make_index_sequence<sizeof...(Queries)>{});
        }

        static void delete_queries(void* data) {
            static_cast<entt_views*>(data)->~entt_views();
        }
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
        static inline void fn(int, int, Query<int, float>) {
        }

        static inline void fn2(Query<int, float>) {
        }

        static_assert(query_start_index<decltype(&fn)>() == 2);
        static_assert(query_start_index<decltype(&fn2)>() == 0);
    } // namespace test

    template <typename T>
    SystemInfo compute_system_info(ComputeContext& context, T sys_pointer) {
        // From this index onwards, all args are queries
        using fn_info = impl::FunctionInfo<T>;
        constexpr auto q_start_index = query_start_index<T>();

        std::unordered_map<TypeHash, ComponentAccess> accesses{};

        static_assert(fn_info::arg_count > 0, "Cannot have 0 arg systems!");

        using seq = std::make_index_sequence<fn_info::arg_count>;

        auto access_compute_lambda = [&]<size_t index>() {
            if constexpr (index < q_start_index) {
                using a = fn_info::template ArgType<index>;

                if constexpr (is_event_handler_v<a>) {
                    using event_type = a::event_type;
                    context.event_manager.register_event<event_type>(context.schedule);
                }
            } else {
                using q = fn_info::template ArgType<index>;
                // A std tuple of types the query consumes
                using comp = q::components;
                constexpr size_t comp_count = q::queried_types;

                using comp_seq = std::make_integer_sequence<size_t, comp_count>;

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

                iterate(comp_seq{});
            }
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

    template <typename>
    struct system_arg_folder;

    template <typename... Args>
    struct system_arg_folder<std::tuple<Args...>> {
        using query_tuple = filter_queries_t<Args...>;
    };

    template <typename>
    struct system_query_info_generator {};

    template <typename... Qs>
    struct system_query_info_generator<std::tuple<Qs...>> {
        using query_info = SystemQueryInfo<Qs...>;
    };

    template <typename... SystemPointerTypes>
    struct ScheduleBatchInfo {
        // This insanity of an expression basically turns the raw function pointer types
        // void (*)(..., Query<Velocity, Mut<Position>) into a tuple of instantiations
        // for SystemQueryInfo for all systems

        // clang-format off
        using query_info =
            std::tuple<
                typename system_query_info_generator<
                    typename system_arg_folder<
                        typename impl::FunctionInfo<SystemPointerTypes>::arg_types
                    >::query_tuple
                >::query_info...
            >;
        // clang-format on
        using pointer_types = std::tuple<SystemPointerTypes...>;

        constexpr static size_t view_allocation_size = [] constexpr {
            auto l = []<typename... SysInfo>(
                         std::type_identity<std::tuple<SysInfo...>>
                     ) constexpr {
                size_t result{ 0 };

                static_assert(sizeof...(SysInfo) != 0);

                ((result += sizeof(typename SysInfo::entt_views)), ...);
                return result;
            };
            auto size = l(std::type_identity<query_info>{});

            if (size == 0) {
                std::unreachable();
            }

            return size;
        }();

        static void
        perform_view_generation(SystemInvokeContext& context, void* raw_view_memory) {
            auto l = [&]<typename... SysInfo>(
                         std::type_identity<std::tuple<SysInfo...>>
                     ) {
                auto* raw_view_mem_cast = static_cast<uint8_t*>(raw_view_memory);

                // Per system, we have, we invoke its view generator, then advance to the
                // next location for the next system to have fresh memory to work with
                // for its view
                ((SysInfo::perform_view_generation(
                      context, reinterpret_cast<SysInfo::entt_views*>(raw_view_mem_cast)
                  ),
                  raw_view_mem_cast += sizeof(typename SysInfo::entt_views)),
                 ...);
            };

            l(std::type_identity<query_info>{});
        }

        template <typename FnType, typename SystemInfo>
        static void invoke_single_system(
            FnType sys_ptr, SystemInfo::entt_views* raw_memory,
            SystemInvokeContext& context
        ) {
            using fn_info = impl::FunctionInfo<FnType>;
            using fn_args = fn_info::arg_types;
            using qs = typename SystemInfo::qs;
            const auto arg_seq = std::make_index_sequence<fn_info::arg_count>();
            constexpr static auto q_apply_index = query_start_index<FnType>();

            fn_args args = [&] {
                auto l = [&]<size_t I>() {
                    using arg_type = std::tuple_element_t<I, fn_args>;

                    if constexpr (is_world_access_v<arg_type>) {
                        return std::reference_wrapper{ context.world };
                    } else if constexpr (is_query_v<arg_type>) {
                        return arg_type{};
                    } else if constexpr (is_res_v<arg_type>) {
                        return arg_type{};
                    } else if constexpr (is_event_handler_v<arg_type>) {
                        return arg_type{};
                    } else
                        static_assert(
                            std::false_type::value,
                            "Function argument was not one of the allowed types, "
                            "EventReader/Writer, Query, World&, Res/MutRes"
                        );
                };

                auto arg_application = [&]<size_t... Is>(std::index_sequence<Is...>) {
                    return std::make_tuple((l.template operator()<Is>())...);
                };

                return arg_application(arg_seq);
            }();

            if constexpr (q_apply_index != std::numeric_limits<size_t>::max()) {
                qs queries{};

                const auto seq = std::make_index_sequence<std::tuple_size_v<qs>>();

                auto query_invoke = [&]<size_t I>() {
                    using q = std::tuple_element_t<I, qs>;
                    auto& entt_q = std::get<I>(*raw_memory);
                    std::get<I>(queries) = std::move(q{ entt_q });
                };

                auto query_loop = [&]<size_t... Is>(std::index_sequence<Is...>) {
                    (query_invoke.template operator()<Is>(), ...);
                };

                query_loop(seq);

                // moves the queries into the correct slots in the args
                auto query_move = [&]<size_t I>() {
                    if constexpr (I >= q_apply_index) {
                        constexpr static auto query_index = I - q_apply_index;
                        std::get<I>(args) = std::move(std::get<query_index>(queries));
                    }
                };

                auto query_move_loop = [&]<size_t... Is>(std::index_sequence<Is...>) {
                    (query_move.template operator()<Is>(), ...);
                };

                query_move_loop(arg_seq);
            }

            // TODO: Add support for the things required, like resources
            std::apply(sys_ptr, std::move(args));
        }

        static void perform_system_execution(
            SystemInvokeContext& context, void* raw_memory, ScheduleBatch& batch
        ) {
            auto* memory_advanced = static_cast<uint8_t*>(raw_memory);
            auto invoke = [&]<size_t I>() {
                using pointer_type = std::tuple_element_t<I, pointer_types>;
                using sys_info = std::tuple_element_t<I, query_info>;

                const auto& b = batch.batch_systems[I];
                auto p = b.raw_function_pointer;
                auto pointer = std::bit_cast<pointer_type>(p);

                invoke_single_system<pointer_type, sys_info>(
                    pointer,
                    reinterpret_cast<typename sys_info::entt_views*>(memory_advanced),
                    context
                );
                memory_advanced += sizeof(typename sys_info::entt_views);
            };

            auto loop = [&]<size_t... Is>(std::index_sequence<Is...>) {
                ((invoke.template operator()<Is>()), ...);
            };

            loop(std::make_index_sequence<sizeof...(SystemPointerTypes)>{});
        }

        static void perform_view_deletion(void* raw_memory) {
            auto* memory_advanced = static_cast<uint8_t*>(raw_memory);
            auto invoke = [&]<size_t I>() {
                using pointer_type = std::tuple_element_t<I, pointer_types>;
                using sys_info = std::tuple_element_t<I, query_info>;

                sys_info::delete_queries(reinterpret_cast<void*>(memory_advanced));
                memory_advanced += sizeof(typename sys_info::entt_views);
            };

            auto loop = [&]<size_t... Is>(std::index_sequence<Is...>) {
                ((invoke.template operator()<Is>()), ...);
            };

            loop(std::make_index_sequence<sizeof...(SystemPointerTypes)>{});
        }
    };

    template <typename... Args>
    ScheduleBatch compute_schedule_batch(ComputeContext& context, Args... funcs) {
        constexpr static auto func_count = sizeof...(funcs);
        std::vector<SystemInfo> system_batches{};
        system_batches.reserve(func_count);

        (system_batches.emplace_back(
             std::move(compute_system_info<Args>(context, funcs))
         ),
         ...);

        std::unordered_map<TypeHash, bool> schedule_accesses{};

        for (const auto& system : system_batches) {
            for (const auto& access : system.system_accesses) {
                if (schedule_accesses.contains(access.component_hash)
                    && schedule_accesses.at(access.component_hash)) {
                    continue;
                }

                schedule_accesses[access.component_hash] = access.is_mut_access;
            }
        }

        std::vector<ComponentAccess> flattened_accesses{};
        flattened_accesses.reserve(schedule_accesses.size());

        for (const auto& [hash, mut] : schedule_accesses) {
            flattened_accesses.emplace_back(hash, mut);
        }

        ScheduleBatch batch{};
        batch.batch_accesses = std::move(flattened_accesses);
        batch.batch_systems = std::move(system_batches);

        using batch_info = ScheduleBatchInfo<Args...>;

        batch.query_reserve_size = batch_info::view_allocation_size;
        batch.view_allocation_function = &batch_info::perform_view_generation;
        batch.invoke_function = &batch_info::perform_system_execution;
        batch.query_deleter = &batch_info::perform_view_deletion;

        return batch;
    }
} // namespace orb
