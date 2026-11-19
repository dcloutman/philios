// Unit tests for src/frontend/parser/parser.hpp (plan 0001 §3.4, D11, D27; task 6).
//
// Per §8, the error cases of T-PARSE-1 and T-LEX-10 (and T-PARSE-2, -4, -5) are CLI golden
// tests; this file checks the accepted cases on the AST.
//
// Plan 0002 T2-TYPE-8 (§3.3, §3.4; task 3): each of the nine declarable types parses to the
// expected DeclaredType as a parameter type and as a return type. The T2-TYPE-4 type-position
// syntax errors are CLI golden tests.
//
// Plan 0002 task 5 (§3.3, §3.8): accepted `echo` and `typeof` pseudo-calls, as statements and as
// expressions, parse to EchoCall and TypeofCall nodes located at the keyword. The pseudo-call
// syntax errors (T2-ECHO-4, T2-TYPEOF-4, T2-TYPEOF-6) are CLI golden tests.

#include "frontend/parser/parser.hpp"
#include "frontend/ast/ast.hpp"
#include "support/source_pipeline.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace philios::test {
namespace {

/// One declarable type (plan 0002 §3.4): its source spelling and the expected DeclaredType.
struct DeclarableTypeCase {
    std::string_view spelling;
    DeclaredType expected;
};

/// The nine declarable types of plan 0002 §3.4.
constexpr std::array<DeclarableTypeCase, 9> kDeclarableTypes{{
    {"Integer", DeclaredType{TypeName::Integer, false}},
    {"String", DeclaredType{TypeName::String, false}},
    {"Function", DeclaredType{TypeName::Function, false}},
    {"Object", DeclaredType{TypeName::Object, false}},
    {"null", DeclaredType{TypeName::Null, false}},
    {"?Integer", DeclaredType{TypeName::Integer, true}},
    {"?String", DeclaredType{TypeName::String, true}},
    {"?Function", DeclaredType{TypeName::Function, true}},
    {"?Object", DeclaredType{TypeName::Object, true}},
}};

} // namespace

TEST_CASE("T-PARSE-1 a pass body is accepted") {
    const Program program = parseSource("function main():\n    pass\n");
    REQUIRE(program.size() == 1);
    REQUIRE(program[0] != nullptr);
    const FunctionDecl& main = *program[0];
    CHECK(main.name() == "main");
    CHECK(main.params().empty());
    CHECK_FALSE(main.returnType().has_value());
    REQUIRE(main.body().statements().size() == 1);
    CHECK(main.body().statements()[0]->kind() == NodeKind::PassStmt);
}

TEST_CASE("T-PARSE-1 pass mixed with other statements is accepted") {
    const Program program = parseSource("function main():\n    pass\n    f()\n    pass\n"
                                        "function f(): null:\n    pass\n");
    REQUIRE(program.size() == 2);
    const auto& statements = program[0]->body().statements();
    REQUIRE(statements.size() == 3);
    CHECK(statements[0]->kind() == NodeKind::PassStmt);
    CHECK(statements[1]->kind() == NodeKind::ExprStmt);
    CHECK(statements[2]->kind() == NodeKind::PassStmt);
    CHECK(program[1]->returnType() ==
          std::optional<DeclaredType>{DeclaredType{TypeName::Null, false}});
}

TEST_CASE("T-LEX-10 Integer2, integer, and _if are accepted as function and parameter names") {
    const Program program =
        parseSource("function Integer2(integer: Integer, _if: String): null:\n    pass\n"
                    "function integer(Integer2: Function): null:\n    pass\n"
                    "function _if(x: null): null:\n    pass\n"
                    "function main():\n    pass\n");
    REQUIRE(program.size() == 4);
    CHECK(program[0]->name() == "Integer2");
    REQUIRE(program[0]->params().size() == 2);
    CHECK(program[0]->params()[0]->name() == "integer");
    CHECK(program[0]->params()[0]->type() == DeclaredType{TypeName::Integer, false});
    CHECK(program[0]->params()[1]->name() == "_if");
    CHECK(program[0]->params()[1]->type() == DeclaredType{TypeName::String, false});
    CHECK(program[1]->name() == "integer");
    REQUIRE(program[1]->params().size() == 1);
    CHECK(program[1]->params()[0]->name() == "Integer2");
    CHECK(program[1]->params()[0]->type() == DeclaredType{TypeName::Function, false});
    CHECK(program[2]->name() == "_if");
}

