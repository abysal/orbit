#pragma once
#include "event.hpp"
#include "impl/builder_helpers.hpp"
#include "impl/func_info.hpp"
#include "schedules.hpp"

#include <chrono>
#include <vector>

namespace orb {

    class Builder {
    public:
        // This forces a free function only to allow for some the storage of the function
        // pointer with an uintptr_t

        // Registers N systems to be executed when the Scheduler executes. Functions must
        // be compatible with free function pointers
        template <typename... FuncTypes>
            requires((impl::free_function<FuncTypes> && ...))
        Builder& system(const Schedule sched, FuncTypes... functions) {
            ComputeContext context{ .event_manager = this->m_events, .schedule = sched };
            auto batch = compute_schedule_batch<FuncTypes...>(context, functions...);
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
