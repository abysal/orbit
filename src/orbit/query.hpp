#pragma once
#include <cstddef>
#include <entt/entity/registry.hpp>
#include <entt/fwd.hpp>
#include <tuple>
#include <type_traits>
#include <utility>

namespace orb {

    template <typename T>
    struct Mut {
        using type = T;
        constexpr static bool mut{ true };
    };

    template <typename>
    struct is_mut : std::false_type {};

    template <typename T>
    struct is_mut<Mut<T>> : std::true_type {};

    template <typename T>
    constexpr bool is_mut_v = is_mut<T>::value;

    template <typename T>
    struct stored_type {
        using type = const T;
    };

    template <typename T>
    struct stored_type<Mut<T>> {
        using type = T;
    };

    template <typename T>
    using stored_type_t = stored_type<T>::type;

    using Entity = entt::entity;

    template <typename... Args>
    class Query {
    public:
        constexpr static size_t queried_types = sizeof...(Args);
        using components = std::tuple<Args...>;
        template <size_t N>
        using component_at = std::tuple_element_t<N, components>;

        using view_type =
            decltype(std::declval<entt::registry&>().view<stored_type_t<Args>...>());
        template <typename T>
        using ref_type = std::conditional_t<std::is_const_v<T>, const T&, T&>;

        using it_ret = std::tuple<ref_type<stored_type_t<Args>>...>;

        explicit Query(view_type& view) : m_view{ &view } {
        }
        Query() = default;
        Query(const Query&) = delete;
        Query& operator=(const Query&) = delete;
        Query(Query&&) = default;
        Query& operator=(Query&&) = default;
        ~Query() = default;

        class ComponentIterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = it_ret;
            using difference_type = std::ptrdiff_t;
            using pointer = it_ret*;
            using reference = it_ret&;

            explicit ComponentIterator(view_type& view, const bool begin) : m_v(&view) {
                if (begin) {
                    it = view.begin();
                } else {
                    it = view.end();
                }
            }

            it_ret operator*() const {
                auto entity = *it;
                return std::tuple<ref_type<stored_type_t<Args>>...>(
                    m_v->template get<stored_type_t<Args>>(entity)...
                );
            }

            ComponentIterator& operator++() {
                ++it;
                return *this;
            }

            ComponentIterator operator++(int) {
                ComponentIterator tmp = *this;
                ++(*this);
                return tmp;
            }

            bool operator==(const ComponentIterator& other) const {
                return it == other.it;
            }
            bool operator!=(const ComponentIterator& other) const {
                return it != other.it;
            }

        private:
            view_type::iterator it{};
            view_type* m_v{};
        };

        ComponentIterator begin() {
            return ComponentIterator(*this->m_view, true);
        }
        ComponentIterator end() {
            return ComponentIterator(*this->m_view, false);
        }

        [[nodiscard]] bool empty() const {
            return this->m_view == nullptr || this->m_view->size_hint() == 0;
        }

        class EntityIterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = Entity;
            using difference_type = std::ptrdiff_t;
            using pointer = Entity*;
            using reference = Entity&;

            explicit EntityIterator(view_type& view, const bool begin) : m_v(&view) {
                if (begin) {
                    it = view.begin();
                } else {
                    it = view.end();
                }
            }

            Entity operator*() const {
                auto entity = *it;
                return entity;
            }

            EntityIterator& operator++() {
                ++it;
                return *this;
            }

            EntityIterator operator++(int) {
                EntityIterator tmp = *this;
                ++(*this);
                return tmp;
            }

            bool operator==(const EntityIterator& other) const {
                return it == other.it;
            }
            bool operator!=(const EntityIterator& other) const {
                return it != other.it;
            }

        private:
            view_type::iterator it{};
            view_type* m_v{};
        };

        it_ret get_components(Entity entity) const {
            return std::tuple<ref_type<stored_type_t<Args>>...>(
                this->m_view->template get<stored_type_t<Args>>(entity)...
            );
        }

        it_ret operator[](const Entity entity) const {
            return this->get_components(entity);
        }

        struct EntityIteratorWrapper {
            view_type* owner;

            auto begin() const {
                return EntityIterator{*owner, true};
            }

            auto end() const {
                return EntityIterator{*owner, false};
            }
        };

        EntityIteratorWrapper entities() {
            return EntityIteratorWrapper{this->m_view};
        }
    private:
        view_type* m_view{ nullptr };
    };

    template <typename>
    struct is_query : std::false_type {};

    template <typename... T>
    struct is_query<Query<T...>> : std::true_type {};

    template <typename T>
    constexpr static bool is_query_v = is_query<T>::value;

    static_assert(is_query_v<Query<int>>);
    static_assert(is_query_v<Query<int, float>>);
    static_assert(!is_query_v<float>);

    template <typename...>
    struct filter_queries;

    template <>
    struct filter_queries<> {
        using type = std::tuple<>;
    };

    template <typename T, typename... Rest>
    struct filter_queries<T, Rest...> {
    private:
        using tail = filter_queries<Rest...>::type;

    public:
        using type = std::conditional_t<
            is_query_v<T>,
            decltype(std::tuple_cat(
                std::declval<std::tuple<T>>(), std::declval<tail>()
            )),
            tail>;
    };

    template <typename... T>
    using filter_queries_t = filter_queries<T...>::type;
} // namespace orb