#pragma once

/// @file
/// @brief Whole-file declaration validation (plan 0001 §3.5, D12, D13, D15, D25, D26, D29, D31a;
///        plan 0002 §3.4, §3.5, D23, D29).

#include "frontend/ast/ast.hpp"

namespace philios {

/// @brief Checks every §3.5 rule in every function, including functions that are never called.
///
/// Runs after parsing and before anything executes. Identifies nodes with `kind()` and a
/// `static_cast` to the matching `final` class; its `switch` over NodeKind handles every
/// enumerator explicitly and has no casting `default` (§3.9). Recursion is bounded by the parser
/// nesting limit. Runs no Philios code.
///
/// Plan 0002 §3.4 rules on DeclaredType: `main`'s annotation is omitted or exactly `Integer`
/// (non-nullable); any other annotation, including `?Integer`, `Object`, and `?Object`, is a
/// declaration error at the `main` name (D23). A bare `return` is accepted in `null` functions
/// and in `?T` functions (D29); in `main` and in `Integer`, `String`, `Function`, and `Object`
/// functions it is a declaration error at the `return`. A statement after any `return`, bare or
/// not, is a declaration error at that statement.
///
/// Plan 0002 §3.5, §3.8: an `echo` or `typeof` pseudo-call (EchoCall, TypeofCall), as an
/// expression or wrapped in an ExprStmt, is handled explicitly by validating its single argument
/// like any expression, so `typeof(main)` and `typeof(nope)` are declaration errors at `main` and
/// `nope`.
///
/// @param program The parsed program; borrowed for the duration of the call. Must contain only
///        parser-produced nodes (never test-only ASTNode subclasses).
/// @throws DeclarationError for the first violation, at the location given by §4 (line 1,
///         column 1 when `main` is absent). InternalError for an unhandled NodeKind or a malformed
///         AST. std::bad_alloc on exhaustion.
void validateProgram(const Program& program);

} // namespace philios
