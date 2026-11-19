// Unit tests for AST destruction in src/frontend/ast/ast.hpp (plan 0001 §3.9, CERT DCL57;
// T-OWN-4 and the AST part of T-OWN-5, tasks 6-7; plan 0002 T2-OWN-2 and the EchoCall,
// TypeofCall, and ExprStmt shapes of §3.3 and §3.8, task 5).
//
// The structures below are built directly, far deeper and wider than the parser limit, so a
// destructor that recursed per node, or a worklist node destroyed with its link still set (§3.9
// unlink invariant), would overflow the default 8 MiB stack under ASan.

#include "frontend/ast/ast.hpp"
#include "common/errors.hpp"
#include "runtime/evaluator/completion.hpp"
#include "runtime/evaluator/execution_context.hpp"
#include "support/ast_builders.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace philios::test {
namespace {

constexpr std::size_t kLargeCount = 200'000;

/// A test-only node with no children (T-OWN-5). Never passed to the validator.
class ChildlessNode final : public ASTNode {
public:
    ChildlessNode() noexcept : ASTNode(SourceLocation{}) {}

    [[nodiscard]] NodeKind kind() const noexcept override { return NodeKind::NullLiteral; }

    [[nodiscard]] Completion evaluate(ExecutionContext& /*context*/,
                                      const std::shared_ptr<Environment>& /*environment*/)
        const override {
        throw InternalError("ChildlessNode is not evaluated in this test");
    }

    void detachChildren(std::unique_ptr<ASTNode>& /*worklistHead*/) noexcept override {}
};

// T-OWN-5: every destructor is nothrow and the detach member is noexcept.
static_assert(std::is_nothrow_destructible_v<ASTNode>);
static_assert(std::is_nothrow_destructible_v<FunctionDecl>);
static_assert(std::is_nothrow_destructible_v<Param>);
static_assert(std::is_nothrow_destructible_v<Block>);
static_assert(std::is_nothrow_destructible_v<ReturnStmt>);
static_assert(std::is_nothrow_destructible_v<PassStmt>);
static_assert(std::is_nothrow_destructible_v<ExprStmt>);
static_assert(std::is_nothrow_destructible_v<CallExpr>);
static_assert(std::is_nothrow_destructible_v<IntegerLiteral>);
static_assert(std::is_nothrow_destructible_v<StringLiteral>);
static_assert(std::is_nothrow_destructible_v<NullLiteral>);
static_assert(std::is_nothrow_destructible_v<Identifier>);
static_assert(noexcept(std::declval<ASTNode&>().detachChildren(
    std::declval<std::unique_ptr<ASTNode>&>())));

// Plan 0002 T2-OWN-2: the pseudo-call nodes are nothrow destructible like every other node.
static_assert(std::is_nothrow_destructible_v<EchoCall>);
static_assert(std::is_nothrow_destructible_v<TypeofCall>);

/// A chain of @p depth nested pseudo-calls `typeof(typeof(...typeof("x")...))`, built
/// iteratively (plan 0002 T2-OWN-2).
[[nodiscard]] std::unique_ptr<ASTNode> makeNestedTypeofCalls(std::size_t depth) {
    std::unique_ptr<ASTNode> node = std::make_unique<StringLiteral>(SourceLocation{}, "x");
    for (std::size_t level = 0; level < depth; ++level) {
        node = std::make_unique<TypeofCall>(SourceLocation{}, std::move(node));
    }
    return node;
}

/// A chain of @p depth nested calls `f(f(...f()...))`, built iteratively.
[[nodiscard]] std::unique_ptr<ASTNode> makeNestedCalls(std::size_t depth) {
    std::unique_ptr<ASTNode> node = makeCall("f", {});
    for (std::size_t level = 0; level < depth; ++level) {
        node = makeCall("f", argumentList(std::move(node)));
    }
    return node;
}

/// `function f(): Integer:` whose body is `return <expression>`.
[[nodiscard]] std::shared_ptr<const FunctionDecl>
makeFunctionReturning(std::unique_ptr<ASTNode> expression) {
    std::vector<std::unique_ptr<ASTNode>> statements;
    statements.push_back(std::make_unique<ReturnStmt>(SourceLocation{2, 5}, std::move(expression)));
    auto body = std::make_unique<Block>(SourceLocation{2, 5}, std::move(statements));
    return std::make_shared<const FunctionDecl>(SourceLocation{1, 10}, "f",
                                                std::vector<std::unique_ptr<Param>>{},
                                                DeclaredType{TypeName::Integer, false},
                                                std::move(body));
}

} // namespace

TEST_CASE("T-OWN-4 a call with 200,000 arguments is destroyed without recursion") {
    std::vector<std::unique_ptr<ASTNode>> arguments;
    arguments.reserve(kLargeCount);
    for (std::size_t index = 0; index < kLargeCount; ++index) {
        arguments.push_back(makeIntegerLiteral(static_cast<std::int64_t>(index)));
    }
    std::unique_ptr<CallExpr> call = makeCall("f", std::move(arguments));
    CHECK(call->arguments().size() == kLargeCount);
    call.reset();
    CHECK(call == nullptr);
}

