#pragma once

/// @file
/// @brief The program-output seam: the writer type through which `echo` reaches its destination
///        (plan 0002 §3.6, D13, D18, D24).
///
/// Standard headers only. The shipped writer (`writeStandardOutput`, `src/cli/standard_output.hpp`)
/// writes to standard output; unit tests pass a capturing fake writer
/// (`tests/unit/support/capturing_output.hpp`).

#include <functional>
#include <string_view>
#include <system_error>

namespace philios {

/// @brief Writes program output bytes to the program's output destination.
///
/// Contract every writer must meet (plan 0002 §3.6):
/// - writes every byte of `bytes`, unchanged, and flushes before returning;
/// - returns an empty `std::error_code` on success and a non-empty one on any failure
///   (a short write or a failed flush is a failure);
/// - never reports a failure through `errno`;
/// - runs no Philios code;
/// - throws nothing except `std::bad_alloc`.
///
/// `bytes` is a borrowed view that is valid only for the duration of the call; a writer keeps no
/// reference to it after it returns. Owned by value by ExecutionContext, which never holds an
/// empty writer.
using OutputWriter = std::function<std::error_code(std::string_view bytes)>;

} // namespace philios
