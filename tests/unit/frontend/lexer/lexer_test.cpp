// Unit tests for src/frontend/lexer/lexer.hpp (plan 0001 §3.3, D8, D23, D27, D28, D40; task 5;
// plan 0002 §3.2, D2, D4-D7, D20-D22; task 2).
//
// Per §8, error cases of T-LEX-1 and T-LEX-3..8 are CLI golden tests; this file checks their
// accepted cases on the token stream. T-LEX-2, T-LEX-9, and T-LEX-12 run whole here (T-LEX-9's
// line-break and end-of-file location cases are also CLI tests), plus T-LEX-11 part (a). Plan 0002
// T2-LEX-5 and T2-LEX-6 run here; T2-LEX-1 to T2-LEX-3 and T2-LEX-7 are CLI golden tests.

#include "frontend/lexer/lexer.hpp"
#include "common/errors.hpp"
#include "frontend/lexer/token.hpp"
#include "support/source_pipeline.hpp"

#include "support/require_value.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace philios::test {
namespace {

using Kinds = std::vector<TokenKind>;

[[nodiscard]] Kinds kindsOf(const std::vector<Token>& tokens) {
    Kinds kinds;
    kinds.reserve(tokens.size());
    for (const Token& token : tokens) {
        kinds.push_back(token.kind);
    }
    return kinds;
}

/// The location of the LexicalError raised for @p source, or no value if lexing succeeds.
[[nodiscard]] std::optional<SourceLocation> lexicalErrorAt(std::string_view source) {
    try {
        [[maybe_unused]] const std::vector<Token> tokens = lexSource(source);
    } catch (const LexicalError& error) {
        return error.location();
    }
    return std::nullopt;
}

/// Index of the first token of @p kind, or no value.
[[nodiscard]] std::optional<std::size_t> indexOf(const std::vector<Token>& tokens,
                                                 TokenKind kind) {
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (tokens[index].kind == kind) {
            return index;
        }
    }
    return std::nullopt;
}

/// The line-break styles T-LEX-4 converts an LF-only program to.
enum class LineBreakStyle { CrLf, LoneCr };

/// Replaces every LF in @p text with the line break of @p style.
[[nodiscard]] std::string withLineBreaks(std::string_view text, LineBreakStyle style) {
    const std::string_view lineBreak = style == LineBreakStyle::CrLf ? "\r\n" : "\r";
    std::string result;
    for (const char byte : text) {
        if (byte == '\n') {
            result.append(lineBreak);
        } else {
            result.push_back(byte);
        }
    }
    return result;
}

/// Calls Lexer::tokenize and discards the tokens (for throw checks).
void tokenizeAndDiscard(Lexer& lexer) {
    [[maybe_unused]] const std::vector<Token> tokens = lexer.tokenize();
}

/// Copies @p bytes into heap storage of exactly their size (no terminator, no spare capacity)
/// and passes a view over it to @p check, so ASan reports any over-read (T-LEX-12).
template <typename Check>
void withExactStorage(std::string_view bytes, Check check) {
    const auto storage = std::make_unique<char[]>(bytes.size());
    std::copy(bytes.begin(), bytes.end(), storage.get());
    check(std::string_view(storage.get(), bytes.size()));
}

const Kinds kMinimalProgramKinds = {
    TokenKind::KeywordFunction, TokenKind::Identifier, TokenKind::LeftParen,
    TokenKind::RightParen,      TokenKind::Colon,      TokenKind::Newline,
    TokenKind::Indent,          TokenKind::KeywordPass, TokenKind::Newline,
    TokenKind::Dedent,          TokenKind::EndOfInput,
};

/// Lexes the one-line @p source (which must end in its LF and have static storage, since tokens
/// view it) and returns its tokens without the final NEWLINE and EndOfInput (plan 0002 T2-LEX-5).
[[nodiscard]] std::vector<Token> lineTokens(std::string_view source) {
    std::vector<Token> tokens = lexSource(source);
    REQUIRE(tokens.size() >= 2);
    CHECK(tokens[tokens.size() - 1].kind == TokenKind::EndOfInput);
    CHECK(tokens[tokens.size() - 2].kind == TokenKind::Newline);
    tokens.resize(tokens.size() - 2);
    return tokens;
}

} // namespace

