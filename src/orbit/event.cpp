#include "event.hpp"

namespace orb {
    void EventManager::clear_schedule(const Schedule sched) const {
        const auto& funcs = this->m_clear[sched];

        for (const auto fn : funcs) {
            fn();
        }
    }
} // namespace orb