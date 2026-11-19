// Unit tests for evaluation (plan 0001 §3.6, §3.7, §3.9, D6, D14, D16, D18, D24, D31b; task 8):
// T-FN-1 (unit part), T-FN-4, T-FN-6, T-FN-8 (unit part), T-NULL-1, T-NULL-4 part (b), T-DECL-8,
// and T-SEC-6. ASTs are built directly or loaded with Module::load; nothing under src/ has a test
// hook (T-FN-6).
//
// Plan 0002 task 5 (§3.5, §3.6): T2-ECHO-8 (EchoCall evaluation with the capturing fake writer,
// and the evaluation-depth guard of both pseudo-calls) and T2-TYPEOF-8 (TypeofCall evaluation).

#include "common/errors.hpp"
#include "frontend/ast/ast.hpp"
#include "runtime/environment/environment.hpp"
#include "runtime/evaluator/completion.hpp"
#include "runtime/evaluator/execution_context.hpp"
#include "runtime/evaluator/module.hpp"
#include "runtime/object_model/integer.hpp"
#include "runtime/object_model/object.hpp"
#include "runtime/object_model/string_object.hpp"
#include "support/ast_builders.hpp"
#include "support/capturing_output.hpp"

#include "support/require_value.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace philios::test {
namespace {

/// Evaluates @p node in @p module's environment.
[[nodiscard]] Completion evaluateIn(ExecutionContext& context, const Module& module,
                                    const ASTNode& node) {
    const std::shared_ptr<Environment> environment = module.environment();
    return node.evaluate(context, environment);
}

/// Evaluates @p node and discards the result (for throw checks).
void evaluateAndDiscard(ExecutionContext& context, const std::shared_ptr<Environment>& environment,
                        const ASTNode& node) {
    [[maybe_unused]] const Completion completion = node.evaluate(context, environment);
}

/// The RuntimeError raised by evaluating @p node, or no value if none is raised.
[[nodiscard]] std::optional<RuntimeError>
runtimeErrorFrom(ExecutionContext& context, const std::shared_ptr<Environment>& environment,
                 const ASTNode& node) {
    try {
        evaluateAndDiscard(context, environment, node);
    } catch (const RuntimeError& error) {
        return error;
    }
    return std::nullopt;
}

/// T-FN-6: a test-only node that records each evaluation in a log owned by the test and returns
/// an Integer, or throws a RuntimeError instead.
class RecordingNode final : public ASTNode {
public:
    RecordingNode(std::shared_ptr<std::vector<std::string>> log, std::string label, bool throws)
        : ASTNode(SourceLocation{}), log_(std::move(log)), label_(std::move(label)),
          throws_(throws) {}

    [[nodiscard]] NodeKind kind() const noexcept override { return NodeKind::IntegerLiteral; }

    [[nodiscard]] Completion evaluate(ExecutionContext& /*context*/,
                                      const std::shared_ptr<Environment>& /*environment*/)
        const override {
        log_->push_back(label_);
        if (throws_) {
            throw RuntimeError(RuntimeErrorKind::ArgumentType, location(), "recording node");
        }
        return Completion{CompletionKind::Normal, std::make_shared<Integer>(1)};
    }

    void detachChildren(std::unique_ptr<ASTNode>& /*worklistHead*/) noexcept override {}

private:
    std::shared_ptr<std::vector<std::string>> log_;
    std::string label_;
    bool throws_;
};

} // namespace

TEST_CASE("T-FN-1 a String helper returns the expected String content") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    const std::unique_ptr<Module> module =
        Module::load(context, "function greet(s: String): String:\n    return s\n"
                              "function main():\n    pass\n");
    const auto call = makeCall(
        "greet", argumentList(std::make_unique<StringLiteral>(SourceLocation{}, "hi\tthere")));
    const Completion completion = evaluateIn(context, *module, *call);
    CHECK(completion.kind == CompletionKind::Normal);
    REQUIRE(completion.value != nullptr);
    REQUIRE(completion.value->objectType() == ObjectType::String);
    CHECK(static_cast<const String&>(*completion.value).contentEquals("hi\tthere"));
}