TEST_CASE("Token stream of a minimal program") {
    const std::vector<Token> tokens = lexSource("function main():\n    pass\n");
    CHECK(kindsOf(tokens) == kMinimalProgramKinds);
    REQUIRE(tokens.size() == kMinimalProgramKinds.size());
    CHECK(tokens[0].location == SourceLocation{1, 1});
    CHECK(tokens[1].text == "main");
    CHECK(tokens[1].location == SourceLocation{1, 10});
    CHECK(tokens[2].location == SourceLocation{1, 14});
    CHECK(tokens[4].location == SourceLocation{1, 16});
    CHECK(tokens[7].location == SourceLocation{2, 5});
}

TEST_CASE("T-LEX-1 valid multi-byte UTF-8 in a string and a comment is accepted") {
    const std::string text = "h\xC3\xA9"
                             "llo \xE2\x82\xAC\xF0\x9D\x84\x9E";
    const std::string source =
        "function main():\n    // " + text + "\n    f(\"" + text + "\")\n";
    const std::vector<Token> tokens = lexSource(source);
    const std::size_t literal = requireValue(indexOf(tokens, TokenKind::StringLiteral));
    CHECK(tokens[literal].stringValue == text);
}

TEST_CASE("T-LEX-2 columns count bytes, not characters") {
    // Line 2: `"` at 12, two bytes of U+00E9 at 13-14, `"` at 15, `@` at 17. Plan 0002 T-UPD-1:
    // `@` replaces `-`, which is now a token.
    CHECK(lexicalErrorAt("function main():\n    return \"\xC3\xA9\" @\n") == SourceLocation{2, 17});
}

TEST_CASE("T-LEX-3 accepted characters") {
    SECTION("tab between tokens") {
        CHECK(kindsOf(lexSource("function\tmain():\n    pass\n")) == kMinimalProgramKinds);
    }
    SECTION("tab in a string and in a comment") {
        const std::vector<Token> tokens =
            lexSource("function main():\n    f(\"a\tb\") // c\td\n");
        const std::size_t literal = requireValue(indexOf(tokens, TokenKind::StringLiteral));
        CHECK(tokens[literal].stringValue == "a\tb");
    }
    SECTION("U+202F and U+206A (bidi neighbours) in a string and in a comment") {
        const std::string neighbours = "\xE2\x80\xAF\xE2\x81\xAA";
        const std::string source =
            "function main():\n    f(\"" + neighbours + "\") // " + neighbours + "\n";
        CHECK_FALSE(lexicalErrorAt(source).has_value());
    }
}

TEST_CASE("T-LEX-4 LF, CR LF, lone CR, and mixed line breaks give the same tokens") {
    const std::string_view program = "function main():\n    pass\nfunction f(): null:\n    pass\n";
    const Kinds expected = kindsOf(lexSource(program));
    CHECK(kindsOf(lexSource(withLineBreaks(program, LineBreakStyle::CrLf))) == expected);
    CHECK(kindsOf(lexSource(withLineBreaks(program, LineBreakStyle::LoneCr))) == expected);
    CHECK(kindsOf(lexSource("function main():\r\n    pass\rfunction f(): null:\n    pass\r\n")) ==
          expected);
}

