#pragma once

/// @file
/// @brief The lexer: bytes to tokens, including UTF-8 validation, line breaks, comments,
///        `;` continuation, and indentation (plan 0001 §3.3, D8, D23, D27, D28, D40).

#include "common/errors.hpp"
#include "frontend/lexer/token.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace philios {

/// @brief Converts one source buffer into a token stream.
///
/// Ownership: the Lexer borrows the source. The buffer must outlive the Lexer and every token it
/// produces, and must not be mutated or moved while any of them exists (§3.9). The Lexer reads
/// bytes only through the private bounds-checked accessor peek(); no other member indexes the view
/// or dereferences an iterator into it, and nothing relies on a terminator byte (T-LEX-12).
///
/// Not copyable or movable: it is a short-lived local of the load pipeline.
class Lexer final {
public:
    /// @brief Prepares to lex @p source.
    /// @param source The whole file; borrowed (see class documentation).
    /// @throws Nothing.
    explicit Lexer(std::string_view source) noexcept;

    Lexer(const Lexer&) = delete;
    Lexer& operator=(const Lexer&) = delete;
    Lexer(Lexer&&) = delete;
    Lexer& operator=(Lexer&&) = delete;
    ~Lexer() = default;

    /// @brief Lexes the whole source.
    ///
    /// @pre Called at most once per Lexer.
    /// @return The tokens in source order. Blank and comment-only lines produce no tokens. At end
    ///         of input a pending NEWLINE and one DEDENT per open level are emitted, followed by
    ///         exactly one TokenKind::EndOfInput token.
    /// @throws LexicalError at the location given by §4 for the first lexical error (processing
    ///         stops there). InternalError if called a second time. std::bad_alloc on exhaustion.
    ///         On a throw no tokens are returned.
    [[nodiscard]] std::vector<Token> tokenize();

private:
    /// @brief The only byte accessor (§3.3 "Reading the input").
    /// @param offset Distance from the current position.
    /// @return The byte at `position_ + offset` if that index (checked arithmetic) is less than
    ///         the source size; otherwise no value.
    /// @throws Nothing.
    [[nodiscard]] std::optional<unsigned char> peek(std::size_t offset) const noexcept;

    std::string_view source_;
    std::size_t position_{0};
    SourceLocation location_{};
    bool consumed_{false};
};

} // namespace philios