TEST_CASE("T-FN-4 two references to the same function name yield the same Function object") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    const std::unique_ptr<Module> module =
        Module::load(context, "function id(x: Integer): Integer:\n    return x\n"
                              "function main():\n    pass\n");
    const auto first = makeIdentifier("id");
    const auto second = makeIdentifier("id");
    const Completion a = evaluateIn(context, *module, *first);
    const Completion b = evaluateIn(context, *module, *second);
    REQUIRE(a.value != nullptr);
    CHECK(a.value->objectType() == ObjectType::Function);
    CHECK(a.value == b.value);
}

TEST_CASE("T-FN-6 arguments are evaluated left to right and stop at the first failure") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    const std::unique_ptr<Module> module = Module::load(
        context, "function f(a: Integer, b: Integer, c: Integer): Integer:\n    return a\n"
                 "function main():\n    pass\n");

    SECTION("all succeed") {
        const auto log = std::make_shared<std::vector<std::string>>();
        const auto call = makeCall("f", argumentList(std::make_unique<RecordingNode>(log, "a", false),
                                                     std::make_unique<RecordingNode>(log, "b", false),
                                                     std::make_unique<RecordingNode>(log, "c", false)));
        [[maybe_unused]] const Completion completion = evaluateIn(context, *module, *call);
        CHECK(*log == std::vector<std::string>{"a", "b", "c"});
    }
    SECTION("b throws, so c is not evaluated") {
        const auto log = std::make_shared<std::vector<std::string>>();
        const auto call = makeCall("f", argumentList(std::make_unique<RecordingNode>(log, "a", false),
                                                     std::make_unique<RecordingNode>(log, "b", true),
                                                     std::make_unique<RecordingNode>(log, "c", false)));
        CHECK_THROWS_AS(evaluateAndDiscard(context, module->environment(), *call), RuntimeError);
        CHECK(*log == std::vector<std::string>{"a", "b"});
    }
}

TEST_CASE("T-FN-8 a failing argument wins over the callee-kind and argument-count errors") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    SECTION("over a non-Function callee") {
        const std::unique_ptr<Module> module =
            Module::load(context, "function fail(): Integer:\n    pass\n"
                                  "function call(x: Integer): Integer:\n    return x(fail())\n"
                                  "function main():\n    return call(1)\n");
        const auto call = makeCall("call", argumentList(makeIntegerLiteral(1)));
        const RuntimeError error =
            requireValue(runtimeErrorFrom(context, module->environment(), *call));
        CHECK(error.kind() == RuntimeErrorKind::MissingReturn);
        CHECK(error.location() == SourceLocation{1, 10});
    }
    SECTION("over a wrong argument count") {
        const std::unique_ptr<Module> module =
            Module::load(context, "function fail(): Integer:\n    pass\n"
                                  "function one(x: Integer): Integer:\n    return x\n"
                                  "function main():\n    return one(1, fail())\n");
        const auto call =
            makeCall("one", argumentList(makeIntegerLiteral(1), makeCall("fail", {})));
        const RuntimeError error =
            requireValue(runtimeErrorFrom(context, module->environment(), *call));
        CHECK(error.kind() == RuntimeErrorKind::MissingReturn);
        CHECK(error.location() == SourceLocation{1, 10});
    }
}

TEST_CASE("T-RUN-5 a return-type mismatch is reported at the executed return, not the declaration") {
    // Code-review round 1 regression (§4 return-type row): the declaration of f is at 1:10; the
    // executed `return` is at 3:13, after a comment on its line.
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    const std::unique_ptr<Module> module =
        Module::load(context, "function f(): Integer:\n    pass\n    /* x */ return \"s\"\n"
                              "function main():\n    pass\n");
    const auto call = makeCall("f", {});
    const RuntimeError error = requireValue(runtimeErrorFrom(context, module->environment(), *call));
    CHECK(error.kind() == RuntimeErrorKind::ReturnType);
    CHECK(error.errorClass() == ErrorClass::Runtime);
    CHECK(error.location() == SourceLocation{3, 13});
}