TEST_CASE("T-LEX-4 line-break details") {
    SECTION("LF CR is two line breaks") {
        const std::vector<Token> tokens = lexSource("function main():\n\r    pass\n");
        const std::size_t pass = requireValue(indexOf(tokens, TokenKind::KeywordPass));
        CHECK(tokens[pass].location == SourceLocation{3, 5});
    }
    SECTION("a lone CR ends a // comment") {
        const std::vector<Token> tokens = lexSource("function main(): // c\r    pass\n");
        CHECK(kindsOf(tokens) == kMinimalProgramKinds);
        CHECK(tokens[7].location == SourceLocation{2, 5});
    }
    SECTION("a lone CR inside /* */ advances the line number") {
        const std::vector<Token> tokens = lexSource("function main():\n    /* a\r b */\n    pass\n");
        const std::size_t pass = requireValue(indexOf(tokens, TokenKind::KeywordPass));
        CHECK(tokens[pass].location == SourceLocation{4, 5});
    }
    SECTION("a ; followed by a lone CR continues the statement") {
        const std::vector<Token> tokens = lexSource("function main():\n    f(1,;\r      2)\n");
        const std::size_t comma = requireValue(indexOf(tokens, TokenKind::Comma));
        REQUIRE(comma + 1 < tokens.size());
        CHECK(tokens[comma + 1].kind == TokenKind::IntegerLiteral);
        CHECK(tokens[comma + 1].integerValue == 2);
        CHECK(tokens[comma + 1].location == SourceLocation{3, 7});
    }
}

TEST_CASE("T-LEX-5 accepted comments") {
    SECTION("// after code") {
        CHECK(kindsOf(lexSource("function main(): // note\n    pass\n")) == kMinimalProgramKinds);
    }
    SECTION("/* */ on one line mid-statement") {
        const Kinds kinds = kindsOf(lexSource("function main():\n    f(/* c */ 1)\n"));
        const Kinds expected = {
            TokenKind::KeywordFunction, TokenKind::Identifier,     TokenKind::LeftParen,
            TokenKind::RightParen,      TokenKind::Colon,          TokenKind::Newline,
            TokenKind::Indent,          TokenKind::Identifier,     TokenKind::LeftParen,
            TokenKind::IntegerLiteral,  TokenKind::RightParen,     TokenKind::Newline,
            TokenKind::Dedent,          TokenKind::EndOfInput,
        };
        CHECK(kinds == expected);
    }
    SECTION("// and /* inside a string are string content") {
        const std::vector<Token> tokens = lexSource("function main():\n    f(\"// x /* y\")\n");
        const std::size_t literal = requireValue(indexOf(tokens, TokenKind::StringLiteral));
        CHECK(tokens[literal].stringValue == "// x /* y");
    }
    SECTION("whitespace and // note after the closing */ of a multi-line comment") {
        const std::vector<Token> tokens =
            lexSource("function main():\n    /* a\n    b */   // note\n    pass\n");
        const std::size_t pass = requireValue(indexOf(tokens, TokenKind::KeywordPass));
        CHECK(tokens[pass].location == SourceLocation{4, 5});
    }
}

TEST_CASE("T-LEX-6 accepted continuations produce no NEWLINE, INDENT, or DEDENT at the join") {
    const Kinds expectedCall = {
        TokenKind::Identifier, TokenKind::LeftParen,      TokenKind::IntegerLiteral,
        TokenKind::Comma,      TokenKind::IntegerLiteral, TokenKind::RightParen,
        TokenKind::Newline,
    };
    const auto sources = std::to_array<std::string_view>({
        "function main():\n    f(1,;\n            2)\n",
        "function main():\n    f(1,; // note\n 2)\n",
        "function main():\n    f(1,; /* c */\n 2)\n",
    });
    for (const std::string_view source : sources) {
        const std::vector<Token> tokens = lexSource(source);
        const Kinds all = kindsOf(tokens);
        const std::size_t indent = requireValue(indexOf(tokens, TokenKind::Indent));
        REQUIRE(indent + 1 + expectedCall.size() <= all.size());
        const auto first = all.begin() + static_cast<std::ptrdiff_t>(indent + 1);
        const Kinds call(first, first + static_cast<std::ptrdiff_t>(expectedCall.size()));
        CHECK(call == expectedCall);
    }
}

TEST_CASE("T-LEX-7 a last line without a trailing line break closes its blocks") {
    CHECK(kindsOf(lexSource("function main():\n    pass")) == kMinimalProgramKinds);
}

