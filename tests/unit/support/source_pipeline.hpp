#pragma once

/// @file
/// @brief Test-only helpers that run the frontend phases on in-memory source (plan 0001 §7.1,
///        D44).

#include "frontend/ast/ast.hpp"
#include "frontend/lexer/token.hpp"

#include <string_view>
#include <vector>

namespace philios::test {

/// @brief Lexes @p source with a fresh Lexer.
/// @param source Must outlive the returned tokens.
/// @throws LexicalError and anything Lexer::tokenize throws.
[[nodiscard]] std::vector<Token> lexSource(std::string_view source);

/// @brief Lexes and parses @p source with `ExecutionContext::kMaxParseNesting`.
/// @param source Must outlive the returned AST (nodes hold views into it).
/// @throws LexicalError, SyntaxError, and anything the phases throw.
[[nodiscard]] Program parseSource(std::string_view source);

} // namespace philios::test
