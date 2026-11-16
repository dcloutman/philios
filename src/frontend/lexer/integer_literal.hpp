#pragma once

/// @file
/// @brief Conversion of decimal integer literal digits (plan 0001 §3.3, T-LEX-8).
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

} // namespace philios