TEST_CASE("T-LEX-8 accepted integer literals") {
    const std::vector<Token> maximum =
        lexSource("function main():\n    return 9223372036854775807\n");
    const std::size_t maxIndex = requireValue(indexOf(maximum, TokenKind::IntegerLiteral));
    CHECK(maximum[maxIndex].integerValue == std::numeric_limits<std::int64_t>::max());

    const std::vector<Token> zero = lexSource("function main():\n    return 0\n");
    const std::size_t zeroIndex = requireValue(indexOf(zero, TokenKind::IntegerLiteral));
    CHECK(zero[zeroIndex].integerValue == 0);
}

TEST_CASE("T-LEX-9 escapes produce the expected bytes") {
    const std::vector<Token> tokens = lexSource("function main():\n    f(\"\\\\ \\\" \\n \\t\")\n");
    const std::size_t literal = requireValue(indexOf(tokens, TokenKind::StringLiteral));
    CHECK(tokens[literal].stringValue == "\\ \" \n \t");
}

TEST_CASE("T-LEX-9 a backslash before a line break is the raw-line-break error at the break") {
    // Line 2: `"` at 12, `a` at 13, `\` at 14, the line break at 15.
    CHECK(lexicalErrorAt("function main():\n    return \"a\\\n") == SourceLocation{2, 15});
    CHECK(lexicalErrorAt("function main():\n    return \"a\\\r\n") == SourceLocation{2, 15});
    CHECK(lexicalErrorAt("function main():\n    return \"a\\\r") == SourceLocation{2, 15});
}

TEST_CASE("T-LEX-9 a backslash before end of file is the unterminated-string error at the quote") {
    CHECK(lexicalErrorAt("function main():\n    return \"a\\") == SourceLocation{2, 12});
}

TEST_CASE("T-LEX-10 every reserved word lexes as its keyword, never as an identifier") {
    const auto reserved = std::to_array<std::pair<std::string_view, TokenKind>>({
        {"function", TokenKind::KeywordFunction},
        {"return", TokenKind::KeywordReturn},
        {"null", TokenKind::KeywordNull},
        {"pass", TokenKind::KeywordPass},
        {"Integer", TokenKind::TypeInteger},
        {"String", TokenKind::TypeString},
        {"Function", TokenKind::TypeFunction},
        {"Object", TokenKind::TypeObject},
        {"ImmutableObject", TokenKind::TypeImmutableObject},
        {"NullObject", TokenKind::TypeNullObject},
        {"UnsignedInteger", TokenKind::TypeUnsignedInteger},
        {"Float", TokenKind::TypeFloat},
        {"if", TokenKind::KeywordIf},
        {"elseif", TokenKind::KeywordElseif},
        {"else", TokenKind::KeywordElse},
        {"class", TokenKind::KeywordClass},
        {"extends", TokenKind::KeywordExtends},
        {"implements", TokenKind::KeywordImplements},
        {"interface", TokenKind::KeywordInterface},
        {"mixin", TokenKind::KeywordMixin},
        {"with", TokenKind::KeywordWith},
        {"export", TokenKind::KeywordExport},
        {"import", TokenKind::KeywordImport},
    });
    static_assert(reserved.size() == 23);
    for (const auto& [word, kind] : reserved) {
        const std::string source = std::string(word) + "\n";
        const std::vector<Token> tokens = lexSource(source);
        REQUIRE_FALSE(tokens.empty());
        CHECK(tokens[0].kind == kind);
    }
    for (const std::string_view word : {"Integer2", "integer", "_if"}) {
        const std::string source = std::string(word) + "\n";
        const std::vector<Token> tokens = lexSource(source);
        REQUIRE_FALSE(tokens.empty());
        CHECK(tokens[0].kind == TokenKind::Identifier);
        CHECK(tokens[0].text == word);
    }
}

