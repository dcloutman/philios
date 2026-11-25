// Unit tests for AST destruction in src/frontend/ast/ast.hpp (plan 0001 §3.9, CERT DCL57;
// T-OWN-4 and the AST part of T-OWN-5, tasks 6-7; plan 0002 T2-OWN-2 and the EchoCall,
// TypeofCall, and ExprStmt shapes of §3.3 and §3.8, task 5).
//
// Plan 0003 task 2: owned node names (T3-ARCH-9, §4.14.2 "Name storage"), asFunctionDecl and
// module items (§4.14.2), the new node shapes (§4.12), and the AST destruction part of T3-NEST-5
// (§6.1).
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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

namespace {

// Plan 0003 §4.14.2: destructors stay nothrow, and the Identifier and Param constructors are no
// longer noexcept because they copy their name (std::bad_alloc).
static_assert(std::is_nothrow_destructible_v<BooleanLiteral>);
static_assert(std::is_nothrow_destructible_v<UnaryExpr>);
static_assert(std::is_nothrow_destructible_v<BinaryExpr>);
static_assert(std::is_nothrow_destructible_v<LetDecl>);
static_assert(std::is_nothrow_destructible_v<ConstDecl>);
static_assert(std::is_nothrow_destructible_v<SymbolDecl>);
static_assert(std::is_nothrow_destructible_v<AssignStmt>);
static_assert(!std::is_nothrow_constructible_v<Identifier, SourceLocation, std::string_view>);
static_assert(
    !std::is_nothrow_constructible_v<Param, SourceLocation, std::string_view, DeclaredType>);

/// More than 22 bytes: longer than the small-string buffer of both libstdc++ and libc++, so a
/// `std::string` holding it owns a heap allocation (T3-ARCH-9).
constexpr std::string_view kLongName = "a_node_name_longer_than_any_small_string_buffer";
static_assert(kLongName.size() > 22);

/// 10,000 declarations and 10,000 assignments (T3-NEST-5).
constexpr std::size_t kStatementCount = 10'000;

/// T3-ARCH-9 for one node class. @p makeNode builds a node (owned by any smart pointer) whose
/// name is the given view.
///
/// First the source string is overwritten in place: its heap buffer stays allocated, so a node
/// that still viewed it would read the new bytes (a clean check failure), never freed memory.
/// Only if that passes is a second source string destroyed before name() is read, so no
/// heap-use-after-free is reached under ASan while a node still holds a view.
template <typename MakeNode>
void checkNodeOwnsName(MakeNode makeNode) {
    {
        std::string text(kLongName);
        const auto node = makeNode(std::string_view{text});
        std::fill(text.begin(), text.end(), '#');
        REQUIRE(node->name() == kLongName);
    }
    {
        auto text = std::make_unique<std::string>(kLongName);
        const auto node = makeNode(std::string_view{*text});
        text.reset();
        CHECK(node->name() == kLongName);
    }
}

/// `function <name>(): Integer:` whose body is `return 1`.
[[nodiscard]] std::shared_ptr<const FunctionDecl> makeFunctionNamed(std::string_view name) {
    std::vector<std::unique_ptr<ASTNode>> statements;
    statements.push_back(std::make_unique<ReturnStmt>(SourceLocation{2, 5}, makeIntegerLiteral(1)));
    auto body = std::make_unique<Block>(SourceLocation{2, 5}, std::move(statements));
    return std::make_shared<const FunctionDecl>(SourceLocation{1, 10}, name,
                                                std::vector<std::unique_ptr<Param>>{},
                                                DeclaredType{TypeName::Integer, false},
                                                std::move(body));
}

/// A left-associative chain of @p depth `==`/`!=` operators, `((1 == 1) != 1) == ...`, built
/// iteratively; each operator's left operand is the previous chain.
[[nodiscard]] std::unique_ptr<ASTNode> makeLeftEqualityChain(std::size_t depth) {
    std::unique_ptr<ASTNode> node = makeIntegerLiteral(1);
    for (std::size_t level = 0; level < depth; ++level) {
        const BinaryOperator op = (level % 2 == 0) ? BinaryOperator::Equal : BinaryOperator::NotEqual;
        node = std::make_unique<BinaryExpr>(op, SourceLocation{}, std::move(node),
                                            makeIntegerLiteral(1));
    }
    return node;
}

/// A chain of @p depth `==` operators nested through the right operand, `1 == (1 == (...))`,
/// built iteratively (not parser output; checks the right child's destruction path).
[[nodiscard]] std::unique_ptr<ASTNode> makeRightEqualityChain(std::size_t depth) {
    std::unique_ptr<ASTNode> node = std::make_unique<BooleanLiteral>(SourceLocation{}, true);
    for (std::size_t level = 0; level < depth; ++level) {
        node = std::make_unique<BinaryExpr>(BinaryOperator::Equal, SourceLocation{},
                                            makeIntegerLiteral(1), std::move(node));
    }
    return node;
}

/// @p depth levels alternating UnaryExpr (`!`) and BinaryExpr (`!=`), built iteratively.
[[nodiscard]] std::unique_ptr<ASTNode> makeMixedOperatorNesting(std::size_t depth) {
    std::unique_ptr<ASTNode> node = std::make_unique<BooleanLiteral>(SourceLocation{}, false);
    for (std::size_t level = 0; level < depth; ++level) {
        if (level % 2 == 0) {
            node = std::make_unique<UnaryExpr>(SourceLocation{}, UnaryOperator::Not,
                                               std::move(node));
        } else {
            node = std::make_unique<BinaryExpr>(BinaryOperator::NotEqual, SourceLocation{},
                                                std::move(node),
                                                std::make_unique<BooleanLiteral>(SourceLocation{},
                                                                                 true));
        }
    }
    return node;
}

/// A FunctionDecl `main` whose body holds @p statements.
[[nodiscard]] std::shared_ptr<const FunctionDecl>
makeMainWithBody(std::vector<std::unique_ptr<ASTNode>> statements) {
    auto body = std::make_unique<Block>(SourceLocation{2, 5}, std::move(statements));
    return std::make_shared<const FunctionDecl>(SourceLocation{1, 10}, "main",
                                                std::vector<std::unique_ptr<Param>>{},
                                                std::nullopt, std::move(body));
}

} // namespace

