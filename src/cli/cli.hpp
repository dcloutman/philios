#pragma once

/// @file
/// @brief Command-line entry: argument check, project run, and the top-level handler
///        (plan 0001 §3.1, §4, D19, D20, D30).

#include <functional>

namespace philios {

/// @brief Runs @p body and converts every exception into a report on stderr and an exit status.
///
/// Catches, in this order and by `const&` where applicable (CERT ERR51, ERR54, ERR61): the
/// Philios error base (`PhiliosError`, located diagnostic, 65 or 70 by class), `StartupError`
/// (66), `InternalError` (70), `std::bad_alloc` (70, fixed text
/// `philios: internal error: out of memory`, written without allocating), any other
/// `std::exception` and finally `catch (...)` (70, fixed text
/// `philios: internal error: unexpected exception`). Every diagnostic uses the §4 formats and
/// escapes echoed text. Diagnostics go to stderr only; standard output carries only the program's
/// own output, written by `echo` through the context's writer (plan 0002 §3.6, D13).
///
/// @param body The run to protect; returns the exit status of a run that does not throw.
///             Borrowed for the duration of the call.
/// @pre @p body is callable (not empty).
/// @return @p body's result if it returns normally; otherwise the status mapped above.
/// @post At most one diagnostic line (two for usage, see runCli) has been written to stderr.
/// @throws Nothing in practice: every exception from @p body is caught. Not declared `noexcept`
///         because writing a report could in principle fail; `main` adds a last-resort
///         `catch (...)` returning 70.
[[nodiscard]] int runTopLevel(const std::function<int()>& body);

/// @brief The whole CLI run, intended as the body passed to runTopLevel by `main`.
///
/// If @p argc > 1, writes `philios: usage error: philios takes no arguments` and `usage: philios`
/// to stderr and returns 64 without touching the file system. Otherwise returns runProject().
/// @param argc The `argc` received by `main`; 0 and 1 both mean "no arguments" (§3.1).
/// @return The process exit status.
/// @throws Anything runProject throws.
[[nodiscard]] int runCli(int argc);

/// @brief Loads `./main.phil`, lexes, parses, and validates the whole file, then runs `main()`.
///
/// Before loading, sets `SIGPIPE` to `SIG_IGN` (ignoreBrokenPipeSignal) and creates the
/// execution context with writeStandardOutput as its output writer (plan 0002 §3.6, D26).
/// Pipeline: loadMainSource (§3.2), then Module::load (§3.9), then the interpreter (task 8).
/// Before task 7 an interim pipeline holds the source in a local `const std::string` that is never
/// moved while any token or AST node exists (§3.9). Never opens `philios.json` (D35).
/// @return `main`'s result modulo 256 (§3.8).
/// @throws StartupError on any loading failure; LexicalError, SyntaxError, DeclarationError, or
///         RuntimeError for language errors; InternalError for invariant failures;
///         std::bad_alloc on exhaustion. All are reported by runTopLevel.
[[nodiscard]] int runProject();

} // namespace philios