TEST_CASE("T-NULL-1 and T-NULL-4 (b) every null in one execution is the context's instance") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    const std::unique_ptr<Module> module =
        Module::load(context, "function n(x: null): null:\n    return x\n"
                              "function bare(): null:\n    return\n"
                              "function explicitNull(): null:\n    return null\n"
                              "function falls(): null:\n    pass\n"
                              "function main():\n    pass\n");
    const std::shared_ptr<Object> expected = context.nullObject();

    SECTION("the null literal") {
        const NullLiteral literal(SourceLocation{});
        CHECK(evaluateIn(context, *module, literal).value == expected);
    }
    SECTION("n(null) returns null") {
        const auto call =
            makeCall("n", argumentList(std::make_unique<NullLiteral>(SourceLocation{})));
        const Completion completion = evaluateIn(context, *module, *call);
        REQUIRE(completion.value != nullptr);
        CHECK(completion.value->objectType() == ObjectType::Null);
        CHECK(completion.value == expected);
    }
    SECTION("bare return, return null, and falling off the end") {
        for (const std::string_view name : {"bare", "explicitNull", "falls"}) {
            const auto call = makeCall(name, {});
            const Completion completion = evaluateIn(context, *module, *call);
            REQUIRE(completion.value != nullptr);
            CHECK(completion.value->objectType() == ObjectType::Null);
            CHECK(completion.value == expected);
        }
    }
}

TEST_CASE("T-DECL-8 an unresolved identifier is a runtime error with no fallback") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    const auto identifier = makeIdentifier("nowhere", SourceLocation{7, 3});

    SECTION("in an empty module environment") {
        const auto environment = std::make_shared<Environment>(nullptr);
        const RuntimeError error = requireValue(runtimeErrorFrom(context, environment, *identifier));
        CHECK(error.kind() == RuntimeErrorKind::UndefinedIdentifier);
        CHECK(error.errorClass() == ErrorClass::Runtime);
        CHECK(error.location() == SourceLocation{7, 3});
    }
    SECTION("in a call environment enclosed by a loaded module") {
        const std::unique_ptr<Module> module =
            Module::load(context, "function main():\n    pass\n");
        const auto callEnvironment = std::make_shared<Environment>(module->environment());
        const RuntimeError error =
            requireValue(runtimeErrorFrom(context, callEnvironment, *identifier));
        CHECK(error.kind() == RuntimeErrorKind::UndefinedIdentifier);
        CHECK(error.location() == SourceLocation{7, 3});
    }
}

TEST_CASE("T-SEC-6 when both depth limits would trip, the call-depth error is reported") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    const std::unique_ptr<Module> module =
        Module::load(context, "function f(): Integer:\n    return 1\n"
                              "function main():\n    pass\n");

    std::vector<std::unique_ptr<CallDepthGuard>> callGuards;
    callGuards.reserve(ExecutionContext::kMaxCallDepth);
    for (std::size_t depth = 0; depth < ExecutionContext::kMaxCallDepth; ++depth) {
        callGuards.push_back(std::make_unique<CallDepthGuard>(context, SourceLocation{1, 1}));
    }
    std::vector<std::unique_ptr<EvaluationDepthGuard>> evaluationGuards;
    evaluationGuards.reserve(ExecutionContext::kMaxEvalDepth);
    for (std::size_t depth = 0; depth < ExecutionContext::kMaxEvalDepth; ++depth) {
        evaluationGuards.push_back(
            std::make_unique<EvaluationDepthGuard>(context, SourceLocation{1, 1}));
    }

    const auto call = makeCall("f", {}, SourceLocation{9, 4});
    const RuntimeError error =
        requireValue(runtimeErrorFrom(context, module->environment(), *call));
    CHECK(error.kind() == RuntimeErrorKind::CallDepthExceeded);
    CHECK(error.location() == SourceLocation{9, 4});
}

