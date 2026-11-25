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
/// | `Boolean`  | Boolean |
/// | `Symbol`   | Symbol |
/// | `Object`   | Integer, String, Function, Boolean, Symbol (every non-`null` value) |
/// | `Null`     | NullObject only |
///
/// When @p declared.nullable is true, NullObject is accepted in addition. No value is converted.
/// This is the only place a value is compared with a declared type; the argument check (at call
/// time, at the argument) and the return check (at the `return`) both use it. Decides by
/// Object::objectType only (no `dynamic_cast` or `typeid`); Boolean and Symbol have their own
/// ObjectType tags (plan 0003 §4.3).
/// @param value Borrowed for the duration of the call.
/// @param declared Any DeclaredType value; `?null` (`Null` with `nullable` true) is never
///        produced by the parser and accepts only NullObject.
/// @return Whether @p declared accepts @p value.
/// @throws Nothing.
[[nodiscard]] bool matchesDeclaredType(const Object& value, DeclaredType declared) noexcept;

/// @brief The script-level name of @p value's runtime class (plan 0002 §3.4, D12), as reported by
///        `typeof`.
///
/// Integer → `"Integer"`, String → `"String"`, Function → `"Function"`, NullObject → `"null"`,
/// Boolean → `"Boolean"`, Symbol → `"Symbol"`.
/// Never returns an internal class name such as `NullObject` or `ImmutableObject`. Decides by
/// Object::objectType only.
/// @param value Borrowed for the duration of the call.
/// @return A view of a string literal (static storage; no ownership).
/// @throws Nothing.
[[nodiscard]] std::string_view scriptTypeName(const Object& value) noexcept;

/// @brief Script-level equality of two runtime values (plan 0003 §4.11.2, D11, D21); the result
///        of `left == right`. `left != right` is its negation (§4.11.3).
///
/// Two values are equal only if they have the same runtime class (Object::objectType) and are
/// equal under D11. Values of different runtime classes are unequal; this is not an error.
///
/// | Runtime class | Equal when |
/// |---|---|
/// | `Integer`    | the two `std::int64_t` values are equal (exact; no conversion, §6.2) |
/// | `String`     | the lengths are equal and then the bytes are equal (linear, §6.3) |
/// | `Boolean`    | both are `true` or both are `false` |
/// | `NullObject` | always (`null == null`) |
/// | `Function`, `Symbol`, and every other reference-like class | both are the same instance |
///
/// Static types do not affect the result (D21): only the two runtime values are compared.
/// Equality never recurses into a value's contents. Identity compares the addresses of
/// @p left and @p right; the caller holds owning copies of both operands for the duration of the
/// call (§6.3), so no released object is dereferenced. Decides by Object::objectType only (no
/// `dynamic_cast` or `typeid`).
/// @param left Borrowed for the duration of the call.
/// @param right Borrowed for the duration of the call.
/// @return Whether @p left and @p right are equal under D11.
/// @throws Nothing.
[[nodiscard]] bool valuesEqual(const Object& left, const Object& right) noexcept;

} // namespace philios
