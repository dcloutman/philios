#pragma once

/// @file
/// @brief Conversion of decimal integer literal digits (plan 0001 §3.3, T-LEX-8; negative
///        literals: plan 0002 §3.2, T2-LEX-4).
///
/// Lives in the frontend so the lexer does not depend on the runtime object model (§7.1).

#include <cstdint>
#include <optional>
#include <string_view>

namespace philios {

/// @brief Converts a decimal digit string to a signed 64-bit value with `std::from_chars`.
///
/// @param digits The literal's digits; borrowed for the duration of the call.
/// @pre The lexer has already checked that @p digits is non-empty, contains only `0`-`9`, and
///      obeys the leading-zero rule (D28d).
/// @return The value if it is at most 9,223,372,036,854,775,807 and the whole input is consumed;
///         no value if it is out of range (`std::errc::result_out_of_range`), which is the only
///         failure under the precondition. A literal is never wrapped.
/// @throws Nothing.
[[nodiscard]] std::optional<std::int64_t> parseDecimalInteger(std::string_view digits) noexcept;

/// @brief Converts the magnitude digits of a negative integer literal to its signed 64-bit value
///        (plan 0002 §3.2, D4, D6, D7).
///
/// Parses @p digits as a `std::uint64_t` magnitude with `std::from_chars`, then negates it in
/// unsigned arithmetic (`0 - magnitude`, defined wrapping) and converts the result to
/// `std::int64_t` (a conversion C++20 defines). No signed expression can overflow.
///
/// @param digits The literal's digits, excluding the `-`; borrowed for the duration of the call.
/// @pre The lexer has already checked that @p digits is non-empty, contains only `0`-`9`, and
///      does not start with `0` (D7).
/// @return The negated value if the magnitude is at most 9,223,372,036,854,775,808 (2^63) and the
///         whole input is consumed; `"9223372036854775808"` yields `INT64_MIN`. No value if the
///         magnitude does not fit `std::uint64_t` (`std::errc::result_out_of_range`) or exceeds
///         2^63. A literal is never wrapped; the caller raises the located LexicalError.
/// @throws Nothing.
[[nodiscard]] std::optional<std::int64_t>
parseNegativeDecimalInteger(std::string_view digits) noexcept;

} // namespace philios