// ---------------------------------------------------------------------------------------------
// Plan 0002 task 5: EchoCall and TypeofCall evaluation (§3.5, §3.6; T2-ECHO-8, T2-TYPEOF-8).
// ---------------------------------------------------------------------------------------------

namespace {

/// A module whose environment binds `id` to a Function, for Function-valued arguments.
constexpr std::string_view kIdModule = "function id(x: Integer): Integer:\n    return x\n"
                                       "function main():\n    pass\n";

/// Whether @p completion is Normal and holds a String equal to @p expected.
[[nodiscard]] bool holdsString(const Completion& completion, std::string_view expected) {
    return completion.kind == CompletionKind::Normal && completion.value != nullptr &&
           completion.value->objectType() == ObjectType::String &&
           static_cast<const String&>(*completion.value).contentEquals(expected);
}

/// Fills the evaluation depth to ExecutionContext::kMaxEvalDepth; the guards release it.
[[nodiscard]] std::vector<std::unique_ptr<EvaluationDepthGuard>>
fillEvaluationDepth(ExecutionContext& context) {
    std::vector<std::unique_ptr<EvaluationDepthGuard>> guards;
    guards.reserve(ExecutionContext::kMaxEvalDepth);
    for (std::size_t depth = 0; depth < ExecutionContext::kMaxEvalDepth; ++depth) {
        guards.push_back(std::make_unique<EvaluationDepthGuard>(context, SourceLocation{1, 1}));
    }
    return guards;
}

} // namespace

TEST_CASE("T2-ECHO-8 a String argument is written once with exact bytes and echo returns null") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    const auto environment = std::make_shared<Environment>(nullptr);

    SECTION("bytes including a line break, a tab, a NUL, and UTF-8") {
        const std::string bytes("a\nb\t\\\"\0\xc3\xa9", 9);
        const EchoCall echo(SourceLocation{2, 5},
                            std::make_unique<StringLiteral>(SourceLocation{2, 10}, bytes));
        const Completion completion = echo.evaluate(context, environment);
        CHECK(completion.kind == CompletionKind::Normal);
        CHECK(completion.value == context.nullObject());
        CHECK(output.writes() == std::vector<std::string>{bytes});
        CHECK(output.callCount() == 1);
    }
    SECTION("an empty String writes no bytes and returns null") {
        const EchoCall echo(SourceLocation{2, 5},
                            std::make_unique<StringLiteral>(SourceLocation{2, 10}, ""));
        const Completion completion = echo.evaluate(context, environment);
        CHECK(completion.kind == CompletionKind::Normal);
        CHECK(completion.value == context.nullObject());
        CHECK(output.bytes().empty());
    }
    SECTION("two echoes write in order") {
        const EchoCall first(SourceLocation{2, 5},
                             std::make_unique<StringLiteral>(SourceLocation{2, 10}, "a"));
        const EchoCall second(SourceLocation{3, 5},
                              std::make_unique<StringLiteral>(SourceLocation{3, 10}, "b"));
        evaluateAndDiscard(context, environment, first);
        evaluateAndDiscard(context, environment, second);
        CHECK(output.writes() == std::vector<std::string>{"a", "b"});
    }
}

