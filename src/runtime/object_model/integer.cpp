#include "runtime/object_model/integer.hpp"

#include <cstdint>

namespace philios {

Integer::Integer(std::int64_t value) noexcept : value_(value) {}

ObjectType Integer::objectType() const noexcept {
    return ObjectType::Integer;
}

int Integer::exitStatus() const noexcept {
    std::int64_t remainder = value_ % 256;
    if (remainder < 0) {
        remainder += 256;
    }
    return static_cast<int>(remainder);
}

bool Integer::equals(const Integer& other) const noexcept {
    return value_ == other.value_;
}

} // namespace philios
