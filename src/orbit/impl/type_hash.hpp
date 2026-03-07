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
        TypeHash create_type_hash(size_t hash);
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
        TypeHash() = default;
        TypeHash(const TypeHash&) = default;
        TypeHash& operator=(const TypeHash&) = default;
        TypeHash(TypeHash&&) = default;
        TypeHash& operator=(TypeHash&&) = default;
        ~TypeHash() = default;

        friend TypeHash impl::create_type_hash(size_t hash);

        explicit operator size_t() const {
            return this->m_type_hash;
        }

        bool operator==(const TypeHash& rhs) const noexcept = default;
        bool operator!=(const TypeHash& rhs) const noexcept = default;
        auto operator<=>(const TypeHash& rhs) const noexcept = default;

    private:
        explicit TypeHash(const size_t hash) : m_type_hash(hash) {
        }

    private:
        size_t m_type_hash{};
    };

    template <typename>
    consteval static TypeHash type_hash() {
        constexpr auto hash = impl::fnv1a_hash(FUNCTION_NAME);
        constexpr auto out = static_cast<size_t>(hash);
        return impl::create_type_hash(out);
    }
} // namespace orb

template<>
struct std::hash<orb::TypeHash> {
    size_t operator()(const orb::TypeHash& hash) const noexcept {
        return static_cast<size_t>(hash);
    }
};