TEST_CASE("T2-ECHO-8 a non-String argument is an argument-type error at the argument") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    const std::unique_ptr<Module> module = Module::load(context, std::string(kIdModule));
    const std::shared_ptr<Environment> environment = module->environment();

    /// Checks the ArgumentType error at @p argumentLocation and that nothing was written.
    const auto checkArgumentTypeError = [&](const EchoCall& echo, SourceLocation argumentLocation) {
        const RuntimeError error = requireValue(runtimeErrorFrom(context, environment, echo));
        CHECK(error.kind() == RuntimeErrorKind::ArgumentType);
        CHECK(error.errorClass() == ErrorClass::Runtime);
        CHECK(error.location() == argumentLocation);
    };

    SECTION("an Integer") {
        const EchoCall echo(SourceLocation{2, 5}, makeIntegerLiteral(1, SourceLocation{2, 10}));
        checkArgumentTypeError(echo, SourceLocation{2, 10});
        CHECK(output.callCount() == 0);
    }
    SECTION("null") {
        const EchoCall echo(SourceLocation{2, 5},
                            std::make_unique<NullLiteral>(SourceLocation{2, 10}));
        checkArgumentTypeError(echo, SourceLocation{2, 10});
        CHECK(output.callCount() == 0);
    }
    SECTION("a Function") {
        const EchoCall echo(SourceLocation{4, 5}, makeIdentifier("id", SourceLocation{4, 10}));
        checkArgumentTypeError(echo, SourceLocation{4, 10});
        CHECK(output.callCount() == 0);
    }
    SECTION("the null result of an inner echo, after the inner echo has written") {
        auto inner = std::make_unique<EchoCall>(
            SourceLocation{2, 10}, std::make_unique<StringLiteral>(SourceLocation{2, 15}, "x"));
        const EchoCall outer(SourceLocation{2, 5}, std::move(inner));
        checkArgumentTypeError(outer, SourceLocation{2, 10});
        CHECK(output.writes() == std::vector<std::string>{"x"});
    }
    SECTION("an argument is evaluated exactly once before the check") {
        const auto log = std::make_shared<std::vector<std::string>>();
        const EchoCall echo(SourceLocation{2, 5},
                            std::make_unique<RecordingNode>(log, "argument", false));
        checkArgumentTypeError(echo, SourceLocation{});
        CHECK(*log == std::vector<std::string>{"argument"});
        CHECK(output.callCount() == 0);
    }
}

TEST_CASE("T2-ECHO-8 a failing writer is the output-write error at the echo keyword") {
    CapturingOutput output;
    output.setResult(std::make_error_code(std::errc::broken_pipe));
    ExecutionContext context(output.writer());
    const auto environment = std::make_shared<Environment>(nullptr);
    const EchoCall echo(SourceLocation{2, 5},
                        std::make_unique<StringLiteral>(SourceLocation{2, 10}, "x"));

    const RuntimeError error = requireValue(runtimeErrorFrom(context, environment, echo));
    CHECK(error.kind() == RuntimeErrorKind::OutputWrite);
    CHECK(error.errorClass() == ErrorClass::Runtime);
    CHECK(error.location() == SourceLocation{2, 5});
    CHECK(output.writes() == std::vector<std::string>{"x"});

    // §3.6: the message contains `standard output`, followed by the code's reason text.
    const std::string message = error.what();
    const std::string reason = std::make_error_code(std::errc::broken_pipe).message();
    const std::size_t phraseAt = message.find("standard output");
    REQUIRE(phraseAt != std::string::npos);
    CHECK(message.find(reason, phraseAt) != std::string::npos);
}

TEST_CASE("T2-ECHO-8 the call depth seen by the writer equals the depth before the echo") {
    // The writer observes the context through a pointer set after construction, because the
    // context takes the writer in its constructor. Both locals outlive the context.
    const ExecutionContext* observed = nullptr;
    std::vector<std::size_t> seenCallDepths;
    ExecutionContext context(
        [&observed, &seenCallDepths](std::string_view /*bytes*/) -> std::error_code {
            seenCallDepths.push_back(observed == nullptr
                                         ? std::numeric_limits<std::size_t>::max()
                                         : observed->callDepth());
            return {};
        });
    observed = &context;

    // As inside `main`: one active Philios call.
    const CallDepthGuard mainCall(context, SourceLocation{1, 10});
    const std::size_t depthBefore = context.callDepth();
    const auto environment = std::make_shared<Environment>(nullptr);
    const EchoCall echo(SourceLocation{2, 5},
                        std::make_unique<StringLiteral>(SourceLocation{2, 10}, "x"));

    evaluateAndDiscard(context, environment, echo);
    CHECK(seenCallDepths == std::vector<std::size_t>{depthBefore});
    CHECK(context.callDepth() == depthBefore);
}