TEST_CASE("T3-ARCH-9 every named node owns its name text") {
    SECTION("Identifier") {
        checkNodeOwnsName([](std::string_view name) {
            return std::make_unique<Identifier>(SourceLocation{2, 5}, name);
        });
    }
    SECTION("Param") {
        checkNodeOwnsName([](std::string_view name) {
            return std::make_unique<Param>(SourceLocation{1, 12}, name,
                                           DeclaredType{TypeName::Integer, false});
        });
    }
    SECTION("FunctionDecl") {
        checkNodeOwnsName([](std::string_view name) { return makeFunctionNamed(name); });
    }
    SECTION("LetDecl") {
        checkNodeOwnsName([](std::string_view name) {
            return std::make_unique<LetDecl>(SourceLocation{2, 5}, name, SourceLocation{2, 9},
                                             DeclaredType{TypeName::Integer, true}, nullptr);
        });
    }
    SECTION("ConstDecl") {
        checkNodeOwnsName([](std::string_view name) {
            return std::make_unique<ConstDecl>(SourceLocation{1, 1}, name, SourceLocation{1, 7},
                                               DeclaredType{TypeName::Integer, false},
                                               makeIntegerLiteral(1));
        });
    }
    SECTION("SymbolDecl") {
        checkNodeOwnsName([](std::string_view name) {
            return std::make_unique<SymbolDecl>(SourceLocation{1, 8}, name);
        });
    }
    SECTION("AssignStmt") {
        checkNodeOwnsName([](std::string_view name) {
            return std::make_unique<AssignStmt>(SourceLocation{3, 5}, name,
                                                makeIntegerLiteral(2));
        });
    }
}

