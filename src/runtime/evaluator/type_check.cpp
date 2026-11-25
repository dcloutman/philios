#include "runtime/evaluator/type_check.hpp"

#include "frontend/ast/ast.hpp"
#include "runtime/object_model/boolean.hpp"
#include "runtime/object_model/function.hpp"
#include "runtime/object_model/integer.hpp"
#include "runtime/object_model/object.hpp"
#include "runtime/object_model/string_object.hpp"
#include "runtime/object_model/symbol.hpp"

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
    case TypeName::Boolean:
        accepted = (runtimeType == ObjectType::Boolean);
        break;
    case TypeName::Symbol:
        accepted = (runtimeType == ObjectType::Symbol);
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
    case ObjectType::Boolean:
        return "Boolean";
    case ObjectType::Symbol:
        return "Symbol";
    }
    return {};
}

// Equality is symmetric, so swapping the operands cannot change the result.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool valuesEqual(const Object& left, const Object& right) noexcept {
    const ObjectType leftType = left.objectType();
    if (leftType != right.objectType()) {
        return false;
    }

    switch (leftType) {
    case ObjectType::Integer:
        return static_cast<const Integer&>(left).equals(static_cast<const Integer&>(right));
    case ObjectType::String: {
        const String& leftString = static_cast<const String&>(left);
        const String& rightString = static_cast<const String&>(right);
        if (leftString.bytes().size() != rightString.bytes().size()) {
            return false;
        }
        return leftString.contentEquals(rightString.bytes());
    }
    case ObjectType::Boolean:
        return static_cast<const Boolean&>(left).value() ==
               static_cast<const Boolean&>(right).value();
    case ObjectType::Null:
        return true;
    case ObjectType::Function:
    case ObjectType::Symbol:
        return &left == &right;
    }
    return false;
}

} // namespace philios
