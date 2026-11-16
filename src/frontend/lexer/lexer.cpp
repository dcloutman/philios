#include "frontend/lexer/lexer.hpp"

#include "common/errors.hpp"
#include "frontend/lexer/integer_literal.hpp"
#include "frontend/lexer/token.hpp"
#include "frontend/lexer/utf8.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace philios {
namespace {

[[nodiscard]] bool isAsciiLetter(unsigned char byte) noexcept {
    return (byte >= static_cast<unsigned char>('a') &&
            byte <= static_cast<unsigned char>('z')) ||
           (byte >= static_cast<unsigned char>('A') && byte <= static_cast<unsigned char>('Z'));
}

[[nodiscard]] bool isIdentifierStart(unsigned char byte) noexcept {
    return isAsciiLetter(byte) || byte == static_cast<unsigned char>('_');
}

[[nodiscard]] bool isIdentifierPart(unsigned char byte) noexcept {
    return isIdentifierStart(byte) || (byte >= static_cast<unsigned char>('0') &&
                                       byte <= static_cast<unsigned char>('9'));
}

[[nodiscard]] bool isDigit(unsigned char byte) noexcept {
    return byte >= static_cast<unsigned char>('0') && byte <= static_cast<unsigned char>('9');
}

[[nodiscard]] bool isHorizontalWhitespace(unsigned char byte) noexcept {
    return byte == static_cast<unsigned char>(' ') || byte == static_cast<unsigned char>('\t');
}

[[nodiscard]] bool isLineBreakByte(unsigned char byte) noexcept {
    return byte == static_cast<unsigned char>('\n') || byte == static_cast<unsigned char>('\r');
}

[[nodiscard]] std::optional<TokenKind> keywordKind(std::string_view text) noexcept {
    using Pair = std::pair<std::string_view, TokenKind>;
    static constexpr auto kKeywords = std::array<Pair, 23>{
        Pair{"function", TokenKind::KeywordFunction},
        Pair{"return", TokenKind::KeywordReturn},
        Pair{"null", TokenKind::KeywordNull},
        Pair{"pass", TokenKind::KeywordPass},
        Pair{"Integer", TokenKind::TypeInteger},
        Pair{"String", TokenKind::TypeString},
        Pair{"Function", TokenKind::TypeFunction},
        Pair{"Object", TokenKind::TypeObject},
        Pair{"ImmutableObject", TokenKind::TypeImmutableObject},
        Pair{"NullObject", TokenKind::TypeNullObject},
        Pair{"UnsignedInteger", TokenKind::TypeUnsignedInteger},
        Pair{"Float", TokenKind::TypeFloat},
        Pair{"if", TokenKind::KeywordIf},
        Pair{"elseif", TokenKind::KeywordElseif},
        Pair{"else", TokenKind::KeywordElse},
        Pair{"class", TokenKind::KeywordClass},
        Pair{"extends", TokenKind::KeywordExtends},
        Pair{"implements", TokenKind::KeywordImplements},
        Pair{"interface", TokenKind::KeywordInterface},
        Pair{"mixin", TokenKind::KeywordMixin},
        Pair{"with", TokenKind::KeywordWith},
        Pair{"export", TokenKind::KeywordExport},
        Pair{"import", TokenKind::KeywordImport},
    };

    for (const auto& [word, kind] : kKeywords) {
        if (text == word) {
            return kind;
        }
    }
    return std::nullopt;
}

} // namespace

Lexer::Lexer(std::string_view source) noexcept : source_(source) {}

