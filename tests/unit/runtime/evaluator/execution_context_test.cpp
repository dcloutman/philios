// Unit tests for src/runtime/evaluator/execution_context.hpp (plan 0001 §3.7, §3.9, D7, D18,
// D24; T-NULL-4 part (a), T-SEC-3, T-SEC-5, task 3; plan 0002 §3.6, T2-ECHO-9, task 4). No AST
// nodes are needed.

#include "runtime/evaluator/execution_context.hpp"
#include "common/errors.hpp"
#include "runtime/evaluator/script_output.hpp"
#include "runtime/object_model/object.hpp"

#include "support/capturing_output.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace philios::test {

TEST_CASE("Execution context limits have the plan's values") {
    static_assert(ExecutionContext::kMaxParseNesting == 256);
    static_assert(ExecutionContext::kMaxCallDepth == 1000);
    static_assert(ExecutionContext::kMaxEvalDepth == 10000);
    const CapturingOutput output;
    const ExecutionContext context(output.writer());
    CHECK(context.callDepth() == 0);
    CHECK(context.evaluationDepth() == 0);
}

TEST_CASE("T-NULL-4 (a) one NullObject per execution context") {
    const CapturingOutput output;
    const ExecutionContext first(output.writer());
    const ExecutionContext second(output.writer());
    REQUIRE(first.nullObject() != nullptr);
    CHECK(first.nullObject() == first.nullObject());
    CHECK(first.nullObject().get() == first.nullObject().get());
    CHECK(first.nullObject()->objectType() == ObjectType::Null);
    CHECK(first.nullObject() != second.nullObject());
}

TEST_CASE("T-SEC-3 the call-depth counter is restored after a call-depth error") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    {
        std::vector<std::unique_ptr<CallDepthGuard>> guards;
        guards.reserve(ExecutionContext::kMaxCallDepth);
        for (std::size_t depth = 0; depth < ExecutionContext::kMaxCallDepth; ++depth) {
            guards.push_back(std::make_unique<CallDepthGuard>(context, SourceLocation{1, 1}));
        }
        CHECK(context.callDepth() == ExecutionContext::kMaxCallDepth);

        bool threw = false;
        try {
            const CallDepthGuard overflow(context, SourceLocation{4, 12});
        } catch (const RuntimeError& error) {
            threw = true;
            CHECK(error.kind() == RuntimeErrorKind::CallDepthExceeded);
            CHECK(error.location() == SourceLocation{4, 12});
        }
        CHECK(threw);
        CHECK(context.callDepth() == ExecutionContext::kMaxCallDepth);
    }
    CHECK(context.callDepth() == 0);
}

TEST_CASE("T-SEC-5 evaluation-depth guard: 10,000 entries succeed, the 10,001st fails") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    {
        std::vector<std::unique_ptr<EvaluationDepthGuard>> guards;
        guards.reserve(ExecutionContext::kMaxEvalDepth);
        for (std::size_t depth = 0; depth < ExecutionContext::kMaxEvalDepth; ++depth) {
            guards.push_back(
                std::make_unique<EvaluationDepthGuard>(context, SourceLocation{2, 5}));
        }
        CHECK(context.evaluationDepth() == ExecutionContext::kMaxEvalDepth);

        bool threw = false;
        try {
            const EvaluationDepthGuard overflow(context, SourceLocation{17, 33});
        } catch (const RuntimeError& error) {
            threw = true;
            CHECK(error.kind() == RuntimeErrorKind::EvaluationDepthExceeded);
            CHECK(error.location() == SourceLocation{17, 33});
            CHECK(std::string(error.what()).find("evaluation nested too deeply") !=
                  std::string::npos);
        }
        CHECK(threw);
        CHECK(context.evaluationDepth() == ExecutionContext::kMaxEvalDepth);
    }
    CHECK(context.evaluationDepth() == 0);
}

TEST_CASE("T-SEC-5 the evaluation depth is restored after normal exits") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    {
        const EvaluationDepthGuard outer(context, SourceLocation{1, 1});
        {
            const EvaluationDepthGuard inner(context, SourceLocation{1, 2});
            CHECK(context.evaluationDepth() == 2);
        }
        CHECK(context.evaluationDepth() == 1);
    }
    CHECK(context.evaluationDepth() == 0);
}

TEST_CASE("T2-ECHO-9 an empty OutputWriter is rejected with InternalError") {
    CHECK_THROWS_AS(ExecutionContext(OutputWriter{}), InternalError);

    // Also through a named, default-constructed (empty) writer passed by copy.
    const OutputWriter empty;
    REQUIRE_FALSE(static_cast<bool>(empty));
    CHECK_THROWS_AS(ExecutionContext(empty), InternalError);
}

TEST_CASE("T2-ECHO-9 writeOutput passes the bytes to the writer and returns its result") {
    CapturingOutput output;
    ExecutionContext context(output.writer());

    SECTION("a successful write returns an empty code and passes the exact bytes") {
        constexpr std::string_view kBytes("a\nb\t\\\"\0\xc3\xa9", 9);
        const std::error_code result = context.writeOutput(kBytes);
        CHECK_FALSE(result);
        REQUIRE(output.callCount() == 1);
        CHECK(output.bytes() == std::string(kBytes));
    }

    SECTION("each call reaches the writer once, in order") {
        CHECK_FALSE(context.writeOutput("a"));
        CHECK_FALSE(context.writeOutput(""));
        CHECK_FALSE(context.writeOutput("b"));
        CHECK(output.writes() == std::vector<std::string>{"a", "", "b"});
        CHECK(output.bytes() == "ab");
    }

    SECTION("a failing writer's code is returned unchanged") {
        const std::error_code failure = std::make_error_code(std::errc::broken_pipe);
        output.setResult(failure);
        const std::error_code result = context.writeOutput("x");
        CHECK(result == failure);
        CHECK(result.category() == std::generic_category());
        CHECK(output.callCount() == 1);
        CHECK(output.bytes() == "x");
    }

    SECTION("writeOutput does not change either depth counter") {
        CHECK_FALSE(context.writeOutput("x"));
        CHECK(context.callDepth() == 0);
        CHECK(context.evaluationDepth() == 0);
    }
}

} // namespace philios::test
