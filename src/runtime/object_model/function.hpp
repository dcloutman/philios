#pragma once

/// @file
/// @brief `Function`, the value of a declared function (plan 0001 §3.6, §3.7, §3.9, D3, D16).
///
/// Holds only its declaration and its closure environment; the call sequence lives in
/// `src/runtime/evaluator/call.hpp` (callFunction), so this directory does not depend on the
/// evaluator (§7.1).

#include "runtime/object_model/object.hpp"

#include <memory>

namespace philios {

class Environment;
class FunctionDecl;

/// @brief A first-class function value. Exactly one exists per declared function, created when
///        the module is loaded; every reference to the name yields that object (D16).
///
/// Ownership (D3): shares ownership of its FunctionDecl with the module's Program, and holds a
/// **strong** reference to its closure environment (the module environment), which in turn owns
/// this Function. The cycle is broken only by Module teardown, never by weakening an edge.
class Function final : public Object {
public:
    /// @param declaration The declaration; shared ownership.
    /// @param closureEnv The defining environment (the module environment); shared ownership.
    /// @pre Both pointers are non-null.
    /// @throws InternalError if a precondition is violated.
    Function(std::shared_ptr<const FunctionDecl> declaration,
             std::shared_ptr<Environment> closureEnv);

    /// @return ObjectType::Function.
    /// @throws Nothing.
    [[nodiscard]] ObjectType objectType() const noexcept override;

    /// @brief The declaration. Callers that make reentrant calls copy the pointer.
    /// @throws Nothing.
    [[nodiscard]] const std::shared_ptr<const FunctionDecl>& declaration() const noexcept;

    /// @brief The closure environment (enclosing scope of each call's fresh environment).
    ///        Callers that make reentrant calls copy the pointer.
    /// @throws Nothing.
    [[nodiscard]] const std::shared_ptr<Environment>& closureEnv() const noexcept;

private:
    std::shared_ptr<const FunctionDecl> declaration_;
    std::shared_ptr<Environment> closureEnv_;
};

} // namespace philios
