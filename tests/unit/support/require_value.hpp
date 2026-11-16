#pragma once

/// @file
/// @brief Test-only helper that unwraps a std::optional or fails the current test (plan 0001
///        §7.1, D44).
///
/// Catch2's REQUIRE is opaque to clang-tidy's bugprone-unchecked-optional-access dataflow, so a
/// `REQUIRE(x.has_value()); use(*x);` pair is reported. requireValue checks and unwraps in one
/// visible step: the failure path calls a [[noreturn]] function, which the analysis understands.

#include <optional>
#include <utility>

namespace philios::test {

/// @brief Throws std::logic_error("expected a value"); Catch2 reports it as a test failure.
/// @throws std::logic_error always.
[[noreturn]] void failMissingValue();

/// @brief Returns the contained value of @p value, or fails the current test if it is empty.
/// @throws std::logic_error if @p value is empty.
template <typename T>
[[nodiscard]] T requireValue(std::optional<T> value) {
    if (!value.has_value()) {
        failMissingValue();
    }
    return std::move(*value);
}

} // namespace philios::test
