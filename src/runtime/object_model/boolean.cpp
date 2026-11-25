#include "runtime/object_model/boolean.hpp"

#include "runtime/object_model/object.hpp"

namespace philios {

Boolean::Boolean(bool value) noexcept : value_(value) {}

ObjectType Boolean::objectType() const noexcept {
    return ObjectType::Boolean;
}

bool Boolean::value() const noexcept {
    return value_;
}

} // namespace philios