TEST_CASE("T-LEX-11 (a) comments separate tokens and are never indentation") {
    SECTION("return/*c*/42") {
        const std::vector<Token> tokens = lexSource("function h(): Integer:\n    return/*c*/42\n");
        const std::size_t keyword = requireValue(indexOf(tokens, TokenKind::KeywordReturn));
        REQUIRE(keyword + 1 < tokens.size());
        CHECK(tokens[keyword + 1].kind == TokenKind::IntegerLiteral);
        CHECK(tokens[keyword + 1].integerValue == 42);
        CHECK(tokens[keyword + 1].location == SourceLocation{2, 16});
    }
    SECTION("fo/*c*/o is two identifiers") {
        const std::vector<Token> tokens = lexSource("function main():\n    fo/*c*/o()\n");
        const std::size_t indent = requireValue(indexOf(tokens, TokenKind::Indent));
        REQUIRE(indent + 2 < tokens.size());
        CHECK(tokens[indent + 1].kind == TokenKind::Identifier);
        CHECK(tokens[indent + 1].text == "fo");
        CHECK(tokens[indent + 2].kind == TokenKind::Identifier);
        CHECK(tokens[indent + 2].text == "o");
        CHECK(tokens[indent + 2].location == SourceLocation{2, 12});
    }
    SECTION("12/*c*/abc is a literal then an identifier, not a lexical error") {
        const std::vector<Token> tokens = lexSource("function main():\n    return 12/*c*/abc\n");
        const std::size_t literal = requireValue(indexOf(tokens, TokenKind::IntegerLiteral));
        REQUIRE(literal + 1 < tokens.size());
        CHECK(tokens[literal].integerValue == 12);
        CHECK(tokens[literal + 1].kind == TokenKind::Identifier);
        CHECK(tokens[literal + 1].text == "abc");
        CHECK(tokens[literal + 1].location == SourceLocation{2, 19});
    }
    SECTION("a comment before the spaces leaves indentation 0") {
        const std::vector<Token> tokens =
            lexSource("function main():\n    pass\n/*c*/    return 1\n");
        const std::size_t pass = requireValue(indexOf(tokens, TokenKind::KeywordPass));
        REQUIRE(pass + 3 < tokens.size());
        CHECK(tokens[pass + 1].kind == TokenKind::Newline);
        CHECK(tokens[pass + 2].kind == TokenKind::Dedent);
        CHECK(tokens[pass + 3].kind == TokenKind::KeywordReturn);
        CHECK(tokens[pass + 3].location == SourceLocation{3, 10});
    }
    SECTION("a comment after the spaces keeps indentation 4") {
        const Kinds expected = {
            TokenKind::KeywordFunction, TokenKind::Identifier,     TokenKind::LeftParen,
            TokenKind::RightParen,      TokenKind::Colon,          TokenKind::Newline,
            TokenKind::Indent,          TokenKind::KeywordReturn,  TokenKind::IntegerLiteral,
            TokenKind::Newline,         TokenKind::Dedent,         TokenKind::EndOfInput,
        };
        const std::vector<Token> tokens = lexSource("function main():\n    /*c*/return 1\n");
        CHECK(kindsOf(tokens) == expected);
        REQUIRE(tokens.size() == expected.size());
        CHECK(tokens[7].location == SourceLocation{2, 10});
    }
}

