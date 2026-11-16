#pragma once

/// @file
/// @brief UTF-8 decoding and character classification for the lexer (plan 0001 §3.3, D8, D23,
///        D28e).
///
/// Every function takes a `std::string_view` and checks every index before reading; none relies
/// on a terminator byte or spare capacity after the end of the view (T-LEX-12).

#include <cstddef>
#include <optional>
#include <string_view>

namespace philios {

/// @brief One decoded Unicode scalar value.
struct Utf8Sequence {
    char32_t codePoint{0}; ///< The decoded scalar value (never a surrogate, at most U+10FFFF).
    std::size_t length{0}; ///< Number of bytes the sequence occupies (1 to 4).
};

/// @brief Decodes the well-formed UTF-8 sequence (RFC 3629) that starts at @p offset.
///
/// @param text The whole input; borrowed for the duration of the call.
/// @param offset Byte offset of the sequence's first byte.
/// @return The decoded sequence, or no value if @p offset is not less than `text.size()` or the
///         bytes at @p offset are not a well-formed sequence: a truncated sequence (including one
///         cut off by the end of @p text), a stray continuation byte, an overlong encoding, a
///         surrogate (U+D800-U+DFFF), or a code point above U+10FFFF. The lexer reports a
///         malformed sequence at @p offset.
/// @post On success, `offset + length <= text.size()` (checked arithmetic).
/// @throws Nothing.
[[nodiscard]] std::optional<Utf8Sequence> decodeUtf8(std::string_view text,
                                                     std::size_t offset) noexcept;

/// @brief Whether @p codePoint is a control character the lexer rejects everywhere.
///
/// Control characters are Unicode general category Cc: U+0000-U+001F, U+007F, U+0080-U+009F.
/// Tab (U+0009), LF (U+000A), and CR (U+000D) are the only allowed ones.
/// @return True for a Cc code point other than tab, LF, and CR.
/// @throws Nothing.
[[nodiscard]] bool isDisallowedControl(char32_t codePoint) noexcept;

/// @brief Whether @p codePoint is a bidirectional formatting character rejected everywhere
///        ("Trojan Source", D28e): U+202A-U+202E or U+2066-U+2069.
/// @throws Nothing.
[[nodiscard]] bool isBidiFormatting(char32_t codePoint) noexcept;

} // namespace philios
