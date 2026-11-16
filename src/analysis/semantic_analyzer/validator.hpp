#pragma once

/// @file
/// @brief Whole-file declaration validation (plan 0001 §3.5, D12, D13, D15, D25, D26, D29, D31a).

#include "frontend/ast/ast.hpp"

namespace philios {

/// @brief Checks every §3.5 rule in every function, including functions that are never called.
///
/// Runs after parsing and before anything executes. Identifies nodes with `kind()` and a
/// `static_cast` to the matching `final` class; its `switch` over NodeKind handles every
/// enumerator explicitly and has no casting `default` (§3.9). Recursion is bounded by the parser
/// nesting limit. Runs no Philios code.
///
/// @param program The parsed program; borrowed for the duration of the call. Must contain only
///        parser-produced nodes (never test-only ASTNode subclasses).
/// @throws DeclarationError for the first violation, at the location given by §4 (line 1,
///         column 1 when `main` is absent). InternalError for an unhandled NodeKind or a malformed
///         AST. std::bad_alloc on exhaustion.
void validateProgram(const Program& program);

} // namespace philios
