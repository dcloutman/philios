#include "runtime/evaluator/type_check.hpp"

#include "frontend/ast/ast.hpp"
#include "runtime/object_model/object.hpp"

namespace philios {

bool matchesDeclaredType(const Object& value, TypeName declared) noexcept {
    const ObjectType runtimeType = value.objectType();
    switch (declared) {
    case TypeName::Integer:
        return runtimeType == ObjectType::Integer;
    case TypeName::String:
        return runtimeType == ObjectType::String;
    case TypeName::Function:
        return runtimeType == ObjectType::Function;
    case TypeName::Null:
        return runtimeType == ObjectType::Null;
    }
    return false;
}

} // namespace philios