TEST_CASE("Parsed nodes carry source locations") {
    const Program program = parseSource("function main():\n    return f(1, \"s\", null, g)\n");
    REQUIRE(program.size() == 1);
    CHECK(program[0]->location() == SourceLocation{1, 10});
    const auto& statements = program[0]->body().statements();
    REQUIRE(statements.size() == 1);
    REQUIRE(statements[0]->kind() == NodeKind::ReturnStmt);
    CHECK(statements[0]->location() == SourceLocation{2, 5});
    const ASTNode* value = static_cast<const ReturnStmt&>(*statements[0]).value();
    REQUIRE(value != nullptr);
    REQUIRE(value->kind() == NodeKind::CallExpr);
    const auto& call = static_cast<const CallExpr&>(*value);
    CHECK(call.callee().name() == "f");
    CHECK(call.location() == SourceLocation{2, 12});
    REQUIRE(call.arguments().size() == 4);
    CHECK(call.arguments()[0]->kind() == NodeKind::IntegerLiteral);
    CHECK(call.arguments()[1]->kind() == NodeKind::StringLiteral);
    CHECK(call.arguments()[2]->kind() == NodeKind::NullLiteral);
    CHECK(call.arguments()[3]->kind() == NodeKind::Identifier);
}

TEST_CASE("T2-TYPE-8 each declarable type parses as a parameter type") {
    for (const DeclarableTypeCase& typeCase : kDeclarableTypes) {
        INFO("parameter type " << typeCase.spelling);
        const std::string source = "function f(x: " + std::string(typeCase.spelling) +
                                   "): null:\n    pass\nfunction main():\n    pass\n";
        const Program program = parseSource(source);
        REQUIRE(program.size() == 2);
        REQUIRE(program[0]->params().size() == 1);
        CHECK(program[0]->params()[0]->type() == typeCase.expected);
        CHECK(program[0]->returnType() ==
              std::optional<DeclaredType>{DeclaredType{TypeName::Null, false}});
    }
}

TEST_CASE("T2-TYPE-8 each declarable type parses as a return type") {
    for (const DeclarableTypeCase& typeCase : kDeclarableTypes) {
        INFO("return type " << typeCase.spelling);
        const std::string source = "function f(): " + std::string(typeCase.spelling) +
                                   ":\n    pass\nfunction main():\n    pass\n";
        const Program program = parseSource(source);
        REQUIRE(program.size() == 2);
        CHECK(program[0]->params().empty());
        CHECK(program[0]->returnType() == std::optional<DeclaredType>{typeCase.expected});
    }
}

TEST_CASE("T2-TYPE-8 nullable and non-nullable types mix in one parameter list") {
    const Program program = parseSource("function f(a: ?Integer, b: Object, c: ?Function): ?String:"
                                        "\n    pass\nfunction main():\n    pass\n");
    REQUIRE(program.size() == 2);
    const auto& params = program[0]->params();
    REQUIRE(params.size() == 3);
    CHECK(params[0]->type() == DeclaredType{TypeName::Integer, true});
    CHECK(params[1]->type() == DeclaredType{TypeName::Object, false});
    CHECK(params[2]->type() == DeclaredType{TypeName::Function, true});
    CHECK(program[0]->returnType() ==
          std::optional<DeclaredType>{DeclaredType{TypeName::String, true}});
}

TEST_CASE("Plan 0002 pseudo-call statements parse to ExprStmt holding EchoCall or TypeofCall") {
    // T2-ECHO-1 and T2-TYPEOF-7 shapes: `echo("Hello World")` and `typeof(1)` as statements.
    const Program program =
        parseSource("function main(): Integer:\n    echo(\"Hello World\")\n    typeof(1)\n"
                    "    return 1\n");
    REQUIRE(program.size() == 1);
    const auto& statements = program[0]->body().statements();
    REQUIRE(statements.size() == 3);

    REQUIRE(statements[0]->kind() == NodeKind::ExprStmt);
    CHECK(statements[0]->location() == SourceLocation{2, 5});
    const ASTNode& echoNode = static_cast<const ExprStmt&>(*statements[0]).expression();
    REQUIRE(echoNode.kind() == NodeKind::EchoCall);
    const auto& echo = static_cast<const EchoCall&>(echoNode);
    CHECK(echo.location() == SourceLocation{2, 5});
    REQUIRE(echo.argument().kind() == NodeKind::StringLiteral);
    CHECK(echo.argument().location() == SourceLocation{2, 10});
    CHECK(static_cast<const StringLiteral&>(echo.argument()).value() == "Hello World");

    REQUIRE(statements[1]->kind() == NodeKind::ExprStmt);
    const ASTNode& typeofNode = static_cast<const ExprStmt&>(*statements[1]).expression();
    REQUIRE(typeofNode.kind() == NodeKind::TypeofCall);
    const auto& typeOf = static_cast<const TypeofCall&>(typeofNode);
    CHECK(typeOf.location() == SourceLocation{3, 5});
    REQUIRE(typeOf.argument().kind() == NodeKind::IntegerLiteral);
    CHECK(typeOf.argument().location() == SourceLocation{3, 12});

    CHECK(statements[2]->kind() == NodeKind::ReturnStmt);
}

