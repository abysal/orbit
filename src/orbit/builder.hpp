#pragma once
#include "impl/builder_helpers.hpp"
#include "impl/func_info.hpp"
#include "schedules.hpp"

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
        Builder& system(Schedule sched, FuncTypes... functions) {



            return *this;
        }

    private:
        SchedulesStorage<std::vector<ScheduleBatch>> m_schedule_batches{};
    };

} // namespace orb
