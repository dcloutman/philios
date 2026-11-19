#pragma once

/// @file
/// @brief The shipped OutputWriter for standard output and the `SIGPIPE` disposition
///        (plan 0002 §3.6, D13, D18, D26).
///
/// Standard headers only (`<cstdio>`, `<csignal>`, ...); the POSIX confinement of plan 0001
/// (T-BUILD-6) is unchanged. Adds no global state, signal handler, or flag.

#include <string_view>
#include <system_error>

namespace philios {

/// @brief Writes @p bytes to `stdout` exactly and flushes it; meets the OutputWriter contract.
///
/// Calls `std::fwrite` on `stdout` (skipped when @p bytes is empty), then `std::fflush(stdout)`.
/// On a short write or a failed flush, converts `errno` immediately into a
/// `std::generic_category()` code (`std::errc::io_error` if `errno` is 0) and calls
/// `std::clearerr(stdout)`.
///
/// @param bytes The bytes to write, unchanged (no line break, transcoding, or escaping). Borrowed
///              for the duration of the call only.
/// @return An empty code on success; a non-empty `std::generic_category()` code on failure.
/// @post On success, every byte has been written and `stdout` has been flushed.
/// @throws Nothing.
[[nodiscard]] std::error_code writeStandardOutput(std::string_view bytes) noexcept;

/// @brief Sets the process's `SIGPIPE` disposition to `SIG_IGN` (D26).
///
/// Called once by runProject before `main.phil` is loaded, so that writing to a pipe whose reader
/// has exited fails with `EPIPE` and is reported as the output-write runtime error instead of
/// killing the process. Changes a disposition only; installs no handler and no flag.
///
/// @post `SIGPIPE` is ignored by the process.
/// @throws InternalError if `std::signal` returns `SIG_ERR`; std::bad_alloc on exhaustion while
///         building the error.
void ignoreBrokenPipeSignal();

} // namespace philios
