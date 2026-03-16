#pragma once
#include <cstddef>
#include <entt/entity/registry.hpp>
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
    struct Exclude {
        using type = T;
        constexpr static bool exclude{ true };
    };

    template <typename>
    struct is_exclude : std::false_type {};

    template <typename T>
    struct is_exclude<Exclude<T>> : std::true_type {};

    template <typename T>
    constexpr static bool is_exclude_v = is_exclude<T>::value;

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

    template <typename T>
    using no_const_stored_type_t = std::remove_const_t<stored_type_t<T>>;

    using Entity = entt::entity;

    template <bool exclude_only, typename...>
    struct exclude_processor;

    template <bool exclude_only>
    struct exclude_processor<exclude_only> {
        using type = std::tuple<>;
    };

    template <typename T, typename... Rest>
    struct exclude_processor<false, T, Rest...> {
    private:
        using tail = exclude_processor<false, Rest...>::type;

        constexpr static bool match = is_exclude_v<T> == false;

    public:
        using type = std::conditional_t<
            match,
            decltype(std::tuple_cat(
                std::declval<std::tuple<no_const_stored_type_t<T>>>(),
                std::declval<tail>()
            )),
            tail>;
    };

    template <typename T, typename... Rest>
    struct exclude_processor<true, T, Rest...> {
    private:
        using tail = exclude_processor<true, Rest...>::type;

        constexpr static bool match = is_exclude_v<T> == true;

    public:

        template<typename U>
        struct unwrap {
            using type = U;
        };

        template<typename U>
        struct unwrap<Exclude<U>> {
            using type = U;
        };

        using type = std::conditional_t<
            match,
            decltype(std::tuple_cat(
                std::declval<std::tuple<typename unwrap<T>::type>>(),
                std::declval<tail>()
            )),
            tail>;
    };

    template <typename T>
    struct exclusion_maker;

    template <typename... Args>
    struct exclusion_maker<std::tuple<Args...>> {
        static constexpr auto exclusion = entt::exclude<no_const_stored_type_t<Args>...>;
    };

    namespace test {
        using q1 = exclude_processor<false, int, float, Exclude<double>>;
        static_assert(std::same_as<q1::type, std::tuple<int, float>>);
        using q2 = exclude_processor<true, int, float, Exclude<double>>;
        static_assert(std::same_as<q2::type, std::tuple<double>>);
    } // namespace test

    template <typename... Args>
    class Query {
    public:
        using components = exclude_processor<false, Args...>::type;
        using excludes = exclude_processor<true, Args...>::type;
        using exclude_mask = exclusion_maker<excludes>;
        template <size_t N>
        using component_at = std::tuple_element_t<N, components>;
        template <size_t N>
        using exclude_at = std::tuple_element_t<N, excludes>;
        constexpr static size_t queried_types = std::tuple_size_v<components>;

        template <typename T>
        using ref_type = std::conditional_t<std::is_const_v<T>, const T&, T&>;

        template <typename Tuple>
        struct view_builder;

        template <typename... Cs>
        struct view_builder<std::tuple<Cs...>> {
            using type = decltype(std::declval<entt::registry&>().view<Cs...>(
                exclude_mask::exclusion
            ));
        };

        template<typename Tuple>
        struct component_unpack;

        template<typename... Cs>
        struct component_unpack<std::tuple<Cs...>> {
            using refs = std::tuple<ref_type<Cs>...>;
        };
        template<typename Tuple>
        struct component_getter;

        template<typename... Cs>
        struct component_getter<std::tuple<Cs...>> {
            template<typename View>
            static auto get(View* v, Entity e) {
                return std::tuple<ref_type<Cs>...>(v->template get<Cs>(e)...);
            }
        };

        using view_type = view_builder<components>::type;

        using it_ret = component_unpack<components>::refs;

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
                return component_getter<components>::get(m_v, entity);
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
            return component_getter<components>::get(this->m_view, entity);
        }

        it_ret operator[](const Entity entity) const {
            return this->get_components(entity);
        }

        struct EntityIteratorWrapper {
            view_type* owner;

            auto begin() const {
                return EntityIterator{ *owner, true };
            }

            auto end() const {
                return EntityIterator{ *owner, false };
            }
        };

        EntityIteratorWrapper entities() {
            return EntityIteratorWrapper{ this->m_view };
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