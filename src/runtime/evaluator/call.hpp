#pragma once

/// @file
/// @brief The call sequence (plan 0001 §3.7, D6, D7, D13, D14, D16, D24).

#include "frontend/ast/ast.hpp"
#include "runtime/evaluator/execution_context.hpp"
#include "runtime/object_model/object.hpp"

#include <memory>
#include <vector>

namespace philios {

/// @brief Performs one Philios call after the callee has been resolved and every argument has
///        been evaluated left to right by the caller (D6).
///
/// Sequence (§3.7): call-depth guard (D7), callee-kind check (must be a Function), argument-count
/// check, argument-type checks left to right (matchesDeclaredType), a fresh Environment holding the
/// parameters whose enclosing scope is the callee's `closureEnv`, the body, and the return-type
/// check. A `null`-returning function that falls off its end returns the context's NullObject;
/// `main` returns Integer 0 in that case; any other function raises the missing-return error.
///
/// Reentrant: runs Philios code.
/// @param context The execution context; borrowed.
/// @param callee The resolved callee value; held (owning) for the entire call.
/// @param arguments The evaluated arguments in source order; held (owning) for the entire call.
/// @param callSite The call expression, used only for error locations (callee identifier and
///        argument nodes); borrowed and must outlive the call.
/// @pre @p callee and every element of @p arguments are non-null.
/// @return The callee's result; never null.
/// @post `context.callDepth()` equals its value on entry, on every exit path.
/// @throws RuntimeError of kind CallDepthExceeded, NotCallable, ArgumentCount, ArgumentType,
///         ReturnType, or MissingReturn at the §4 location, or any error the body raises.
///         InternalError for invariant failures. std::bad_alloc on exhaustion.
[[nodiscard]] std::shared_ptr<Object> callFunction(ExecutionContext& context,
                                                   std::shared_ptr<Object> callee,
                                                   std::vector<std::shared_ptr<Object>> arguments,
                                                   const CallExpr& callSite);

} // namespace philios
