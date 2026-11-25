#include "runtime/object_model/symbol.hpp"

#include "runtime/object_model/object.hpp"

namespace philios {

ObjectType Symbol::objectType() const noexcept {
    return ObjectType::Symbol;
}

} // namespace philios
