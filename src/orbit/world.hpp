#pragma once
#include "impl/platform.hpp"
#include "query.hpp"

#include <entt/entt.hpp>

namespace orb {

    class SystemInvokeContext;
    class World;

    class WorldEntity {
    public:
        explicit WorldEntity(const Entity entity, World* world = nullptr) : m_entity(entity), m_world(world) {}

        template<typename T, typename... Args>
        [[maybe_unused]] WorldEntity& component(Args&&... args);

    private:
        friend class World;
        Entity m_entity{};
        World* m_world{nullptr};
    };


    class World {
    public:
        World();
        World(const World&) = delete;
        World(World&&) = delete;
        World& operator=(const World&) = delete;
        World& operator=(World&&) = delete;

        [[maybe_unused]] WorldEntity spawn();

        template<typename T, typename... Args>
        void add_component(Entity entity, Args&&... args) {
            this->m_world_registry.emplace_or_replace<T>(entity, std::forward<Args>(args)...);
        }

        void remove(Entity entity);
        void remove(const WorldEntity entity) {
            this->remove(entity.m_entity);
        }

        template<typename... Args>
        auto view(std::type_identity<Query<Args...>>) {
            return this->m_world_registry.view<stored_type_t<Args>...>();
        }
    private:
        entt::registry m_world_registry{};
        friend class Builder;
    };

    template <typename T, typename... Args>
    WorldEntity& WorldEntity::component(Args&&... args) {
        ORB_ASSERT(this->m_world);
        this->m_world->add_component<T>(this->m_entity, std::forward<Args>(args)...);
        return *this;
    }

    class SystemInvokeContext {
    public:
        World& world;
    };
} // namespace orb
