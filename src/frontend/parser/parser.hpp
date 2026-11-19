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
/// values (string literal bytes) and keeps `std::string_view`s into the source buffer that the
/// tokens refer to, so that buffer must outlive the AST (§3.9).
///
/// Not copyable or movable.
class Parser final {
public:
    /// @param tokens The complete token stream from Lexer::tokenize (ends with EndOfInput).
    /// @param maxNesting The nesting limit (§3.4): one counter for blocks, call argument lists, and
    ///        pseudo-call argument lists (plan 0002 §3.3); a function body is level 1; reaching
    ///        level `maxNesting + 1` is a syntax error at the token that would open it, reported
    ///        before recursing.
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
    /// @pre Called at most once per Parser.
    /// @return The module-level function declarations in source order; ownership passes to the
    ///         caller.
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
    ///         `echo` is an error at the argument's first byte (D16). InternalError on a
    ///         malformed token stream or a second call.
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
