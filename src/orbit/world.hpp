#pragma once
#include "query.hpp"

#include <entt/entt.hpp>

namespace orb {

    class SystemInvokeContext;

    class World {
    public:
        World() = default;
        World(const World&) = delete;
        World(World&&) = delete;
        World& operator=(const World&) = delete;
        World& operator=(World&&) = delete;

        template<typename... Args>
        auto view(std::type_identity<Query<Args...>>) {
            return this->m_world_registry.view<stored_type_t<Args>...>();
        }
    private:
        entt::registry m_world_registry{};
        friend class Builder;
    };


    class SystemInvokeContext {
    public:
        World& world;
    };
} // namespace orb