TEST_CASE("T-LEX-12 end-of-input lookahead on exact-size storage") {
    SECTION("a lone CR as the last byte is a line break") {
        withExactStorage("function main():\r    pass\r", [](std::string_view source) {
            CHECK(kindsOf(lexSource(source)) == kMinimalProgramKinds);
        });
    }
    SECTION("a final / is an unexpected character") {
        withExactStorage("function main():\n    pass\n/", [](std::string_view source) {
            CHECK(lexicalErrorAt(source) == SourceLocation{3, 1});
        });
    }
    SECTION("an unterminated /* whose last byte is *") {
        withExactStorage("function main():\n    pass\n/* c *", [](std::string_view source) {
            CHECK(lexicalErrorAt(source) == SourceLocation{3, 1});
        });
    }
    SECTION("an unterminated string ending in a backslash") {
        withExactStorage("function main():\n    f(\"a\\", [](std::string_view source) {
            CHECK(lexicalErrorAt(source) == SourceLocation{2, 7});
        });
    }
    SECTION("an unterminated string ending in $") {
        withExactStorage("function main():\n    f(\"a$", [](std::string_view source) {
            CHECK(lexicalErrorAt(source) == SourceLocation{2, 7});
        });
    }
    SECTION("a final ; is a continuation followed by end of file") {
        withExactStorage("function main():\n    pass;", [](std::string_view source) {
            CHECK(lexicalErrorAt(source) == SourceLocation{2, 9});
        });
    }
    SECTION("a final digit") {
        withExactStorage("function main():\n    return 7", [](std::string_view source) {
            const std::vector<Token> tokens = lexSource(source);
            const std::size_t literal = requireValue(indexOf(tokens, TokenKind::IntegerLiteral));
            CHECK(tokens[literal].integerValue == 7);
            CHECK(tokens.back().kind == TokenKind::EndOfInput);
        });
    }
    SECTION("a final UTF-8 lead byte without continuation bytes, in a comment") {
        withExactStorage("function main():\n    pass\n// \xE2", [](std::string_view source) {
            CHECK(lexicalErrorAt(source) == SourceLocation{3, 4});
        });
    }
}

TEST_CASE("T2-LEX-5 negative literals in expression-start context") {
    SECTION("return -1 is return and the literal -1 at the -") {
        const std::vector<Token> tokens = lineTokens("return -1\n");
        CHECK(kindsOf(tokens) == Kinds{TokenKind::KeywordReturn, TokenKind::IntegerLiteral});
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[1].integerValue == -1);
        CHECK(tokens[1].text == "-1");
        CHECK(tokens[1].location == SourceLocation{1, 8});
    }
    SECTION("f(-1, -2) gives two negative literals") {
        const std::vector<Token> tokens = lineTokens("f(-1, -2)\n");
        const Kinds expected = {
            TokenKind::Identifier, TokenKind::LeftParen,      TokenKind::IntegerLiteral,
            TokenKind::Comma,      TokenKind::IntegerLiteral, TokenKind::RightParen,
        };
        CHECK(kindsOf(tokens) == expected);
        REQUIRE(tokens.size() == expected.size());
        CHECK(tokens[2].integerValue == -1);
        CHECK(tokens[2].text == "-1");
        CHECK(tokens[2].location == SourceLocation{1, 3});
        CHECK(tokens[4].integerValue == -2);
        CHECK(tokens[4].text == "-2");
        CHECK(tokens[4].location == SourceLocation{1, 7});
    }
    SECTION("-9223372036854775808 is INT64_MIN") {
        const std::vector<Token> tokens = lineTokens("return -9223372036854775808\n");
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[1].kind == TokenKind::IntegerLiteral);
        CHECK(tokens[1].integerValue == std::numeric_limits<std::int64_t>::min());
        CHECK(tokens[1].text == "-9223372036854775808");
    }
    SECTION("a comment between return and the literal keeps expression-start context") {
        const std::vector<Token> tokens = lineTokens("return /*c*/ -1\n");
        CHECK(kindsOf(tokens) == Kinds{TokenKind::KeywordReturn, TokenKind::IntegerLiteral});
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[1].integerValue == -1);
        CHECK(tokens[1].location == SourceLocation{1, 14});
    }
    SECTION("a ; continuation after return keeps expression-start context") {
        const std::vector<Token> tokens = lexSource("function main():\n    return;\n        -1\n");
        const std::size_t keyword = requireValue(indexOf(tokens, TokenKind::KeywordReturn));
        REQUIRE(keyword + 2 < tokens.size());
        CHECK(tokens[keyword + 1].kind == TokenKind::IntegerLiteral);
        CHECK(tokens[keyword + 1].integerValue == -1);
        CHECK(tokens[keyword + 1].location == SourceLocation{3, 9});
        CHECK(tokens[keyword + 2].kind == TokenKind::Newline);
    }
    SECTION("-1/*c*/abc is the literal -1 then an identifier, not a lexical error") {
        const std::vector<Token> tokens = lineTokens("return -1/*c*/abc\n");
        const Kinds expected = {TokenKind::KeywordReturn, TokenKind::IntegerLiteral,
                                TokenKind::Identifier};
        CHECK(kindsOf(tokens) == expected);
        REQUIRE(tokens.size() == expected.size());
        CHECK(tokens[1].integerValue == -1);
        CHECK(tokens[2].text == "abc");
        CHECK(tokens[2].location == SourceLocation{1, 15});
    }
}

