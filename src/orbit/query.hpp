#pragma once
#include <type_traits>
#include <cstddef>
#include <tuple>

namespace orb {

    template<typename T>
    struct Mut {
        using type = T;
        constexpr static bool mut{true};
    };

    template<typename>
    struct is_mut : std::false_type {};

    template<typename T>
    struct is_mut<Mut<T>> : std::true_type {};

    template<typename T>
    constexpr bool is_mut_v = is_mut<T>::value;

    template<typename T>
    struct stored_type {
        using type = T;
    };

    template<typename T>
    struct stored_type<Mut<T>> {
        using type = T;
    };

    template<typename T>
    using stored_type_t = stored_type<T>::type;


    template<typename... Args>
    class Query {
    public:
        constexpr static size_t queried_types = sizeof...(Args);
        using components = std::tuple<Args...>;
        template<size_t N>
        using component_at = std::tuple_element_t<N, components>;
    };

    template<typename>
    struct is_query : std::false_type {};

    template<typename... T>
    struct is_query<Query<T...>> : std::true_type {};


    template<typename T>
    constexpr static bool is_query_v = is_query<T>::value;

    static_assert(is_query_v<Query<int>>);
    static_assert(is_query_v<Query<int, float>>);
    static_assert(!is_query_v<float>);


    template<typename...>
    struct filter_queries;

    template<>
    struct filter_queries<> {
        using type = std::tuple<>;
    };

    template<typename T, typename... Rest>
    struct filter_queries<T, Rest...> {
    private:
        using tail = filter_queries<Rest...>::type;

    public:
        using type = std::conditional_t<
            is_query_v<T>,
            decltype(std::tuple_cat(std::declval<std::tuple<T>>(), std::declval<tail>())),
            tail
        >;
    };

    template<typename... T>
    using filter_queries_t = filter_queries<T...>::type;
}