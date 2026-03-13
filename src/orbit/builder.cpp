#include "builder.hpp"

namespace orb {
    void Builder::run() {
        World w{};

        for (int i = 0; i < 100; i++) {
            auto e = w.m_world_registry.create();
            w.m_world_registry.emplace_or_replace<Position>(e, static_cast<float>(i));
            w.m_world_registry.emplace_or_replace<Velocity>(e, static_cast<float>(i) * 0.5f);
            w.m_world_registry.emplace_or_replace<Acceleration>(e, static_cast<float>(i) * 0.5f);
            w.m_world_registry.emplace_or_replace<Health>(e, 30.f);
        }

        SystemInvokeContext c{ w };

        // TODO: Make this way smarter and handle multiple schedules
        std::vector<void*> allocations{};
        allocations.reserve(this->m_schedule_batches[FixedUpdate].size());

        for (auto& batch : this->m_schedule_batches[FixedUpdate]) {
            void* raw_view_mem = ORB_STACK_ALLOC(batch.query_reserve_size);
            batch.view_allocation_function(c, raw_view_mem);
            allocations.emplace_back(raw_view_mem);
        }

        for (const auto& [alloc, batch] :
             std::views::zip(allocations, this->m_schedule_batches[FixedUpdate])) {
            batch.invoke_function(c, alloc, batch);
        }
    }
} // namespace orb