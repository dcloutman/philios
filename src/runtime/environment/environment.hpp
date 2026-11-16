#pragma once

/// @file
/// @brief Lexical environments (plan 0001 §3.6, §3.7, §3.9, D3, D15, D38).
///
/// No global environment or fallback namespace exists (AGENTS.md Core Architectural Constraint 1):
/// a lookup that reaches the end of the `enclosingScope` chain fails.

#include "runtime/object_model/object.hpp"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace philios {

/// @brief A table of name bindings plus an owning link to the enclosing scope.
///
/// In this plan there are two levels: the module environment (no enclosing scope; binds functions)
/// and a fresh call environment per call (binds parameters; enclosing scope is the callee's
/// `closureEnv`). Lookups consult this table first, then each enclosing scope (D38).
///
/// Ownership: owns its bound values and its enclosing scope (strong `std::shared_ptr`). Not
/// copyable or movable.
///
/// Reentrancy: lookup returns an owning copy; no reference, pointer, iterator, or view into the
/// table is exposed.
class Environment final {
public:
    /// @param enclosingScope The enclosing environment, or null for a module environment.
    /// @throws Nothing.
    explicit Environment(std::shared_ptr<Environment> enclosingScope) noexcept;

    /// @brief Releases the `enclosingScope` chain in a loop, never recursing once per link.
    /// @throws Nothing; allocates nothing.
    ~Environment();

    Environment(const Environment&) = delete;
    Environment& operator=(const Environment&) = delete;
    Environment(Environment&&) = delete;
    Environment& operator=(Environment&&) = delete;

    /// @brief Binds @p name to @p value in this environment.
    /// @param name Copied into the table.
    /// @param value Ownership is shared.
    /// @pre @p value is non-null and @p name is not already bound in this environment
    ///      (validation guarantees unique names, D15).
    /// @post lookup(@p name) on this environment returns @p value.
    /// @throws InternalError if a precondition is violated. std::bad_alloc on exhaustion (strong
    ///         guarantee).
    void define(std::string_view name, std::shared_ptr<Object> value);

    /// @brief Resolves @p name in this environment, then in each enclosing scope.
    /// @return An owning copy of the bound value (never a null pointer), or no value if no
    ///         environment in the chain binds @p name. The caller decides how to report absence.
    /// @throws Nothing in practice (no allocation; copying a `std::shared_ptr` does not throw).
    ///         Not declared `noexcept` because the standard map lookup is not specified as
    ///         `noexcept`.
    [[nodiscard]] std::optional<std::shared_ptr<Object>> lookup(std::string_view name) const;

    /// @brief The enclosing scope, or null for a module environment.
    /// @throws Nothing.
    [[nodiscard]] const std::shared_ptr<Environment>& enclosingScope() const noexcept;

    /// @brief Removes every binding by swapping the table with an empty local and destroying the
    ///        local, so no entry is destroyed while the table is being modified.
    ///
    /// Used by Module teardown to break the module env -> Function -> module env cycle (D3).
    /// Safe to call repeatedly.
    /// @post This environment binds nothing.
    /// @throws Nothing; allocates nothing.
    void releaseBindings() noexcept;

private:
    std::shared_ptr<Environment> enclosingScope_;
    std::map<std::string, std::shared_ptr<Object>, std::less<>> bindings_;
};

} // namespace philios
