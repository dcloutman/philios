#pragma once

/// @file
/// @brief The module: owner of the source buffer, the AST, and the module environment, and the
///        single load pipeline (plan 0001 §3.9, D3).

#include "frontend/ast/ast.hpp"
#include "runtime/environment/environment.hpp"
#include "runtime/evaluator/execution_context.hpp"

#include <memory>
#include <string>

namespace philios {

/// @brief One loaded file (in this plan, exactly one: `main.phil`).
///
/// Owns, in this declaration order, the source buffer (never mutated or moved after loading), the
/// AST, and the module Environment, which binds one Function per declaration. Only tokens, which
/// live only during load(), hold views into the buffer; AST nodes own their names (plan 0003
/// §4.14.2, "Name storage"). A Module never moves: copy and move are deleted, and it is only
/// ever reachable through the `std::unique_ptr` returned by load(). A Module is returned only
/// after lexing, parsing, validation, and function registration all succeed.
class Module final {
    /// @brief Passkey: only Module can construct it, so only load() can create a Module while
    ///        `std::make_unique` still works.
    struct Passkey {
        explicit Passkey() = default;
    };

public:
    /// @brief Runs the whole load pipeline.
    ///
    /// Allocates the Module with `std::make_unique`, moves @p source into its buffer, then lexes,
    /// parses (passing ExecutionContext::kMaxParseNesting), validates, and registers functions,
    /// in that order, so every token and AST view refers to the buffer's final owner.
    /// @param context The execution context; borrowed.
    /// @param source The file's bytes; ownership is taken.
    /// @return The loaded module; ownership passes to the caller.
    /// @throws LexicalError, SyntaxError, DeclarationError, InternalError, std::bad_alloc. On any
    ///         throw the partly built Module is destroyed and nothing is returned (T-OWN-2).
    [[nodiscard]] static std::unique_ptr<Module> load(ExecutionContext& context,
                                                      std::string source);

    /// @brief Constructs an empty module owning @p source. Callable only with a Passkey.
    /// @throws Nothing.
    Module(Passkey passkey, std::string source) noexcept;

    /// @brief Calls release().
    /// @throws Nothing; allocates nothing.
    ~Module();

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;
    Module(Module&&) = delete;
    Module& operator=(Module&&) = delete;

    /// @brief Breaks the module env -> Function -> module env cycle deterministically (D3).
    ///
    /// Releases the module environment's bindings by swapping its table with an empty local, then
    /// releases the AST, and the source buffer last. Safe to call more than once. No AST node
    /// views the buffer (only tokens do, during load()), so a Function still held elsewhere keeps
    /// a self-contained FunctionDecl (plan 0003 §4.14.2).
    /// @post The Module holds no bindings, no AST, and no source; all Functions and the module
    ///       Environment are destroyed unless an external owner still holds them.
    /// @throws Nothing; allocates nothing.
    void release() noexcept;

    /// @brief The module environment (binds every declared function by name).
    /// @return Null after release().
    /// @throws Nothing.
    [[nodiscard]] const std::shared_ptr<Environment>& environment() const noexcept;

private:
    std::string source_;
    Program program_;
    std::shared_ptr<Environment> environment_;
};

} // namespace philios
