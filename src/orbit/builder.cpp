#include "builder.hpp"

namespace orb {
    void Builder::run() {
        World w{};
        SystemInvokeContext c{w};
        for (auto& batch : this->m_schedule_batches[FixedUpdate]) {
            void* raw_view_mem = ORB_STACK_ALLOC(batch.query_reserve_size);
            batch.view_allocation_function(c, raw_view_mem);
        }
    }
} // namespace orb