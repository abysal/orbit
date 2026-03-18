#pragma once
#include "event.hpp"
#include "impl/builder_helpers.hpp"
#include "impl/func_info.hpp"
#include "schedules.hpp"

#include <chrono>
#include <vector>

namespace orb {


    template <auto Fn>
    consteval static SystemID sys_id() {
        return nttp_hash<Fn>();
    }

    namespace impl {
        template<auto... Fns>
        struct Wrapper {

        };
    }

    template<auto... Fns>
    consteval static SystemID folded_id() {
        return sys_id<impl::Wrapper<Fns...>{}>();
    }

    class Builder {
    public:
        template <auto... Fns>
            requires((impl::free_function<decltype(Fns)> && ...))
        Builder& seq_system(const Schedule sched) {
            ComputeContext context{ .event_manager = this->m_events, .schedule = sched };
            auto batch = compute_schedule_batch<Fns...>(context);
            batch.system_id = folded_id<Fns...>();
            this->m_schedule_batches[sched].emplace_back(std::move(batch));
            return *this;
        }

        template<auto Fn>
        Builder& system(const Schedule sched) {
            ComputeContext context{ .event_manager = this->m_events, .schedule = sched };
            auto batch = compute_schedule_batch<Fn>(context);
            batch.system_id = sys_id<Fn>();
            this->m_schedule_batches[sched].emplace_back(std::move(batch));
            return *this;
        }

        Builder& fixed_update_time(std::chrono::milliseconds time);

        void run();

    private:
        // We can't inline this function ever, since it does dynamic stack allocations,
        // which could cause a stack overflow if not freed which only happens on func
        // return
        ORB_NEVER_INLINE void run_setup_systems(SystemInvokeContext& c);

    private:
        SchedulesStorage<std::vector<ScheduleBatch>> m_schedule_batches{};
        std::chrono::milliseconds m_fixed_update_tick_size{ std::chrono::milliseconds{
            50 } };
        EventManager m_events{};
    };

} // namespace orb
