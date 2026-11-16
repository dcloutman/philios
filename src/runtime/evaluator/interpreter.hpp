#pragma once

/// @file
/// @brief Running `main` and mapping its result to the exit status (plan 0001 §3.5, §3.8, D12,
///        D20, D25).

#include "runtime/evaluator/execution_context.hpp"
#include "runtime/evaluator/module.hpp"

namespace philios {

/// @brief Calls the module's `main()` and converts its result to a process exit status.
///
/// Reentrant: runs Philios code. `main` is call depth 1. A non-`Integer` result is the
/// return-type runtime error at the `return`. The result is converted with Integer::exitStatus
/// before the module is torn down, so no Philios value outlives the Module (§3.9).
/// @param context The execution context; borrowed.
/// @param module A successfully loaded module; borrowed. Validation guarantees exactly one `main`.
/// @return `main`'s result modulo 256, in [0, 255].
/// @throws RuntimeError for any runtime error; InternalError if `main` is not bound to a
///         Function; std::bad_alloc on exhaustion.
[[nodiscard]] int runMain(ExecutionContext& context, const Module& module);

} // namespace philios
