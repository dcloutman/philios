#pragma once

/// @file
/// @brief Token kinds and the token record produced by the lexer (plan 0001 §3.3, D27).

#include "common/errors.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace philios {

/// @brief Every token kind. Each of the 23 reserved words has its own keyword kind (D27); the
///        lexer never emits a reserved word as TokenKind::Identifier.
enum class TokenKind {
    Identifier,     ///< `[A-Za-z_][A-Za-z0-9_]*` that is not a reserved word.
    IntegerLiteral, ///< Decimal literal; value in Token::integerValue.
    StringLiteral,  ///< Double-quoted literal; escape-processed bytes in Token::stringValue.
    LeftParen,      ///< `(`
    RightParen,     ///< `)`
    Colon,          ///< `:`
    Comma,          ///< `,`
    Newline,        ///< End of a logical line.
    Indent,         ///< Indentation increased by one level (4 spaces).
    Dedent,         ///< Indentation decreased by one level.
    EndOfInput,     ///< Always the last token.

    // Reserved words with a meaning in this plan's grammar (§3.4).
    KeywordFunction, ///< `function`
    KeywordReturn,   ///< `return`
    KeywordNull,     ///< `null`
    KeywordPass,     ///< `pass`
    TypeInteger,     ///< `Integer`
    TypeString,      ///< `String`
    TypeFunction,    ///< `Function`

    // Reserved for later features; always a syntax error where the grammar needs IDENT.
    TypeObject,          ///< `Object`
    TypeImmutableObject, ///< `ImmutableObject`
    TypeNullObject,      ///< `NullObject`
    TypeUnsignedInteger, ///< `UnsignedInteger`
    TypeFloat,           ///< `Float`
    KeywordIf,           ///< `if`
    KeywordElseif,       ///< `elseif`
    KeywordElse,         ///< `else`
    KeywordClass,        ///< `class`
    KeywordExtends,      ///< `extends`
    KeywordImplements,   ///< `implements`
    KeywordInterface,    ///< `interface`
    KeywordMixin,        ///< `mixin`
    KeywordWith,         ///< `with`
    KeywordExport,       ///< `export`
    KeywordImport,       ///< `import`
};

/// @brief One token.
///
/// Plain aggregate. Ownership: `text` is a non-owning view into the source buffer passed to the
/// Lexer; the buffer must outlive the token and must not be mutated or moved while the token
/// exists (§3.9). `stringValue` is owned.
struct Token {
    TokenKind kind{TokenKind::EndOfInput}; ///< The token kind.
    SourceLocation location{};             ///< Location of the token's first byte.
    std::string_view text{};               ///< The token's source bytes (empty for synthetic
                                           ///< NEWLINE/INDENT/DEDENT/EndOfInput tokens).
    std::int64_t integerValue{0};          ///< Value of an IntegerLiteral; 0 otherwise.
    std::string stringValue{};             ///< Escape-processed bytes of a StringLiteral.
};

} // namespace philios
