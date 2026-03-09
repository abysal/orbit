#pragma once
#include <compare> // This stays for the spaceship operator overload
#include <cstdint>
#include <string_view>
#if defined(_MSC_VER)
#define FUNCTION_NAME __FUNCDNAME__
#elif defined(__clang__) || defined(__GNUC__)
#define FUNCTION_NAME __PRETTY_FUNCTION__
#else
#define FUNCTION_NAME "unknown compiler"
#endif

#include <utility>
namespace orb {
    class TypeHash;
    namespace impl {
        constexpr TypeHash create_type_hash(size_t hash);

        constexpr uint64_t fnv1a_hash(const std::string_view str) {
            uint64_t hash = 14695981039346656037ull;
            for (const char c : str) {
                hash ^= static_cast<uint64_t>(c);
                hash *= 1099511628211ull;
            }
            return hash;
        }
    } // namespace impl

    class TypeHash {
    public:
        constexpr TypeHash() = default;
        constexpr TypeHash(const TypeHash&) = default;
        constexpr TypeHash& operator=(const TypeHash&) = default;
        constexpr TypeHash(TypeHash&&) = default;
        constexpr TypeHash& operator=(TypeHash&&) = default;
        constexpr ~TypeHash() = default;

        friend constexpr TypeHash impl::create_type_hash(size_t hash);

        constexpr explicit operator size_t() const {
            return this->m_type_hash;
        }

        constexpr bool operator==(const TypeHash& rhs) const noexcept = default;
        constexpr bool operator!=(const TypeHash& rhs) const noexcept = default;
        constexpr auto operator<=>(const TypeHash& rhs) const noexcept = default;

    private:
        constexpr explicit TypeHash(const size_t hash) : m_type_hash(hash) {
        }

    private:
        size_t m_type_hash{};
    };

    namespace impl {
        constexpr TypeHash create_type_hash(size_t hash) {
            return TypeHash(hash);
        }
    }

    template <typename T_T>
    consteval static TypeHash type_hash() {
        constexpr auto hash = impl::fnv1a_hash(FUNCTION_NAME);
        constexpr auto out = static_cast<size_t>(hash);
        return impl::create_type_hash(out);
    }

    template <typename T_T>
    constexpr static std::string_view type_of() {
        constexpr static auto name = FUNCTION_NAME;
        return std::string_view{name};
    }
} // namespace orb

template<>
struct std::hash<orb::TypeHash> {
    size_t operator()(const orb::TypeHash& hash) const noexcept {
        return static_cast<size_t>(hash);
    }
};