TEST_CASE("Plan 0003 asFunctionDecl shares FunctionDecl items and rejects other items") {
    SECTION("a FunctionDecl item shares the item's control block") {
        ModuleItem item = makeFunctionNamed("f");
        const std::weak_ptr<const ASTNode> observer = item;
        std::shared_ptr<const FunctionDecl> declaration = asFunctionDecl(item);
        REQUIRE(declaration != nullptr);
        CHECK(static_cast<const ASTNode*>(declaration.get()) == item.get());
        CHECK(item.use_count() == 2);
        CHECK_FALSE(declaration.owner_before(item));
        CHECK_FALSE(item.owner_before(declaration));

        item.reset();
        CHECK_FALSE(observer.expired());
        CHECK(declaration->name() == "f");
        declaration.reset();
        CHECK(observer.expired());
    }
    SECTION("ConstDecl and SymbolDecl items give null and take no ownership") {
        const ModuleItem constant = std::make_shared<const ConstDecl>(
            SourceLocation{1, 1}, "limit", SourceLocation{1, 7},
            DeclaredType{TypeName::Integer, false}, makeIntegerLiteral(3));
        const ModuleItem symbol = std::make_shared<const SymbolDecl>(SourceLocation{2, 8}, "red");
        CHECK(asFunctionDecl(constant) == nullptr);
        CHECK(asFunctionDecl(symbol) == nullptr);
        CHECK(constant.use_count() == 1);
        CHECK(symbol.use_count() == 1);
    }
    SECTION("an item of any other kind gives null") {
        const ModuleItem other = std::make_shared<const Identifier>(SourceLocation{1, 1}, "x");
        CHECK(asFunctionDecl(other) == nullptr);
    }
    SECTION("a null item is an internal error") {
        CHECK_THROWS_AS(asFunctionDecl(ModuleItem{}), InternalError);
        CHECK_THROWS_AS(asFunctionDecl(nullptr), InternalError);
    }
    SECTION("a Program holds FunctionDecl, ConstDecl, and SymbolDecl items in order") {
        Program program;
        program.push_back(std::make_shared<const ConstDecl>(
            SourceLocation{1, 1}, "limit", SourceLocation{1, 7},
            DeclaredType{TypeName::Integer, false}, makeIntegerLiteral(3)));
        program.push_back(std::make_shared<const SymbolDecl>(SourceLocation{2, 8}, "red"));
        program.push_back(makeFunctionNamed("main"));
        REQUIRE(program.size() == 3);
        CHECK(program[0]->kind() == NodeKind::ConstDecl);
        CHECK(program[1]->kind() == NodeKind::SymbolDecl);
        CHECK(program[2]->kind() == NodeKind::FunctionDecl);
        REQUIRE(asFunctionDecl(program[2]) != nullptr);
        CHECK(asFunctionDecl(program[2])->name() == "main");
    }
}