std::vector<Token> Lexer::tokenize() {
    if (consumed_) {
        throw InternalError("Lexer::tokenize called more than once");
    }
    consumed_ = true;

    std::vector<Token> tokens;
    std::vector<std::size_t> indentStack{0U};

    std::optional<SourceLocation> pendingContinuation;
    bool atLineStart = true;
    bool ignoreIndentation = false;
    bool indentationApplied = false;
    std::size_t indentationSpaces = 0;
    bool lineHasCode = false;
    bool lineContinues = false;
    // Indentation errors are reported at the start of the physical line (§4), not at the first
    // token after the indentation.
    SourceLocation lineStartLocation{};

    const auto currentLocation = [this]() noexcept { return location_; };

    const auto throwLexical = [](SourceLocation location, const std::string& message) -> void {
        throw LexicalError(location, message);
    };

    const auto consumeByte = [this]() {
        const std::optional<unsigned char> byte = peek(0);
        if (!byte) {
            throw InternalError("Lexer: attempted to consume past end of input");
        }
        ++position_;
        ++location_.column;
        return *byte;
    };

    const auto consumeLineBreak = [this, &consumeByte]() {
        const std::optional<unsigned char> byte = peek(0);
        if (!byte || !isLineBreakByte(*byte)) {
            throw InternalError("Lexer: expected line break");
        }
        if (*byte == static_cast<unsigned char>('\r') &&
            peek(1) == static_cast<unsigned char>('\n')) {
            consumeByte();
            consumeByte();
        } else {
            consumeByte();
        }
        ++location_.line;
        location_.column = 1;
    };

    const auto addSynthetic = [&tokens](TokenKind kind, SourceLocation location) {
        tokens.push_back(Token{kind, location, {}, 0, {}});
    };

    const auto addTokenFromSource = [this, &tokens](TokenKind kind, SourceLocation location,
                                                     std::size_t start,
                                                     std::size_t length) {
        tokens.push_back(Token{kind, location, source_.substr(start, length), 0, {}});
    };

    const auto addIntegerToken = [this, &tokens](SourceLocation location, std::size_t start,
                                                  std::size_t length, std::int64_t value) {
        tokens.push_back(
            Token{TokenKind::IntegerLiteral, location, source_.substr(start, length), value, {}});
    };

    const auto addStringToken = [this, &tokens](SourceLocation location, std::size_t start,
                                                 std::size_t length, std::string value) {
        tokens.push_back(Token{TokenKind::StringLiteral, location, source_.substr(start, length),
                               0, std::move(value)});
    };

    const auto applyIndentation = [&](SourceLocation tokenLocation) {
        if (indentationApplied) {
            return;
        }
        indentationApplied = true;
        if (ignoreIndentation) {
            return;
        }
        if ((indentationSpaces % 4U) != 0U) {
            throwLexical(lineStartLocation, "indentation must be a multiple of 4 spaces");
        }
        const std::size_t level = indentationSpaces / 4U;
        const std::size_t currentLevel = indentStack.back();
        if (level == currentLevel) {
            return;
        }
        if (level == currentLevel + 1U) {
            indentStack.push_back(level);
            addSynthetic(TokenKind::Indent, tokenLocation);
            return;
        }
        if (level > currentLevel + 1U) {
            throwLexical(lineStartLocation, "indentation increased by more than one level");
        }
        while (indentStack.back() > level) {
            indentStack.pop_back();
            addSynthetic(TokenKind::Dedent, tokenLocation);
        }
        if (indentStack.back() != level) {
            throwLexical(lineStartLocation, "indentation dedent target is not open");
        }
    };

    const auto validateUtf8Byte = [&](bool allowNonAscii) {
        const SourceLocation start = currentLocation();
        const std::optional<Utf8Sequence> sequence = decodeUtf8(source_, position_);
        if (!sequence) {
            throwLexical(start, "malformed UTF-8 sequence");
        }
        if (isDisallowedControl(sequence->codePoint)) {
            throwLexical(start, "disallowed control character");
        }
        if (isBidiFormatting(sequence->codePoint)) {
            throwLexical(start, "disallowed bidi formatting character");
        }
        if (!allowNonAscii && sequence->codePoint > 0x7F) {
            throwLexical(start, "unexpected character");
        }
        for (std::size_t index = 0; index < sequence->length; ++index) {
            consumeByte();
        }
    };

    const auto consumeSingleLineComment = [&]() {
        consumeByte();
        consumeByte();
        while (const std::optional<unsigned char> byte = peek(0)) {
            if (isLineBreakByte(*byte)) {
                return;
            }
            if (*byte < 0x80U) {
                const SourceLocation location = currentLocation();
                const char32_t codePoint = static_cast<char32_t>(*byte);
                if (isDisallowedControl(codePoint)) {
                    throwLexical(location, "disallowed control character");
                }
                consumeByte();
                continue;
            }
            validateUtf8Byte(true);
        }
    };

    const auto consumeBlockComment = [&](SourceLocation startLocation) {
        consumeByte();
        consumeByte();
        bool hasLineBreak = false;
        while (true) {
            const std::optional<unsigned char> byte = peek(0);
            if (!byte) {
                throwLexical(startLocation, "unterminated block comment");
            }
            if (*byte == static_cast<unsigned char>('*') &&
                peek(1) == static_cast<unsigned char>('/')) {
                consumeByte();
                consumeByte();
                return hasLineBreak;
            }
            if (isLineBreakByte(*byte)) {
                hasLineBreak = true;
                consumeLineBreak();
                continue;
            }
            if (*byte < 0x80U) {
                const SourceLocation location = currentLocation();
                const char32_t codePoint = static_cast<char32_t>(*byte);
                if (isDisallowedControl(codePoint)) {
                    throwLexical(location, "disallowed control character");
                }
                consumeByte();
                continue;
            }
            validateUtf8Byte(true);
        }
    };

    const auto consumeStringLiteral = [&]() {
        const std::size_t start = position_;
        const SourceLocation startLocation = currentLocation();
        consumeByte();

        std::string value;
        while (true) {
            const std::optional<unsigned char> byte = peek(0);
            if (!byte) {
                throwLexical(startLocation, "unterminated string literal");
            }
            if (isLineBreakByte(*byte)) {
                throwLexical(currentLocation(), "raw line break in string literal");
            }
            if (*byte == static_cast<unsigned char>('"')) {
                consumeByte();
                addStringToken(startLocation, start, position_ - start, std::move(value));
                return;
            }
            if (*byte == static_cast<unsigned char>('\\')) {
                const SourceLocation slashLocation = currentLocation();
                consumeByte();
                const std::optional<unsigned char> escaped = peek(0);
                if (!escaped) {
                    throwLexical(startLocation, "unterminated string literal");
                }
                if (isLineBreakByte(*escaped)) {
                    throwLexical(currentLocation(), "raw line break in string literal");
                }
                switch (*escaped) {
                case static_cast<unsigned char>('\\'):
                    value.push_back('\\');
                    consumeByte();
                    break;
                case static_cast<unsigned char>('"'):
                    value.push_back('"');
                    consumeByte();
                    break;
                case static_cast<unsigned char>('n'):
                    value.push_back('\n');
                    consumeByte();
                    break;
                case static_cast<unsigned char>('t'):
                    value.push_back('\t');
                    consumeByte();
                    break;
                default:
                    throwLexical(slashLocation, "unknown string escape");
                }
                continue;
            }
            if (*byte == static_cast<unsigned char>('$') &&
                peek(1) == static_cast<unsigned char>('{')) {
                throwLexical(currentLocation(), "${ is reserved in strings");
            }
            if (*byte < 0x80U) {
                const SourceLocation location = currentLocation();
                const char32_t codePoint = static_cast<char32_t>(*byte);
                if (isDisallowedControl(codePoint)) {
                    throwLexical(location, "disallowed control character");
                }
                value.push_back(static_cast<char>(*byte));
                consumeByte();
                continue;
            }

            const SourceLocation location = currentLocation();
            const std::optional<Utf8Sequence> sequence = decodeUtf8(source_, position_);
            if (!sequence) {
                throwLexical(location, "malformed UTF-8 sequence");
            }
            if (isDisallowedControl(sequence->codePoint)) {
                throwLexical(location, "disallowed control character");
            }
            if (isBidiFormatting(sequence->codePoint)) {
                throwLexical(location, "disallowed bidi formatting character");
            }
            value.append(source_.substr(position_, sequence->length));
            for (std::size_t index = 0; index < sequence->length; ++index) {
                consumeByte();
            }
        }
    };

    const auto consumeSemicolonContinuation = [&](SourceLocation semicolonLocation) {
        while (true) {
            const std::optional<unsigned char> byte = peek(0);
            if (!byte) {
                throwLexical(semicolonLocation, "continuation requires a following line");
            }
            if (isLineBreakByte(*byte)) {
                return;
            }
            if (isHorizontalWhitespace(*byte)) {
                consumeByte();
                continue;
            }
            if (*byte == static_cast<unsigned char>('/') &&
                peek(1) == static_cast<unsigned char>('/')) {
                consumeSingleLineComment();
                continue;
            }
            if (*byte == static_cast<unsigned char>('/') &&
                peek(1) == static_cast<unsigned char>('*')) {
                const SourceLocation commentLocation = currentLocation();
                const bool hasLineBreak = consumeBlockComment(commentLocation);
                if (hasLineBreak) {
                    throwLexical(semicolonLocation,
                                 "continuation cannot be followed by a multi-line comment");
                }
                continue;
            }
            throwLexical(semicolonLocation, "continuation marker ';' must end the physical line");
        }
    };

    const auto consumeWhitespaceAndCommentsAfterMultilineComment = [&]() {
        while (true) {
            const std::optional<unsigned char> byte = peek(0);
            if (!byte || isLineBreakByte(*byte)) {
                return;
            }
            if (isHorizontalWhitespace(*byte)) {
                consumeByte();
                continue;
            }
            if (*byte == static_cast<unsigned char>('/') &&
                peek(1) == static_cast<unsigned char>('/')) {
                consumeSingleLineComment();
                return;
            }
            if (*byte == static_cast<unsigned char>('/') &&
                peek(1) == static_cast<unsigned char>('*')) {
                const SourceLocation commentLocation = currentLocation();
                [[maybe_unused]] const bool ignored = consumeBlockComment(commentLocation);
                continue;
            }
            throwLexical(currentLocation(),
                         "code after closing */ of a multi-line comment is not allowed");
        }
    };

    while (true) {
        if (atLineStart) {
            lineHasCode = false;
            lineContinues = false;
            ignoreIndentation = pendingContinuation.has_value();
            indentationApplied = ignoreIndentation;
            indentationSpaces = 0;
            lineStartLocation = currentLocation();

            if (ignoreIndentation) {
                while (true) {
                    const std::optional<unsigned char> byte = peek(0);
                    if (!byte || !isHorizontalWhitespace(*byte)) {
                        break;
                    }
                    consumeByte();
                }
            } else {
                while (true) {
                    const std::optional<unsigned char> byte = peek(0);
                    if (!byte) {
                        break;
                    }
                    if (*byte == static_cast<unsigned char>(' ')) {
                        ++indentationSpaces;
                        consumeByte();
                        continue;
                    }
                    if (*byte == static_cast<unsigned char>('\t')) {
                        throwLexical(lineStartLocation, "tab in indentation is not allowed");
                    }
                    break;
                }
            }
            atLineStart = false;
        }

        const std::optional<unsigned char> byte = peek(0);
        if (!byte) {
            if (pendingContinuation.has_value()) {
                throwLexical(*pendingContinuation, "continuation requires a following line");
            }
            if (lineHasCode) {
                addSynthetic(TokenKind::Newline, currentLocation());
            }
            while (indentStack.size() > 1U) {
                indentStack.pop_back();
                addSynthetic(TokenKind::Dedent, currentLocation());
            }
            addSynthetic(TokenKind::EndOfInput, currentLocation());
            return tokens;
        }

        if (isLineBreakByte(*byte)) {
            if (lineHasCode && !lineContinues) {
                addSynthetic(TokenKind::Newline, currentLocation());
            } else if (!lineHasCode && pendingContinuation.has_value()) {
                throwLexical(*pendingContinuation,
                             "continuation cannot be followed by a blank or comment-only line");
            }
            consumeLineBreak();
            atLineStart = true;
            continue;
        }

        if (isHorizontalWhitespace(*byte)) {
            consumeByte();
            continue;
        }

        if (*byte == static_cast<unsigned char>('/') && peek(1) == static_cast<unsigned char>('/')) {
            consumeSingleLineComment();
            continue;
        }

        if (*byte == static_cast<unsigned char>('/') && peek(1) == static_cast<unsigned char>('*')) {
            const SourceLocation commentLocation = currentLocation();
            const bool lineHadCodeBeforeComment = lineHasCode;
            const bool hasLineBreak = consumeBlockComment(commentLocation);
            if (hasLineBreak) {
                if (lineHadCodeBeforeComment) {
                    addSynthetic(TokenKind::Newline, commentLocation);
                    lineHasCode = false;
                } else if (pendingContinuation.has_value()) {
                    throwLexical(*pendingContinuation,
                                 "continuation cannot be followed by a blank or comment-only line");
                }
                consumeWhitespaceAndCommentsAfterMultilineComment();
            }
            continue;
        }

        const SourceLocation tokenLocation = currentLocation();

        // A `;` is a continuation marker only after code on the same physical line (§3.3); a line
        // consisting only of `;` (after whitespace and comments) is a lexical error at the `;`.
        if (*byte == static_cast<unsigned char>(';')) {
            if (!lineHasCode) {
                throwLexical(tokenLocation, "';' must follow code on the same line");
            }
            consumeByte();
            consumeSemicolonContinuation(tokenLocation);
            pendingContinuation = tokenLocation;
            lineContinues = true;
            continue;
        }

        applyIndentation(tokenLocation);

        if (*byte == static_cast<unsigned char>('(')) {
            addTokenFromSource(TokenKind::LeftParen, tokenLocation, position_, 1U);
            consumeByte();
            lineHasCode = true;
            pendingContinuation.reset();
            continue;
        }
        if (*byte == static_cast<unsigned char>(')')) {
            addTokenFromSource(TokenKind::RightParen, tokenLocation, position_, 1U);
            consumeByte();
            lineHasCode = true;
            pendingContinuation.reset();
            continue;
        }
        if (*byte == static_cast<unsigned char>(':')) {
            addTokenFromSource(TokenKind::Colon, tokenLocation, position_, 1U);
            consumeByte();
            lineHasCode = true;
            pendingContinuation.reset();
            continue;
        }
        if (*byte == static_cast<unsigned char>(',')) {
            addTokenFromSource(TokenKind::Comma, tokenLocation, position_, 1U);
            consumeByte();
            lineHasCode = true;
            pendingContinuation.reset();
            continue;
        }
        if (*byte == static_cast<unsigned char>('"')) {
            consumeStringLiteral();
            lineHasCode = true;
            pendingContinuation.reset();
            continue;
        }
        if (isIdentifierStart(*byte)) {
            const std::size_t start = position_;
            consumeByte();
            while (true) {
                const std::optional<unsigned char> next = peek(0);
                if (!next || !isIdentifierPart(*next)) {
                    break;
                }
                consumeByte();
            }
            const std::string_view text = source_.substr(start, position_ - start);
            const std::optional<TokenKind> keyword = keywordKind(text);
            addTokenFromSource(keyword.value_or(TokenKind::Identifier), tokenLocation, start,
                               position_ - start);
            lineHasCode = true;
            pendingContinuation.reset();
            continue;
        }
        if (isDigit(*byte)) {
            const std::size_t start = position_;
            consumeByte();
            while (true) {
                const std::optional<unsigned char> next = peek(0);
                if (!next || !isDigit(*next)) {
                    break;
                }
                consumeByte();
            }
            const std::string_view digits = source_.substr(start, position_ - start);
            if (digits.size() > 1U && digits.front() == '0') {
                throwLexical(tokenLocation, "integer literal has a leading zero");
            }
            const std::optional<unsigned char> next = peek(0);
            if (next && isIdentifierPart(*next)) {
                throwLexical(tokenLocation,
                             "digit sequence immediately followed by an identifier");
            }
            const std::optional<std::int64_t> value = parseDecimalInteger(digits);
            if (!value) {
                throwLexical(tokenLocation, "integer literal out of range");
            }
            addIntegerToken(tokenLocation, start, position_ - start, *value);
            lineHasCode = true;
            pendingContinuation.reset();
            continue;
        }

        if (*byte < 0x80U) {
            const char32_t codePoint = static_cast<char32_t>(*byte);
            if (isDisallowedControl(codePoint)) {
                throwLexical(tokenLocation, "disallowed control character");
            }
            throwLexical(tokenLocation, "unexpected character");
        }

        const std::optional<Utf8Sequence> sequence = decodeUtf8(source_, position_);
        if (!sequence) {
            throwLexical(tokenLocation, "malformed UTF-8 sequence");
        }
        if (isDisallowedControl(sequence->codePoint)) {
            throwLexical(tokenLocation, "disallowed control character");
        }
        if (isBidiFormatting(sequence->codePoint)) {
            throwLexical(tokenLocation, "disallowed bidi formatting character");
        }
        throwLexical(tokenLocation, "unexpected character");
    }
}

std::optional<unsigned char> Lexer::peek(std::size_t offset) const noexcept {
    if (offset > (std::numeric_limits<std::size_t>::max)() - position_) {
        return std::nullopt;
    }
    const std::size_t index = position_ + offset;
    if (index >= source_.size()) {
        return std::nullopt;
    }
    return static_cast<unsigned char>(source_[index]);
}

} // namespace philios
