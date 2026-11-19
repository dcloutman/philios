#include "runtime/evaluator/type_check.hpp"

#include "frontend/ast/ast.hpp"
#include "runtime/object_model/object.hpp"

#include <string_view>

namespace philios {

bool matchesDeclaredType(const Object& value, DeclaredType declared) noexcept {
    const ObjectType runtimeType = value.objectType();

    bool accepted = false;
    switch (declared.base) {
    case TypeName::Integer:
        accepted = (runtimeType == ObjectType::Integer);
        break;
    case TypeName::String:
        accepted = (runtimeType == ObjectType::String);
        break;
    case TypeName::Function:
        accepted = (runtimeType == ObjectType::Function);
        break;
    case TypeName::Object:
        accepted = (runtimeType != ObjectType::Null);
        break;
    case TypeName::Null:
        accepted = (runtimeType == ObjectType::Null);
        break;
    }

    if (accepted) {
        return true;
    }
    return declared.nullable && runtimeType == ObjectType::Null;
}

std::string_view scriptTypeName(const Object& value) noexcept {
    switch (value.objectType()) {
    case ObjectType::Integer:
        return "Integer";
    case ObjectType::String:
        return "String";
    case ObjectType::Function:
        return "Function";
    case ObjectType::Null:
        return "null";
    }
    return {};
}

} // namespace philios