TEST_CASE("Plan 0003 declaration, assignment, and operator nodes keep their parts") {
    SECTION("BooleanLiteral") {
        const BooleanLiteral literal(SourceLocation{2, 12}, true);
        CHECK(literal.kind() == NodeKind::BooleanLiteral);
        CHECK(literal.location() == SourceLocation{2, 12});
        CHECK(literal.value());
        CHECK_FALSE(BooleanLiteral(SourceLocation{}, false).value());
    }
    SECTION("UnaryExpr") {
        const UnaryExpr negation(SourceLocation{2, 12}, UnaryOperator::Not,
                                 makeIdentifier("flag", SourceLocation{2, 13}));
        CHECK(negation.kind() == NodeKind::UnaryExpr);
        CHECK(negation.location() == SourceLocation{2, 12});
        CHECK(negation.op() == UnaryOperator::Not);
        CHECK(negation.operand().location() == SourceLocation{2, 13});
        CHECK_THROWS_AS(UnaryExpr(SourceLocation{}, UnaryOperator::Not, nullptr), InternalError);
    }
    SECTION("BinaryExpr") {
        const BinaryExpr equality(BinaryOperator::NotEqual, SourceLocation{2, 14},
                                  makeIdentifier("a", SourceLocation{2, 12}),
                                  makeIntegerLiteral(1, SourceLocation{2, 17}));
        CHECK(equality.kind() == NodeKind::BinaryExpr);
        CHECK(equality.location() == SourceLocation{2, 12});
        CHECK(equality.operatorLocation() == SourceLocation{2, 14});
        CHECK(equality.op() == BinaryOperator::NotEqual);
        CHECK(equality.left().kind() == NodeKind::Identifier);
        CHECK(equality.right().location() == SourceLocation{2, 17});
        CHECK_THROWS_AS(BinaryExpr(BinaryOperator::Equal, SourceLocation{}, nullptr,
                                   makeIntegerLiteral(1)),
                        InternalError);
        CHECK_THROWS_AS(BinaryExpr(BinaryOperator::Equal, SourceLocation{},
                                   makeIntegerLiteral(1), nullptr),
                        InternalError);
    }
    SECTION("LetDecl with and without an initializer") {
        const LetDecl initialized(SourceLocation{2, 5}, "count", SourceLocation{2, 9},
                                  DeclaredType{TypeName::Integer, false},
                                  makeIntegerLiteral(0, SourceLocation{2, 25}));
        CHECK(initialized.kind() == NodeKind::LetDecl);
        CHECK(initialized.location() == SourceLocation{2, 5});
        CHECK(initialized.nameLocation() == SourceLocation{2, 9});
        CHECK(initialized.type() == DeclaredType{TypeName::Integer, false});
        REQUIRE(initialized.initializer() != nullptr);
        CHECK(initialized.initializer()->location() == SourceLocation{2, 25});

        const LetDecl uninitialized(SourceLocation{3, 5}, "flag", SourceLocation{3, 9},
                                    DeclaredType{TypeName::Boolean, true}, nullptr);
        CHECK(uninitialized.type() == DeclaredType{TypeName::Boolean, true});
        CHECK(uninitialized.initializer() == nullptr);
    }
    SECTION("ConstDecl with and without an initializer") {
        const ConstDecl initialized(SourceLocation{1, 1}, "limit", SourceLocation{1, 7},
                                    DeclaredType{TypeName::Symbol, false},
                                    makeIdentifier("red", SourceLocation{1, 22}));
        CHECK(initialized.kind() == NodeKind::ConstDecl);
        CHECK(initialized.location() == SourceLocation{1, 1});
        CHECK(initialized.nameLocation() == SourceLocation{1, 7});
        CHECK(initialized.type() == DeclaredType{TypeName::Symbol, false});
        REQUIRE(initialized.initializer() != nullptr);
        CHECK(initialized.initializer()->kind() == NodeKind::Identifier);

        const ConstDecl uninitialized(SourceLocation{2, 5}, "later", SourceLocation{2, 11},
                                      DeclaredType{TypeName::String, true}, nullptr);
        CHECK(uninitialized.initializer() == nullptr);
    }
    SECTION("SymbolDecl") {
        const SymbolDecl symbol(SourceLocation{1, 8}, "red");
        CHECK(symbol.kind() == NodeKind::SymbolDecl);
        CHECK(symbol.location() == SourceLocation{1, 8});
        CHECK(symbol.name() == "red");
    }
    SECTION("AssignStmt") {
        const AssignStmt assignment(SourceLocation{3, 5}, "count",
                                    makeIntegerLiteral(2, SourceLocation{3, 13}));
        CHECK(assignment.kind() == NodeKind::AssignStmt);
        CHECK(assignment.location() == SourceLocation{3, 5});
        CHECK(assignment.name() == "count");
        CHECK(assignment.value().location() == SourceLocation{3, 13});
        CHECK_THROWS_AS(AssignStmt(SourceLocation{}, "count", nullptr), InternalError);
    }
}