TEST_CASE("Plan 0002 pseudo-calls parse as expressions and nest") {
    // T2-ECHO-3 and T2-TYPEOF-2 shapes: `return typeof(x)`, a pseudo-call as a call argument, and
    // `echo(typeof(echo("x")))`, `typeof(-1)`, `typeof(null)`, and `typeof(id)` as arguments.
    const Program program =
        parseSource("function f(x: Object): String:\n    return typeof(x)\n"
                    "function main():\n    echo(typeof(echo(\"x\")))\n    f(typeof(-1))\n"
                    "    echo(typeof(null))\n    echo(typeof(f))\n");
    REQUIRE(program.size() == 2);

    const auto& fStatements = program[0]->body().statements();
    REQUIRE(fStatements.size() == 1);
    REQUIRE(fStatements[0]->kind() == NodeKind::ReturnStmt);
    const ASTNode* returned = static_cast<const ReturnStmt&>(*fStatements[0]).value();
    REQUIRE(returned != nullptr);
    REQUIRE(returned->kind() == NodeKind::TypeofCall);
    CHECK(returned->location() == SourceLocation{2, 12});
    const ASTNode& returnedArgument = static_cast<const TypeofCall&>(*returned).argument();
    REQUIRE(returnedArgument.kind() == NodeKind::Identifier);
    CHECK(static_cast<const Identifier&>(returnedArgument).name() == "x");

    const auto& mainStatements = program[1]->body().statements();
    REQUIRE(mainStatements.size() == 4);

    // echo(typeof(echo("x"))) on line 4.
    REQUIRE(mainStatements[0]->kind() == NodeKind::ExprStmt);
    const ASTNode& outer = static_cast<const ExprStmt&>(*mainStatements[0]).expression();
    REQUIRE(outer.kind() == NodeKind::EchoCall);
    const ASTNode& middle = static_cast<const EchoCall&>(outer).argument();
    REQUIRE(middle.kind() == NodeKind::TypeofCall);
    CHECK(middle.location() == SourceLocation{4, 10});
    const ASTNode& inner = static_cast<const TypeofCall&>(middle).argument();
    REQUIRE(inner.kind() == NodeKind::EchoCall);
    CHECK(inner.location() == SourceLocation{4, 17});
    CHECK(static_cast<const EchoCall&>(inner).argument().kind() == NodeKind::StringLiteral);

    // f(typeof(-1)) on line 5: a pseudo-call as a call argument.
    REQUIRE(mainStatements[1]->kind() == NodeKind::ExprStmt);
    const ASTNode& callNode = static_cast<const ExprStmt&>(*mainStatements[1]).expression();
    REQUIRE(callNode.kind() == NodeKind::CallExpr);
    const auto& call = static_cast<const CallExpr&>(callNode);
    REQUIRE(call.arguments().size() == 1);
    REQUIRE(call.arguments()[0]->kind() == NodeKind::TypeofCall);
    CHECK(call.arguments()[0]->location() == SourceLocation{5, 7});
    const ASTNode& negative = static_cast<const TypeofCall&>(*call.arguments()[0]).argument();
    REQUIRE(negative.kind() == NodeKind::IntegerLiteral);
    CHECK(static_cast<const IntegerLiteral&>(negative).value() == -1);

    // echo(typeof(null)) and echo(typeof(f)) on lines 6 and 7.
    for (std::size_t index = 2; index < 4; ++index) {
        REQUIRE(mainStatements[index]->kind() == NodeKind::ExprStmt);
        const ASTNode& echoNode = static_cast<const ExprStmt&>(*mainStatements[index]).expression();
        REQUIRE(echoNode.kind() == NodeKind::EchoCall);
        CHECK(static_cast<const EchoCall&>(echoNode).argument().kind() == NodeKind::TypeofCall);
    }
    const ASTNode& nullTypeof =
        static_cast<const EchoCall&>(static_cast<const ExprStmt&>(*mainStatements[2]).expression())
            .argument();
    CHECK(static_cast<const TypeofCall&>(nullTypeof).argument().kind() == NodeKind::NullLiteral);
    const ASTNode& functionTypeof =
        static_cast<const EchoCall&>(static_cast<const ExprStmt&>(*mainStatements[3]).expression())
            .argument();
    CHECK(static_cast<const TypeofCall&>(functionTypeof).argument().kind() ==
          NodeKind::Identifier);
}

} // namespace philios::test