TEST_CASE("T2-LEX-5 every other - is its own Minus token") {
    SECTION("a-1 is an identifier, Minus, and the literal 1") {
        const std::vector<Token> tokens = lineTokens("a-1\n");
        const Kinds expected = {TokenKind::Identifier, TokenKind::Minus,
                                TokenKind::IntegerLiteral};
        CHECK(kindsOf(tokens) == expected);
        REQUIRE(tokens.size() == expected.size());
        CHECK(tokens[1].text == "-");
        CHECK(tokens[1].location == SourceLocation{1, 2});
        CHECK(tokens[2].integerValue == 1);
        CHECK(tokens[2].location == SourceLocation{1, 3});
    }
    SECTION("return --1 is Minus, Minus, 1") {
        const std::vector<Token> tokens = lineTokens("return --1\n");
        const Kinds expected = {TokenKind::KeywordReturn, TokenKind::Minus, TokenKind::Minus,
                                TokenKind::IntegerLiteral};
        CHECK(kindsOf(tokens) == expected);
        REQUIRE(tokens.size() == expected.size());
        CHECK(tokens[1].location == SourceLocation{1, 8});
        CHECK(tokens[2].location == SourceLocation{1, 9});
        CHECK(tokens[3].integerValue == 1);
    }
    SECTION("return - 1 is Minus, 1") {
        const std::vector<Token> tokens = lineTokens("return - 1\n");
        const Kinds expected = {TokenKind::KeywordReturn, TokenKind::Minus,
                                TokenKind::IntegerLiteral};
        CHECK(kindsOf(tokens) == expected);
        REQUIRE(tokens.size() == expected.size());
        CHECK(tokens[1].location == SourceLocation{1, 8});
        CHECK(tokens[2].integerValue == 1);
    }
    SECTION("a - in expression-start context not followed by a digit") {
        CHECK(kindsOf(lineTokens("return -abc\n")) ==
              Kinds{TokenKind::KeywordReturn, TokenKind::Minus, TokenKind::Identifier});
        CHECK(kindsOf(lineTokens("return -/*c*/1\n")) ==
              Kinds{TokenKind::KeywordReturn, TokenKind::Minus, TokenKind::IntegerLiteral});
        CHECK(kindsOf(lineTokens("return -\t1\n")) ==
              Kinds{TokenKind::KeywordReturn, TokenKind::Minus, TokenKind::IntegerLiteral});
        CHECK(kindsOf(lineTokens("return -(\n")) ==
              Kinds{TokenKind::KeywordReturn, TokenKind::Minus, TokenKind::LeftParen});
        CHECK(kindsOf(lineTokens("return -\n")) ==
              Kinds{TokenKind::KeywordReturn, TokenKind::Minus});
    }
    SECTION("a - outside expression-start context") {
        CHECK(kindsOf(lineTokens("pass -1\n")) ==
              Kinds{TokenKind::KeywordPass, TokenKind::Minus, TokenKind::IntegerLiteral});
        CHECK(kindsOf(lineTokens("-1\n")) ==
              Kinds{TokenKind::Minus, TokenKind::IntegerLiteral});
        CHECK(kindsOf(lineTokens("a -1\n")) ==
              Kinds{TokenKind::Identifier, TokenKind::Minus, TokenKind::IntegerLiteral});
        CHECK(kindsOf(lineTokens("null -1\n")) ==
              Kinds{TokenKind::KeywordNull, TokenKind::Minus, TokenKind::IntegerLiteral});
        const Kinds callMinus = {TokenKind::Identifier, TokenKind::LeftParen,
                                 TokenKind::IntegerLiteral, TokenKind::RightParen,
                                 TokenKind::Minus, TokenKind::IntegerLiteral};
        CHECK(kindsOf(lineTokens("f(1)-1\n")) == callMinus);
        // f(1)-0 is a Minus then 0, not the leading-zero lexical error.
        CHECK(kindsOf(lineTokens("f(1)-0\n")) == callMinus);
    }
}

