#pragma once

/// @file
/// @brief The single comparison of a runtime value with a declared type (plan 0001 §3.6, D14).

#include "frontend/ast/ast.hpp"
#include "runtime/object_model/object.hpp"

namespace philios {

/// @brief Exact typing: true only if @p value's runtime class is exactly @p declared
///        (`Integer` <-> Integer, `String` <-> String, `Function` <-> Function, `null` <->
///        NullObject). No implicit conversion.
///
/// Used by both the argument check and the return check; uses Object::objectType only.
/// @param value Borrowed for the duration of the call.
/// @throws Nothing.
[[nodiscard]] bool matchesDeclaredType(const Object& value, TypeName declared) noexcept;

} // namespace philios
