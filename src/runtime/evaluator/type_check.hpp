#pragma once

/// @file
/// @brief The single comparison of a runtime value with a declared type, and the script-level
///        type names (plan 0001 §3.6; plan 0002 §3.4, D9, D12).

#include "frontend/ast/ast.hpp"
#include "runtime/object_model/object.hpp"

#include <string_view>

namespace philios {

/// @brief Subtype acceptance with explicit nullability (plan 0002 §3.4, D9): true if @p declared
///        accepts @p value's runtime class.
///
/// | @p declared.base | Accepted runtime classes when @p declared.nullable is false |
/// |---|---|
/// | `Integer`  | Integer |
/// | `String`   | String |
/// | `Function` | Function |
/// | `Object`   | Integer, String, Function (every non-`null` value) |
/// | `Null`     | NullObject only |
///
/// When @p declared.nullable is true, NullObject is accepted in addition. No value is converted.
/// This is the only place a value is compared with a declared type; the argument check (at call
/// time, at the argument) and the return check (at the `return`) both use it. Decides by
/// Object::objectType only (no `dynamic_cast` or `typeid`); ObjectType is unchanged.
/// @param value Borrowed for the duration of the call.
/// @param declared Any DeclaredType value; `?null` (`Null` with `nullable` true) is never
///        produced by the parser and accepts only NullObject.
/// @return Whether @p declared accepts @p value.
/// @throws Nothing.
[[nodiscard]] bool matchesDeclaredType(const Object& value, DeclaredType declared) noexcept;

/// @brief The script-level name of @p value's runtime class (plan 0002 §3.4, D12), as reported by
///        `typeof`.
///
/// Integer → `"Integer"`, String → `"String"`, Function → `"Function"`, NullObject → `"null"`.
/// Never returns an internal class name such as `NullObject` or `ImmutableObject`. Decides by
/// Object::objectType only.
/// @param value Borrowed for the duration of the call.
/// @return A view of a string literal (static storage; no ownership).
/// @throws Nothing.
[[nodiscard]] std::string_view scriptTypeName(const Object& value) noexcept;

} // namespace philios