TEST_CASE("T3-NEST-5 a body with 10,000 let declarations and 10,000 assignments is destroyed") {
    std::vector<std::unique_ptr<ASTNode>> statements;
    statements.reserve(2 * kStatementCount);
    for (std::size_t index = 0; index < kStatementCount; ++index) {
        const std::string name = "value_" + std::to_string(index);
        statements.push_back(std::make_unique<LetDecl>(
            SourceLocation{}, name, SourceLocation{}, DeclaredType{TypeName::Integer, false},
            makeIntegerLiteral(static_cast<std::int64_t>(index))));
    }
    for (std::size_t index = 0; index < kStatementCount; ++index) {
        const std::string name = "value_" + std::to_string(index);
        statements.push_back(std::make_unique<AssignStmt>(
            SourceLocation{}, name, makeIntegerLiteral(static_cast<std::int64_t>(index) + 1)));
    }
    std::shared_ptr<const FunctionDecl> declaration = makeMainWithBody(std::move(statements));
    REQUIRE(declaration->body().statements().size() == 2 * kStatementCount);
    CHECK(declaration->body().statements().front()->kind() == NodeKind::LetDecl);
    CHECK(declaration->body().statements().back()->kind() == NodeKind::AssignStmt);
    declaration.reset();
    CHECK(declaration == nullptr);
}

TEST_CASE("T3-NEST-5 operator expressions are destroyed without recursion per node") {
    SECTION("an equality chain at the parser nesting limit, returned from a function") {
        std::shared_ptr<const FunctionDecl> declaration =
            makeFunctionReturning(makeLeftEqualityChain(ExecutionContext::kMaxParseNesting));
        declaration.reset();
        CHECK(declaration == nullptr);
    }
    SECTION("alternating ! and != at the parser nesting limit") {
        std::unique_ptr<ASTNode> expression =
            makeMixedOperatorNesting(ExecutionContext::kMaxParseNesting);
        CHECK(expression->kind() == NodeKind::BinaryExpr); // 256 levels: the last is a `!=`.
        expression.reset();
        CHECK(expression == nullptr);
    }
    SECTION("a left-associative chain of 200,000 operators") {
        std::unique_ptr<ASTNode> chain = makeLeftEqualityChain(kLargeCount);
        CHECK(chain->kind() == NodeKind::BinaryExpr);
        chain.reset();
        CHECK(chain == nullptr);
    }
    SECTION("a chain of 200,000 operators nested through the right operand") {
        std::unique_ptr<ASTNode> chain = makeRightEqualityChain(kLargeCount);
        chain.reset();
        CHECK(chain == nullptr);
    }
    SECTION("200,000 mixed levels as let, assignment, and module-level const initializers") {
        std::vector<std::unique_ptr<ASTNode>> statements;
        statements.push_back(std::make_unique<LetDecl>(
            SourceLocation{}, "flag", SourceLocation{}, DeclaredType{TypeName::Boolean, false},
            makeMixedOperatorNesting(kLargeCount)));
        statements.push_back(
            std::make_unique<AssignStmt>(SourceLocation{}, "flag",
                                         makeMixedOperatorNesting(kLargeCount)));
        std::shared_ptr<const FunctionDecl> declaration = makeMainWithBody(std::move(statements));
        declaration.reset();
        CHECK(declaration == nullptr);

        ModuleItem constant = std::make_shared<const ConstDecl>(
            SourceLocation{}, "flag", SourceLocation{}, DeclaredType{TypeName::Boolean, false},
            makeMixedOperatorNesting(kLargeCount));
        constant.reset();
        CHECK(constant == nullptr);
    }
}

} // namespace philios::test
