// Unit tests for src/frontend/parser/parser.hpp (plan 0001 §3.4, D11, D27; task 6).
//
// Per §8, the error cases of T-PARSE-1 and T-LEX-10 (and T-PARSE-2, -4, -5) are CLI golden
// tests; this file checks the accepted cases on the AST.

#include "frontend/parser/parser.hpp"
#include "frontend/ast/ast.hpp"
#include "support/source_pipeline.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <string_view>

namespace philios::test {

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
    CHECK(program[1]->returnType() == std::optional<TypeName>{TypeName::Null});
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
    CHECK(program[0]->params()[0]->type() == TypeName::Integer);
    CHECK(program[0]->params()[1]->name() == "_if");
    CHECK(program[0]->params()[1]->type() == TypeName::String);
    CHECK(program[1]->name() == "integer");
    REQUIRE(program[1]->params().size() == 1);
    CHECK(program[1]->params()[0]->name() == "Integer2");
    CHECK(program[1]->params()[0]->type() == TypeName::Function);
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

} // namespace philios::test
