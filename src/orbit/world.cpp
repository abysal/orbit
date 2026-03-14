#include "world.hpp"

namespace orb {

    World::World() {
        static std::atomic_flag initialized = false;
        if (!initialized.test_and_set()) {

        } else {
            throw std::logic_error("More than one world created");
        }
    }
    WorldEntity World::spawn() {
        return WorldEntity{this->m_world_registry.create(), this};
    }

    void World::remove(const Entity entity) {
        this->m_world_registry.destroy(entity);
    }
} // namespace orb