#include "runtime/object_model/null_object.hpp"

namespace philios {

ObjectType NullObject::objectType() const noexcept {
    return ObjectType::Null;
}

} // namespace philios
