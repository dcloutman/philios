#pragma once

/// @file
/// @brief The built-in `Boolean` type (plan 0003 §4.3, D9, D10, D11).

#include "runtime/object_model/object.hpp"

namespace philios {

/// @brief An immutable Philios `Boolean`, whose only values are `true` and `false`. Its storage
///        is private to this class.
///
/// Compares by value (D11; the comparison itself is `valuesEqual` in
/// `src/runtime/evaluator/type_check.hpp`). There are no canonical `true`/`false` instances: like
/// an `Integer` literal, each evaluation that produces a Boolean creates a new object (plan 0003
/// §4.3). No Boolean instance is process-global or `static` (AGENTS.md Core Architectural
/// Constraint 1).
class Boolean final : public ImmutableObject {
public:
    /// @brief Constructs a Boolean holding @p value.
    /// @param value `true` or `false`; copied.
    /// @throws Nothing.
    explicit Boolean(bool value) noexcept;

    /// @return ObjectType::Boolean.
    /// @throws Nothing.
    [[nodiscard]] ObjectType objectType() const noexcept override;

    /// @brief The value this Boolean was constructed with. Never changes.
    /// @throws Nothing.
    [[nodiscard]] bool value() const noexcept;

private:
    bool value_;
};

} // namespace philios
