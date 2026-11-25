#pragma once

/// @file
/// @brief Recursive-descent parser for the §3.4 grammar (plan 0001 §3.4, D7, D11, D27).
///
/// Dependency rule (§7.1): includes nothing from `src/runtime/` or `src/analysis/`. The parser runs
/// no Philios code and does not take the execution context; it receives the nesting limit as a
/// value (`ExecutionContext::kMaxParseNesting`, passed by Module::load or the interim pipeline).

#include "frontend/ast/ast.hpp"
#include "frontend/lexer/token.hpp"

#include <cstddef>
#include <span>

namespace philios {

/// @brief Builds a Program from a token stream.
///
/// Ownership: borrows the tokens; they must outlive the Parser. The produced AST copies owned
/// values (string literal bytes and names) and keeps no view into the source buffer that the
/// tokens refer to: only the tokens view it, so that buffer must outlive the tokens but not the
/// AST (plan 0003 §4.14.2, "Name storage").
///
/// Not copyable or movable.
class Parser final {
public:
    /// @param tokens The complete token stream from Lexer::tokenize (ends with EndOfInput).
    /// @param maxNesting The nesting limit (§3.4): one counter for blocks, call argument lists, and
    ///        pseudo-call argument lists (plan 0002 §3.3); a function body is level 1; reaching
    ///        level `maxNesting + 1` is a syntax error at the token that would open it, reported
    ///        before recursing. Plan 0003 §6.1 (DA7): expression operators count on the same
    ///        counter; `!` adds one level for its operand and each `==`/`!=` in a chain adds one
    ///        level; a module-level initializer starts at level 0; level `maxNesting + 1` is a
    ///        syntax error at the `!`, `==`, or `!=` token that would open it.
    /// @throws Nothing.
    Parser(std::span<const Token> tokens, std::size_t maxNesting) noexcept;

    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;
    Parser(Parser&&) = delete;
    Parser& operator=(Parser&&) = delete;
    ~Parser() = default;

    /// @brief Parses the whole program.
    ///
    /// Type annotations follow plan 0002 §3.3: `typeName := baseType | '?' nullableBase`, giving
    /// one of the nine declarable DeclaredType values (never `nullable` with `TypeName::Null`).
    /// Plan 0003 §4.3 adds `Boolean`, `Symbol`, `?Boolean`, and `?Symbol` (thirteen in total).
    /// @pre Called at most once per Parser.
    /// @return The module items (plan 0003 §4.4, §4.14.2: FunctionDecl, ConstDecl, and
    ///         SymbolDecl, each created with `std::make_shared<const …>`) in source order; every
    ///         item is non-null. Ownership passes to the caller.
    /// @throws SyntaxError for the first grammar violation (including an empty block, a
    ///         module-level statement, a nested function, `f()()`, a trailing comma, an unexpected
    ///         indent, a reserved word in an IDENT position, and the nesting limit), at the
    ///         location given by §4. In a type position (plan 0002 §3.3): a `?` not immediately
    ///         followed, on the same line one column later, by the next token is an error at the
    ///         `?` (D25); a token after an adjacent `?` that is not in `nullableBase` (`null`,
    ///         `?`, `)`, a non-declarable word) is an error at that token (D10); and
    ///         `ImmutableObject`, `NullObject`, `UnsignedInteger`, or `Float` is an error at the
    ///         word (D11). Pseudo-calls (plan 0002 §3.3), parsed into EchoCall and TypeofCall as
    ///         expressions and, wrapped in ExprStmt, as statements: `echo` or `typeof` not followed
    ///         by `(` is an error at the word (D8, D20); a pseudo-call with zero or more than one
    ///         argument is an error at the keyword, reported after its argument list is parsed
    ///         (D17); an integer literal (positive or negative) or `null` literal argument of
    ///         `echo` is an error at the argument's first byte (D16). Plan 0003 §5.2 (grammar
    ///         §4.4): `let` at module level is an error at `let`; any other module-level token
    ///         that does not start `function`, `const`, or `symbol` is an error at that token; a
    ///         reserved word where a function, parameter, `let`, `const`, or `symbol` name is
    ///         required is an error at the word; a missing `let`/`const` name, `:`, type, or
    ///         initializer expression after `=`, and a missing operand after `!`, `==`, or `!=`,
    ///         are errors at the token where the part is expected; `symbol` followed by anything
    ///         other than its name and a line end is an error at the unexpected token; an `=`
    ///         inside an expression or a second `=` is an error at that `=`; an identifier at
    ///         statement start followed by a token other than `(` or `=` is an error at that
    ///         token; a statement starting with `!`, `true`, `false`, or a literal is an error at
    ///         that token; `!` directly followed by `!` is an error at the second `!` with the
    ///         message `repeated ! is not allowed` (DA5); the nesting limit for operators is as
    ///         for @p maxNesting; `?Boolean` and `?Symbol` are nullable bases like `?Integer`;
    ///         and `echo(true)` or `echo(false)` is an error at the argument's first byte
    ///         (§4.11.5). InternalError on a malformed token stream or a second call.
    ///         std::bad_alloc on exhaustion. On a throw no AST is returned.
    [[nodiscard]] Program parseProgram();

private:
    std::span<const Token> tokens_;
    std::size_t position_{0};
    std::size_t maxNesting_;
    std::size_t nesting_{0};
    bool consumed_{false};
};

} // namespace philios
