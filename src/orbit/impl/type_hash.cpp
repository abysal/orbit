#include "type_hash.hpp"

namespace orb::impl {
    TypeHash create_type_hash(const size_t hash) {
        return TypeHash(hash);
    }
}