TEST_CASE("T2-LEX-5 ? is a Question token") {
    const std::vector<Token> tokens = lineTokens("?Integer\n");
    CHECK(kindsOf(tokens) == Kinds{TokenKind::Question, TokenKind::TypeInteger});
    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0].text == "?");
    CHECK(tokens[0].location == SourceLocation{1, 1});
    CHECK(tokens[1].location == SourceLocation{1, 2});
}

TEST_CASE("T2-LEX-5 Object and the words reserved by plan 0002 lex as keywords") {
    const auto reserved = std::to_array<std::pair<std::string_view, TokenKind>>({
        {"Object", TokenKind::TypeObject},
        {"not", TokenKind::KeywordNot},
        {"goto", TokenKind::KeywordGoto},
        {"isa", TokenKind::KeywordIsa},
        {"echo", TokenKind::KeywordEcho},
        {"typeof", TokenKind::KeywordTypeof},
    });
    for (const auto& [word, kind] : reserved) {
        const std::string source = std::string(word) + "\n";
        const std::vector<Token> tokens = lexSource(source);
        REQUIRE_FALSE(tokens.empty());
        CHECK(tokens[0].kind == kind);
        CHECK(tokens[0].kind != TokenKind::Identifier);
        CHECK(tokens[0].text == word);
    }
}

TEST_CASE("T2-LEX-6 end-of-input lookahead for - and ? on exact-size storage") {
    SECTION("a final - after return is a Minus token, not a lexical error") {
        withExactStorage("function main():\n    return -", [](std::string_view source) {
            const std::vector<Token> tokens = lexSource(source);
            const std::size_t minus = requireValue(indexOf(tokens, TokenKind::Minus));
            CHECK(tokens[minus].location == SourceLocation{2, 12});
            CHECK(tokens.back().kind == TokenKind::EndOfInput);
        });
    }
    SECTION("final bytes return -9 give the literal -9") {
        withExactStorage("function main():\n    return -9", [](std::string_view source) {
            const std::vector<Token> tokens = lexSource(source);
            const std::size_t literal = requireValue(indexOf(tokens, TokenKind::IntegerLiteral));
            CHECK(tokens[literal].integerValue == -9);
            CHECK(tokens[literal].location == SourceLocation{2, 12});
            CHECK(tokens.back().kind == TokenKind::EndOfInput);
        });
    }
    SECTION("a final ? is a Question token") {
        withExactStorage("function main():\n    return ?", [](std::string_view source) {
            const std::vector<Token> tokens = lexSource(source);
            const std::size_t question = requireValue(indexOf(tokens, TokenKind::Question));
            CHECK(tokens[question].location == SourceLocation{2, 12});
            CHECK(tokens.back().kind == TokenKind::EndOfInput);
        });
    }
    SECTION("final bytes return -0 are the lexical error at the -") {
        withExactStorage("function main():\n    return -0", [](std::string_view source) {
            CHECK(lexicalErrorAt(source) == SourceLocation{2, 12});
        });
    }
}

TEST_CASE("Lexer::tokenize may be called only once") {
    Lexer lexer("function main():\n    pass\n");
    tokenizeAndDiscard(lexer);
    CHECK_THROWS_AS(tokenizeAndDiscard(lexer), InternalError);
}

} // namespace philios::test