TEST_CASE("T-OWN-4 a chain of 200,000 nested calls is destroyed without recursion") {
    std::unique_ptr<ASTNode> chain = makeNestedCalls(kLargeCount);
    chain.reset();
    CHECK(chain == nullptr);
}

TEST_CASE("T-OWN-4 a FunctionDecl owning a deep AST is destroyed without recursion") {
    std::shared_ptr<const FunctionDecl> declaration =
        makeFunctionReturning(makeNestedCalls(kLargeCount));
    declaration.reset();
    CHECK(declaration == nullptr);
}

TEST_CASE("T-OWN-5 an AST nested to the parser limit and a childless test node are destroyed") {
    std::shared_ptr<const FunctionDecl> declaration =
        makeFunctionReturning(makeNestedCalls(ExecutionContext::kMaxParseNesting));
    declaration.reset();

    std::unique_ptr<CallExpr> call =
        makeCall("f", argumentList(std::make_unique<ChildlessNode>(), makeIntegerLiteral(1)));
    call.reset();
    CHECK(call == nullptr);
}

TEST_CASE("T2-OWN-2 a chain of 200,000 nested TypeofCall nodes is destroyed without recursion") {
    SECTION("the bare chain") {
        std::unique_ptr<ASTNode> chain = makeNestedTypeofCalls(kLargeCount);
        CHECK(chain->kind() == NodeKind::TypeofCall);
        chain.reset();
        CHECK(chain == nullptr);
    }
    SECTION("the chain as an echo argument in a statement owned by a FunctionDecl") {
        auto echo =
            std::make_unique<EchoCall>(SourceLocation{}, makeNestedTypeofCalls(kLargeCount));
        std::vector<std::unique_ptr<ASTNode>> statements;
        statements.push_back(std::make_unique<ExprStmt>(std::move(echo)));
        auto body = std::make_unique<Block>(SourceLocation{2, 5}, std::move(statements));
        auto declaration = std::make_shared<const FunctionDecl>(
            SourceLocation{1, 10}, "f", std::vector<std::unique_ptr<Param>>{},
            DeclaredType{TypeName::Null, false}, std::move(body));
        declaration.reset();
        CHECK(declaration == nullptr);
    }
    SECTION("alternating EchoCall and TypeofCall nodes inside a call argument") {
        std::unique_ptr<ASTNode> node = makeIntegerLiteral(1);
        for (std::size_t level = 0; level < kLargeCount; ++level) {
            if (level % 2 == 0) {
                node = std::make_unique<TypeofCall>(SourceLocation{}, std::move(node));
            } else {
                node = std::make_unique<EchoCall>(SourceLocation{}, std::move(node));
            }
        }
        std::unique_ptr<CallExpr> call = makeCall("f", argumentList(std::move(node)));
        call.reset();
        CHECK(call == nullptr);
    }
}

TEST_CASE("Plan 0002 pseudo-call nodes keep the keyword location and own their argument") {
    SECTION("EchoCall") {
        const EchoCall echo(SourceLocation{2, 5},
                            std::make_unique<StringLiteral>(SourceLocation{2, 10}, "x"));
        CHECK(echo.kind() == NodeKind::EchoCall);
        CHECK(echo.location() == SourceLocation{2, 5});
        CHECK(echo.argument().kind() == NodeKind::StringLiteral);
        CHECK(echo.argument().location() == SourceLocation{2, 10});
    }
    SECTION("TypeofCall") {
        const TypeofCall typeOf(SourceLocation{2, 10},
                                makeIntegerLiteral(1, SourceLocation{2, 17}));
        CHECK(typeOf.kind() == NodeKind::TypeofCall);
        CHECK(typeOf.location() == SourceLocation{2, 10});
        CHECK(typeOf.argument().kind() == NodeKind::IntegerLiteral);
        CHECK(typeOf.argument().location() == SourceLocation{2, 17});
    }
    SECTION("a null argument is an internal error") {
        CHECK_THROWS_AS(EchoCall(SourceLocation{}, nullptr), InternalError);
        CHECK_THROWS_AS(TypeofCall(SourceLocation{}, nullptr), InternalError);
    }
    SECTION("ExprStmt accepts a call or pseudo-call and rejects any other expression") {
        const ExprStmt echoStatement(
            std::make_unique<EchoCall>(SourceLocation{2, 5}, makeIntegerLiteral(1)));
        CHECK(echoStatement.location() == SourceLocation{2, 5});
        CHECK(echoStatement.expression().kind() == NodeKind::EchoCall);
        const ExprStmt typeofStatement(
            std::make_unique<TypeofCall>(SourceLocation{3, 5}, makeIntegerLiteral(1)));
        CHECK(typeofStatement.expression().kind() == NodeKind::TypeofCall);
        const ExprStmt callStatement(makeCall("f", {}, SourceLocation{4, 5}));
        CHECK(callStatement.expression().kind() == NodeKind::CallExpr);
        CHECK_THROWS_AS(ExprStmt(makeIntegerLiteral(1)), InternalError);
        CHECK_THROWS_AS(ExprStmt(nullptr), InternalError);
    }
}

} // namespace philios::test
