// Unit tests for the top-level handler in src/cli/cli.hpp (plan 0001 §4, D30; T-SEC-8, task 2).
//
// The throwing bodies are defined only here (D44); the shipped binary has no hook a script can
// reach.

#include "cli/cli.hpp"
#include "cli/exit_status.hpp"
#include "common/errors.hpp"
#include "support/stderr_capture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <new>
#include <stdexcept>
#include <string>

namespace philios::test {
namespace {

/// A thrown type outside the std::exception hierarchy.
struct NotAStandardException {};

} // namespace

TEST_CASE("T-SEC-8 runTopLevel reports std::bad_alloc as out of memory") {
    StderrCapture capture;
    const int status = runTopLevel([]() -> int { throw std::bad_alloc(); });
    const std::string text = capture.text();
    CHECK(status == kExitSoftware);
    CHECK(text.find("philios: internal error: out of memory") != std::string::npos);
}

TEST_CASE("T-SEC-8 runTopLevel reports a non-Philios std::exception as unexpected") {
    StderrCapture capture;
    const int status = runTopLevel([]() -> int { throw std::runtime_error("outside"); });
    const std::string text = capture.text();
    CHECK(status == kExitSoftware);
    CHECK(text.find("philios: internal error: unexpected exception") != std::string::npos);
}

TEST_CASE("T-SEC-8 runTopLevel reports a non-std::exception value as unexpected") {
    StderrCapture capture;
    const int status = runTopLevel([]() -> int { throw NotAStandardException{}; });
    const std::string text = capture.text();
    CHECK(status == kExitSoftware);
    CHECK(text.find("philios: internal error: unexpected exception") != std::string::npos);
}

TEST_CASE("runTopLevel maps philios errors to class-specific status codes") {
    SECTION("lexical maps to 65") {
        StderrCapture capture;
        const int status = runTopLevel([]() -> int {
            throw LexicalError(SourceLocation{1, 2}, "bad token");
        });
        CHECK(status == kExitDataError);
        CHECK(capture.text().find("main.phil:1:2: lexical error: bad token") !=
              std::string::npos);
    }

    SECTION("runtime maps to 70") {
        StderrCapture capture;
        const int status = runTopLevel([]() -> int {
            throw RuntimeError(RuntimeErrorKind::NotCallable, SourceLocation{3, 4}, "no call");
        });
        CHECK(status == kExitSoftware);
        CHECK(capture.text().find("main.phil:3:4: runtime error: no call") != std::string::npos);
    }
}

TEST_CASE("runTopLevel maps startup and internal errors") {
    SECTION("startup maps to 66") {
        StderrCapture capture;
        const int status = runTopLevel([]() -> int { throw StartupError("open failed"); });
        CHECK(status == kExitNoInput);
        CHECK(capture.text().find("philios: startup error: main.phil: open failed") !=
              std::string::npos);
    }

    SECTION("internal maps to 70") {
        StderrCapture capture;
        const int status = runTopLevel([]() -> int { throw InternalError("bug"); });
        CHECK(status == kExitSoftware);
        CHECK(capture.text().find("philios: internal error: bug") != std::string::npos);
    }
}

TEST_CASE("runCli returns usage status and text when arguments are present") {
    StderrCapture capture;
    const int status = runCli(2);
    const std::string text = capture.text();

    CHECK(status == kExitUsage);
    CHECK(text.find("philios: usage error: philios takes no arguments") == 0);
    CHECK(text.find("usage: philios") != std::string::npos);
}

TEST_CASE("runTopLevel returns the body's status when nothing is thrown") {
    StderrCapture capture;
    const int status = runTopLevel([]() -> int { return 42; });
    CHECK(status == 42);
    CHECK(capture.text().empty());
}

} // namespace philios::test
