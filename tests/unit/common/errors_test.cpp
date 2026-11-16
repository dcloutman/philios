// Unit tests for src/common/errors.hpp (plan 0001 §4; T-SEC-7, task 2).

#include "common/errors.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

namespace philios::test {
namespace {

[[nodiscard]] bool startsWith(std::string_view text, std::string_view prefix) {
    return text.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] bool containsNoControlBytes(std::string_view text) {
    for (const char byte : text) {
        const auto value = static_cast<unsigned char>(byte);
        if (value < 0x20U || value == 0x7FU) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("T-SEC-7 escapeForDiagnostic escapes control characters, CR, and ESC sequences") {
    const std::string raw = std::string("a") + '\x01' + "b\rc" + '\x1b' + "[31md" + '\x7f' + "e" +
                            '\0' + "f\n";
    const std::string escaped = escapeForDiagnostic(raw);
    CHECK(containsNoControlBytes(escaped));
    CHECK(escaped.find('\x1b') == std::string::npos);
    CHECK(escaped.find('\r') == std::string::npos);
    CHECK(escaped.find('a') != std::string::npos);
    CHECK(escaped.find('f') != std::string::npos);
}

TEST_CASE("T-SEC-7 printable ASCII is unchanged by escaping") {
    CHECK(escapeForDiagnostic("plain text 123") == "plain text 123");
}

TEST_CASE("T-SEC-7 located diagnostics use the §4 format and escape the message") {
    const LexicalError error(SourceLocation{3, 7}, std::string("bad ") + '\x1b' + "[2J\rthing");
    const std::string line = formatDiagnostic(error);
    CHECK(startsWith(line, "main.phil:3:7: lexical error: "));
    CHECK(containsNoControlBytes(line));
}

TEST_CASE("§4 class names and located-diagnostic prefixes") {
    CHECK(errorClassName(ErrorClass::Lexical) == "lexical");
    CHECK(errorClassName(ErrorClass::Syntax) == "syntax");
    CHECK(errorClassName(ErrorClass::Declaration) == "declaration");
    CHECK(errorClassName(ErrorClass::Runtime) == "runtime");

    CHECK(startsWith(formatDiagnostic(SyntaxError(SourceLocation{1, 2}, "x")),
                     "main.phil:1:2: syntax error: "));
    CHECK(startsWith(formatDiagnostic(DeclarationError(SourceLocation{4, 18}, "x")),
                     "main.phil:4:18: declaration error: "));
    const RuntimeError runtime(RuntimeErrorKind::MissingReturn, SourceLocation{1, 10}, "x");
    CHECK(runtime.kind() == RuntimeErrorKind::MissingReturn);
    CHECK(startsWith(formatDiagnostic(runtime), "main.phil:1:10: runtime error: "));
}

TEST_CASE("§4 startup and internal diagnostic prefixes escape their text") {
    const std::string startup =
        formatDiagnostic(StartupError(std::string("no such file") + '\x1b' + "[0m"));
    CHECK(startsWith(startup, "philios: startup error: main.phil: "));
    CHECK(containsNoControlBytes(startup));

    const std::string internal = formatDiagnostic(InternalError(std::string("bug\r")));
    CHECK(startsWith(internal, "philios: internal error: "));
    CHECK(containsNoControlBytes(internal));
}

} // namespace philios::test
