#include "builder.hpp"

#include <bits/this_thread_sleep.h>

namespace orb {
    Builder& Builder::fixed_update_time(std::chrono::milliseconds time) {
        this->m_fixed_update_tick_size = time;
        return *this;
    }

    void Builder::run() {
        World w{};

        SystemInvokeContext c{ w };

        this->run_setup_systems(c);

        // TODO: Make this way smarter and handle multiple schedules
        std::vector<void*> allocations{};
        allocations.reserve(this->m_schedule_batches[FixedUpdate].size());

        for (const auto& batch : this->m_schedule_batches[FixedUpdate]) {
            void* raw_view_mem = ORB_STACK_ALLOC(batch.query_reserve_size);
            allocations.emplace_back(raw_view_mem);
        }

        while (true) {
            const auto current_time = std::chrono::steady_clock::now();
            for (const auto& [alloc, batch] :
                 std::views::zip(allocations, this->m_schedule_batches[FixedUpdate])) {
                batch.view_allocation_function(c, alloc);
                batch.invoke_function(c, alloc, batch);
            }
            const auto end_time = std::chrono::steady_clock::now();

            const auto diff = end_time - current_time;
            const auto sleep_time = this->m_fixed_update_tick_size - diff;
            using namespace std::chrono_literals;
            if (sleep_time > 0ms) {
                std::this_thread::sleep_for(sleep_time);
            }
        }
    }

    void Builder::run_setup_systems(SystemInvokeContext& c) {
        std::vector<void*> allocations{};
        allocations.reserve(this->m_schedule_batches[Startup].size());

        for (auto& batch : this->m_schedule_batches[Startup]) {
            void* raw_view_mem = ORB_STACK_ALLOC(batch.query_reserve_size);
            batch.view_allocation_function(c, raw_view_mem);
            allocations.emplace_back(raw_view_mem);
        }

        for (const auto& [alloc, batch] :
             std::views::zip(allocations, this->m_schedule_batches[Startup])) {
            batch.invoke_function(c, alloc, batch);
        }
    }
} // namespace orb