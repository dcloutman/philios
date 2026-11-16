// Unit tests for src/runtime/evaluator/execution_context.hpp (plan 0001 §3.7, §3.9, D7, D18,
// D24; T-NULL-4 part (a), T-SEC-3, T-SEC-5, task 3). No AST nodes are needed.

#include "runtime/evaluator/execution_context.hpp"
#include "common/errors.hpp"
#include "runtime/object_model/object.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace philios::test {

TEST_CASE("Execution context limits have the plan's values") {
    static_assert(ExecutionContext::kMaxParseNesting == 256);
    static_assert(ExecutionContext::kMaxCallDepth == 1000);
    static_assert(ExecutionContext::kMaxEvalDepth == 10000);
    const ExecutionContext context;
    CHECK(context.callDepth() == 0);
    CHECK(context.evaluationDepth() == 0);
}

TEST_CASE("T-NULL-4 (a) one NullObject per execution context") {
    const ExecutionContext first;
    const ExecutionContext second;
    REQUIRE(first.nullObject() != nullptr);
    CHECK(first.nullObject() == first.nullObject());
    CHECK(first.nullObject().get() == first.nullObject().get());
    CHECK(first.nullObject()->objectType() == ObjectType::Null);
    CHECK(first.nullObject() != second.nullObject());
}

TEST_CASE("T-SEC-3 the call-depth counter is restored after a call-depth error") {
    ExecutionContext context;
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
    ExecutionContext context;
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
    ExecutionContext context;
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

} // namespace philios::test