TEST_CASE("T2-ECHO-8 at the evaluation-depth limit a pseudo-call fails at its keyword") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    const auto environment = std::make_shared<Environment>(nullptr);
    const auto log = std::make_shared<std::vector<std::string>>();
    const std::vector<std::unique_ptr<EvaluationDepthGuard>> guards = fillEvaluationDepth(context);

    SECTION("EchoCall") {
        const EchoCall echo(SourceLocation{3, 7},
                            std::make_unique<RecordingNode>(log, "argument", false));
        const RuntimeError error = requireValue(runtimeErrorFrom(context, environment, echo));
        CHECK(error.kind() == RuntimeErrorKind::EvaluationDepthExceeded);
        CHECK(error.location() == SourceLocation{3, 7});
    }
    SECTION("TypeofCall") {
        const TypeofCall typeOf(SourceLocation{3, 7},
                                std::make_unique<RecordingNode>(log, "argument", false));
        const RuntimeError error = requireValue(runtimeErrorFrom(context, environment, typeOf));
        CHECK(error.kind() == RuntimeErrorKind::EvaluationDepthExceeded);
        CHECK(error.location() == SourceLocation{3, 7});
    }
    CHECK(log->empty());
    CHECK(output.callCount() == 0);
    CHECK(context.evaluationDepth() == ExecutionContext::kMaxEvalDepth);
}

TEST_CASE("T2-TYPEOF-8 each evaluation returns a new String holding the runtime type name") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    const std::unique_ptr<Module> module = Module::load(context, std::string(kIdModule));
    const std::shared_ptr<Environment> environment = module->environment();

    /// Evaluates @p typeOf twice; both results hold @p expected and are distinct objects.
    const auto checkTypeName = [&](const TypeofCall& typeOf, std::string_view expected) {
        const Completion first = typeOf.evaluate(context, environment);
        const Completion second = typeOf.evaluate(context, environment);
        CHECK(holdsString(first, expected));
        CHECK(holdsString(second, expected));
        CHECK(first.value != second.value);
    };

    SECTION("Integer") {
        checkTypeName(TypeofCall(SourceLocation{}, makeIntegerLiteral(-1)), "Integer");
    }
    SECTION("String") {
        checkTypeName(
            TypeofCall(SourceLocation{}, std::make_unique<StringLiteral>(SourceLocation{}, "s")),
            "String");
    }
    SECTION("null") {
        checkTypeName(TypeofCall(SourceLocation{}, std::make_unique<NullLiteral>(SourceLocation{})),
                      "null");
    }
    SECTION("Function") {
        checkTypeName(TypeofCall(SourceLocation{}, makeIdentifier("id")), "Function");
    }
    SECTION("the runtime type of a call result") {
        checkTypeName(
            TypeofCall(SourceLocation{}, makeCall("id", argumentList(makeIntegerLiteral(1)))),
            "Integer");
    }
    SECTION("typeof of typeof is String") {
        checkTypeName(
            TypeofCall(SourceLocation{},
                       std::make_unique<TypeofCall>(SourceLocation{}, makeIntegerLiteral(1))),
            "String");
    }
    SECTION("no output is written") {
        checkTypeName(TypeofCall(SourceLocation{}, makeIntegerLiteral(1)), "Integer");
        CHECK(output.callCount() == 0);
    }
}

TEST_CASE("T2-TYPEOF-8 a recording argument is evaluated exactly once") {
    const CapturingOutput output;
    ExecutionContext context(output.writer());
    const auto environment = std::make_shared<Environment>(nullptr);
    const auto log = std::make_shared<std::vector<std::string>>();
    const TypeofCall typeOf(SourceLocation{2, 10},
                            std::make_unique<RecordingNode>(log, "argument", false));

    const Completion completion = typeOf.evaluate(context, environment);
    CHECK(holdsString(completion, "Integer"));
    CHECK(*log == std::vector<std::string>{"argument"});
    CHECK(context.callDepth() == 0);
    CHECK(context.evaluationDepth() == 0);
}

} // namespace philios::test
