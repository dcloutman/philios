#pragma once

/// @file
/// @brief The built-in `Symbol` type (plan 0003 §4.3, §4.9, §4.14.2, D9, D11, D14).

#include "runtime/object_model/object.hpp"

namespace philios {

/// @brief A Philios symbol: a unique, immutable value that is pure identity.
///
/// A symbol is equal only to itself (D11, D14): equality compares the addresses of the owning
/// `std::shared_ptr` targets while both operands are held by owning copies (plan 0003 §6.3; the
/// comparison itself is `valuesEqual` in `src/runtime/evaluator/type_check.hpp`). Every
/// constructed Symbol is therefore distinct from every other.
///
/// Holds no state: no name, no AST pointer, and no view into the source buffer (§4.14.2). It is
/// owned by the bindings and values that hold it (§6.3); a per-call symbol returned through a
/// `Symbol` value outlives its call environment through that owning reference. No Symbol
/// instance is process-global or `static` (AGENTS.md Core Architectural Constraint 1).
/// Not copyable or movable (inherited from Object).
class Symbol final : public Object {
public:
    /// @brief Creates a new symbol, distinct from every other Symbol object.
    /// @throws Nothing.
    Symbol() noexcept = default;

    /// @return ObjectType::Symbol.
    /// @throws Nothing.
    [[nodiscard]] ObjectType objectType() const noexcept override;
};

} // namespace philios
