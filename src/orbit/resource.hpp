#pragma once
#include <type_traits>

namespace orb {
    template<typename T>
    struct ResourceStorage {
        static inline T value{};
    };

    template<typename T>
    class Res {
    public:
        using value_type = T;
        Res() : m_resource(&ResourceStorage<T>::value) {}

        const T& operator*() { return *m_resource; }
        const T* operator->() { return m_resource; }

    private:
        const T* m_resource{};
    };

    template<typename T>
    class MutRes {
    public:
        using value_type = T;
        MutRes() : m_resource(&ResourceStorage<T>::value) {}

        T& operator*() { return *m_resource; }
        T* operator->() { return m_resource; }

    private:
        T* m_resource{};
    };

    template<typename>
    struct is_res : std::false_type {};

    template<typename T>
    struct is_res<Res<T>> : std::true_type {};

    template<typename T>
    struct is_res<MutRes<T>> : std::true_type {};

    template<typename T>
    constexpr static bool is_res_v = is_res<T>::value;

    template <typename T>
    struct stored_type<Res<T>> {
        using type = const T;
    };

    template <typename T>
    struct stored_type<MutRes<T>> {
        using type = T;
    };

    template <typename T>
    struct is_mut<MutRes<T>> : std::true_type {};
}