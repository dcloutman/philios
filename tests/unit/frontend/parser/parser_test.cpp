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
//
// Plan 0003 task 3 (§4.4, §4.11.5, §4.12, §4.14.2, §5.2, §6.1): module items, the new statements,
// the expression levels, the §5.2 syntax errors (class SyntaxError, location, and message where the
// plan gives one), nesting at the 256/257 boundaries, and the parser parts of T3-LEX-1, -3, -5, -6
// and T3-SYM-5. Expected error locations are taken from the lexer's token stream where a token
// is named. T3-NEST-3's no-recursion requirement is checked by the build-asan run.

#include "frontend/parser/parser.hpp"
#include "common/errors.hpp"
#include "frontend/ast/ast.hpp"
#include "frontend/lexer/token.hpp"
#include "support/require_value.hpp"
#include "support/source_pipeline.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
    const FunctionDecl& main = *asFunctionDecl(program[0]);
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
    const auto& statements = asFunctionDecl(program[0])->body().statements();
    REQUIRE(statements.size() == 3);
    CHECK(statements[0]->kind() == NodeKind::PassStmt);
    CHECK(statements[1]->kind() == NodeKind::ExprStmt);
    CHECK(statements[2]->kind() == NodeKind::PassStmt);
    CHECK(asFunctionDecl(program[1])->returnType() ==
          std::optional<DeclaredType>{DeclaredType{TypeName::Null, false}});
}

TEST_CASE("T-LEX-10 Integer2, integer, and _if are accepted as function and parameter names") {
    const Program program =
        parseSource("function Integer2(integer: Integer, _if: String): null:\n    pass\n"
                    "function integer(Integer2: Function): null:\n    pass\n"
                    "function _if(x: null): null:\n    pass\n"
                    "function main():\n    pass\n");
    REQUIRE(program.size() == 4);
    CHECK(asFunctionDecl(program[0])->name() == "Integer2");
    REQUIRE(asFunctionDecl(program[0])->params().size() == 2);
    CHECK(asFunctionDecl(program[0])->params()[0]->name() == "integer");
    CHECK(asFunctionDecl(program[0])->params()[0]->type() ==
          DeclaredType{TypeName::Integer, false});
    CHECK(asFunctionDecl(program[0])->params()[1]->name() == "_if");
    CHECK(asFunctionDecl(program[0])->params()[1]->type() == DeclaredType{TypeName::String, false});
    CHECK(asFunctionDecl(program[1])->name() == "integer");
    REQUIRE(asFunctionDecl(program[1])->params().size() == 1);
    CHECK(asFunctionDecl(program[1])->params()[0]->name() == "Integer2");
    CHECK(asFunctionDecl(program[1])->params()[0]->type() ==
          DeclaredType{TypeName::Function, false});
    CHECK(asFunctionDecl(program[2])->name() == "_if");
}

TEST_CASE("Parsed nodes carry source locations") {
    const Program program = parseSource("function main():\n    return f(1, \"s\", null, g)\n");
    REQUIRE(program.size() == 1);
    CHECK(asFunctionDecl(program[0])->location() == SourceLocation{1, 10});
    const auto& statements = asFunctionDecl(program[0])->body().statements();
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
        REQUIRE(asFunctionDecl(program[0])->params().size() == 1);
        CHECK(asFunctionDecl(program[0])->params()[0]->type() == typeCase.expected);
        CHECK(asFunctionDecl(program[0])->returnType() ==
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
        CHECK(asFunctionDecl(program[0])->params().empty());
        CHECK(asFunctionDecl(program[0])->returnType() ==
              std::optional<DeclaredType>{typeCase.expected});
    }
}

TEST_CASE("T2-TYPE-8 nullable and non-nullable types mix in one parameter list") {
    const Program program = parseSource("function f(a: ?Integer, b: Object, c: ?Function): ?String:"
                                        "\n    pass\nfunction main():\n    pass\n");
    REQUIRE(program.size() == 2);
    const auto& params = asFunctionDecl(program[0])->params();
    REQUIRE(params.size() == 3);
    CHECK(params[0]->type() == DeclaredType{TypeName::Integer, true});
    CHECK(params[1]->type() == DeclaredType{TypeName::Object, false});
    CHECK(params[2]->type() == DeclaredType{TypeName::Function, true});
    CHECK(asFunctionDecl(program[0])->returnType() ==
          std::optional<DeclaredType>{DeclaredType{TypeName::String, true}});
}

TEST_CASE("Plan 0002 pseudo-call statements parse to ExprStmt holding EchoCall or TypeofCall") {
    // T2-ECHO-1 and T2-TYPEOF-7 shapes: `echo("Hello World")` and `typeof(1)` as statements.
    const Program program =
        parseSource("function main(): Integer:\n    echo(\"Hello World\")\n    typeof(1)\n"
                    "    return 1\n");
    REQUIRE(program.size() == 1);
    const auto& statements = asFunctionDecl(program[0])->body().statements();
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

    const auto& fStatements = asFunctionDecl(program[0])->body().statements();
    REQUIRE(fStatements.size() == 1);
    REQUIRE(fStatements[0]->kind() == NodeKind::ReturnStmt);
    const ASTNode* returned = static_cast<const ReturnStmt&>(*fStatements[0]).value();
    REQUIRE(returned != nullptr);
    REQUIRE(returned->kind() == NodeKind::TypeofCall);
    CHECK(returned->location() == SourceLocation{2, 12});
    const ASTNode& returnedArgument = static_cast<const TypeofCall&>(*returned).argument();
    REQUIRE(returnedArgument.kind() == NodeKind::Identifier);
    CHECK(static_cast<const Identifier&>(returnedArgument).name() == "x");

    const auto& mainStatements = asFunctionDecl(program[1])->body().statements();
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

namespace {

/// A SyntaxError's location and message (plan 0003 §5.2).
struct SyntaxFailure {
    SourceLocation location{};
    std::string message{};
};

/// The SyntaxError raised by parsing @p source, or no value if parsing succeeds. Any other
/// exception (a LexicalError or an InternalError) propagates and fails the test
/// case without aborting the test binary.
[[nodiscard]] std::optional<SyntaxFailure> syntaxFailure(std::string_view source) {
    try {
        [[maybe_unused]] const Program program = parseSource(source);
    } catch (const SyntaxError& error) {
        return SyntaxFailure{error.location(), error.what()};
    }
    return std::nullopt;
}

/// The location of the @p occurrence-th (1-based) token of kind @p kind in @p source's token
/// stream. Fails the test if there is no such token.
[[nodiscard]] SourceLocation tokenLocation(std::string_view source, TokenKind kind,
                                           std::size_t occurrence) {
    const std::vector<Token> tokens = lexSource(source);
    std::size_t seen = 0;
    for (const Token& token : tokens) {
        if (token.kind == kind) {
            ++seen;
            if (seen == occurrence) {
                return token.location;
            }
        }
    }
    failMissingValue();
}

/// @p source for an INFO message: itself if short, otherwise only its size.
[[nodiscard]] std::string describeSource(std::string_view source) {
    constexpr std::size_t kMaxShown = 200;
    if (source.size() <= kMaxShown) {
        return std::string(source);
    }
    return "<" + std::to_string(source.size()) + " bytes>";
}

/// Requires that parsing @p source fails with a SyntaxError at the @p occurrence-th token of kind
/// @p kind, and returns the failure.
SyntaxFailure checkSyntaxErrorAtToken(std::string_view source, TokenKind kind,
                                      std::size_t occurrence) {
    INFO("source: " << describeSource(source));
    const SyntaxFailure failure = requireValue(syntaxFailure(source));
    CHECK(failure.location == tokenLocation(source, kind, occurrence));
    return failure;
}

/// @p statement as the only statement of `main` (line 2, column 5).
[[nodiscard]] std::string inMain(std::string_view statement) {
    return "function main():\n    " + std::string(statement) + "\n";
}

/// @p text repeated @p count times.
[[nodiscard]] std::string repeated(std::string_view text, std::size_t count) {
    std::string result;
    result.reserve(text.size() * count);
    for (std::size_t index = 0; index < count; ++index) {
        result += text;
    }
    return result;
}

/// @p node as a @p Node after requiring that its kind() is @p kind.
template <typename Node>
[[nodiscard]] const Node& nodeAs(const ASTNode& node, NodeKind kind) {
    REQUIRE(node.kind() == kind);
    return static_cast<const Node&>(node);
}

/// @p node as a @p Node after requiring that it is non-null and its kind() is @p kind.
template <typename Node>
[[nodiscard]] const Node& nodeAs(const ASTNode* node, NodeKind kind) {
    REQUIRE(node != nullptr);
    return nodeAs<Node>(*node, kind);
}

/// The statements of `main`, the only item of @p program.
[[nodiscard]] const std::vector<std::unique_ptr<ASTNode>>& mainStatements(const Program& program) {
    REQUIRE(program.size() == 1);
    const std::shared_ptr<const FunctionDecl> main = asFunctionDecl(program[0]);
    REQUIRE(main != nullptr);
    return main->body().statements();
}

/// The value of the single `return` statement of `main`, the only item of @p program.
[[nodiscard]] const ASTNode& returnedValue(const Program& program) {
    const std::vector<std::unique_ptr<ASTNode>>& statements = mainStatements(program);
    REQUIRE(statements.size() == 1);
    const ASTNode* value = nodeAs<ReturnStmt>(statements[0].get(), NodeKind::ReturnStmt).value();
    REQUIRE(value != nullptr);
    return *value;
}

/// Checks that @p node is an Identifier named @p name.
void checkIdentifier(const ASTNode& node, std::string_view name) {
    CHECK(nodeAs<Identifier>(node, NodeKind::Identifier).name() == name);
}

/// A statement inside `main` whose syntax error is at the @p occurrence-th token of kind @p kind.
struct StatementErrorCase {
    std::string_view statement;
    TokenKind kind;
    std::size_t occurrence;
};

/// The 7 reserved words added by plan 0003 §4.2.2.
constexpr std::array<std::string_view, 7> kPlan0003ReservedWords{
    "let", "const", "true", "false", "symbol", "Boolean", "Symbol"};

/// T3-LEX-1 names (plan 0003 §4.2.1).
constexpr std::array<std::string_view, 7> kValidNames{"count", "_count", "__count", "count_2",
                                                      "a1b2",  "A",      "_a"};

/// T3-LEX-3 near words: not reserved, so valid names.
constexpr std::array<std::string_view, 6> kNearWords{"Let",     "TRUE", "symbols",
                                                     "boolean", "letx", "_true"};

/// The four declarable types added by plan 0003 §4.3.
constexpr std::array<DeclarableTypeCase, 4> kPlan0003Types{{
    {"Boolean", DeclaredType{TypeName::Boolean, false}},
    {"Symbol", DeclaredType{TypeName::Symbol, false}},
    {"?Boolean", DeclaredType{TypeName::Boolean, true}},
    {"?Symbol", DeclaredType{TypeName::Symbol, true}},
}};

} // namespace

TEST_CASE("T3-ARCH-1 module items are ConstDecl, FunctionDecl, SymbolDecl, FunctionDecl in order") {
    const Program program = parseSource("const a: Integer = 1\nfunction f(): Integer:\n"
                                        "    return 1\nsymbol s\nfunction main():\n    pass\n");
    REQUIRE(program.size() == 4);
    for (const ModuleItem& item : program) {
        REQUIRE(item != nullptr);
    }
    CHECK(program[0]->kind() == NodeKind::ConstDecl);
    CHECK(program[1]->kind() == NodeKind::FunctionDecl);
    CHECK(program[2]->kind() == NodeKind::SymbolDecl);
    CHECK(program[3]->kind() == NodeKind::FunctionDecl);

    CHECK(asFunctionDecl(program[0]) == nullptr);
    CHECK(asFunctionDecl(program[2]) == nullptr);
    const std::shared_ptr<const FunctionDecl> f = asFunctionDecl(program[1]);
    REQUIRE(f != nullptr);
    CHECK(f->name() == "f");
    CHECK(f.get() == program[1].get());
    const std::shared_ptr<const FunctionDecl> main = asFunctionDecl(program[3]);
    REQUIRE(main != nullptr);
    CHECK(main->name() == "main");
    CHECK(main.get() == program[3].get());

    const auto& constant = nodeAs<ConstDecl>(*program[0], NodeKind::ConstDecl);
    CHECK(constant.location() == SourceLocation{1, 1});
    CHECK(constant.name() == "a");
    CHECK(constant.nameLocation() == SourceLocation{1, 7});
    CHECK(constant.type() == DeclaredType{TypeName::Integer, false});
    const auto& initializer =
        nodeAs<IntegerLiteral>(constant.initializer(), NodeKind::IntegerLiteral);
    CHECK(initializer.value() == 1);
    CHECK(initializer.location() == SourceLocation{1, 20});

    const auto& symbol = nodeAs<SymbolDecl>(*program[2], NodeKind::SymbolDecl);
    CHECK(symbol.name() == "s");
    CHECK(symbol.location() == SourceLocation{4, 8});

    CHECK_THROWS_AS(asFunctionDecl(ModuleItem{}), InternalError);
}

TEST_CASE("T3-PARSE-1 let at module level is a syntax error at let") {
    SECTION("first item") {
        const SyntaxFailure failure = requireValue(
            syntaxFailure("let x: Integer = 1\nfunction main():\n    pass\n"));
        CHECK(failure.location == SourceLocation{1, 1});
    }
    SECTION("after a function") {
        const SyntaxFailure failure = requireValue(
            syntaxFailure("function main():\n    pass\nlet x: Integer = 1\n"));
        CHECK(failure.location == SourceLocation{3, 1});
    }
    SECTION("return at module level keeps its location") {
        const SyntaxFailure failure =
            requireValue(syntaxFailure("return 1\nfunction main():\n    pass\n"));
        CHECK(failure.location == SourceLocation{1, 1});
    }
}

TEST_CASE("T3-PARSE-2 an = inside an expression or a second = is an error at that =") {
    constexpr std::array<StatementErrorCase, 11> cases{{
        {"echo(a = \"x\")", TokenKind::Assign, 1},
        {"echo(a = 1)", TokenKind::Assign, 1},
        {"f(a = 1)", TokenKind::Assign, 1},
        {"return a = 1", TokenKind::Assign, 1},
        {"a = b = 1", TokenKind::Assign, 2},
        {"let x: Integer = y = 1", TokenKind::Assign, 2},
        {"a = = b", TokenKind::Assign, 2},
        {"return a === b", TokenKind::Assign, 1},
        {"return a !== b", TokenKind::Assign, 1},
        {"x = a === b", TokenKind::Assign, 2},
        {"x = a !== b", TokenKind::Assign, 2},
    }};
    for (const StatementErrorCase& errorCase : cases) {
        [[maybe_unused]] const SyntaxFailure failure = checkSyntaxErrorAtToken(
            inMain(errorCase.statement), errorCase.kind, errorCase.occurrence);
    }
}

TEST_CASE("T3-PARSE-2 statement a === b and a !== b are errors at the second =") {
    // Developer decision (2026-11-22): the second `=` row of §5.2 wins at statement start.
    SECTION("a === b") {
        const SyntaxFailure failure = requireValue(syntaxFailure(inMain("a === b")));
        CHECK(failure.location == SourceLocation{2, 9});
    }
    SECTION("a !== b") {
        const SyntaxFailure failure = requireValue(syntaxFailure(inMain("a !== b")));
        CHECK(failure.location == SourceLocation{2, 9});
    }
    SECTION("a == b without a following = stays an error at ==") {
        const SyntaxFailure failure = requireValue(syntaxFailure(inMain("a == b")));
        CHECK(failure.location == SourceLocation{2, 7});
    }
    SECTION("a != b without a following = stays an error at !=") {
        const SyntaxFailure failure = requireValue(syntaxFailure(inMain("a != b")));
        CHECK(failure.location == SourceLocation{2, 7});
    }
}

TEST_CASE("T3-PARSE-3 operator expressions, names, and literals alone are not statements") {
    constexpr std::array<StatementErrorCase, 9> cases{{
        {"a == b", TokenKind::EqualEqual, 1},
        {"a != b", TokenKind::BangEqual, 1},
        {"!a", TokenKind::Bang, 1},
        {"a", TokenKind::Newline, 2},
        {"true", TokenKind::KeywordTrue, 1},
        {"false", TokenKind::KeywordFalse, 1},
        {"1", TokenKind::IntegerLiteral, 1},
        {"\"s\"", TokenKind::StringLiteral, 1},
        {"null", TokenKind::KeywordNull, 1},
    }};
    for (const StatementErrorCase& errorCase : cases) {
        [[maybe_unused]] const SyntaxFailure failure = checkSyntaxErrorAtToken(
            inMain(errorCase.statement), errorCase.kind, errorCase.occurrence);
    }
}

TEST_CASE("T3-PARSE-4 missing declaration parts and operands are errors where expected") {
    SECTION("in a function body") {
        // Token 1 of Colon, RightParen, and Newline is in the `function main():` header.
        constexpr std::array<StatementErrorCase, 12> cases{{
            {"let x Integer = 1", TokenKind::TypeInteger, 1},
            {"let : Integer", TokenKind::Colon, 2},
            {"let x: = 1", TokenKind::Assign, 1},
            {"let x: Integer =", TokenKind::Newline, 2},
            {"const x: Integer =", TokenKind::Newline, 2},
            {"symbol", TokenKind::Newline, 2},
            {"symbol x = 1", TokenKind::Assign, 1},
            {"symbol x: Symbol", TokenKind::Colon, 2},
            {"return a ==", TokenKind::Newline, 2},
            {"return a !=", TokenKind::Newline, 2},
            {"return !", TokenKind::Newline, 2},
            {"f(!)", TokenKind::RightParen, 2},
        }};
        for (const StatementErrorCase& errorCase : cases) {
            [[maybe_unused]] const SyntaxFailure failure = checkSyntaxErrorAtToken(
                inMain(errorCase.statement), errorCase.kind, errorCase.occurrence);
        }
    }
    SECTION("operand missing at end of file") {
        [[maybe_unused]] const SyntaxFailure failure =
            checkSyntaxErrorAtToken("function main():\n    return !", TokenKind::Newline, 2);
    }
    SECTION("at module level") {
        [[maybe_unused]] const SyntaxFailure constFailure = checkSyntaxErrorAtToken(
            "const x: Integer =\nfunction main():\n    pass\n", TokenKind::Newline, 1);
        [[maybe_unused]] const SyntaxFailure symbolFailure = checkSyntaxErrorAtToken(
            "symbol\nfunction main():\n    pass\n", TokenKind::Newline, 1);
        [[maybe_unused]] const SyntaxFailure symbolInitFailure = checkSyntaxErrorAtToken(
            "symbol x = 1\nfunction main():\n    pass\n", TokenKind::Assign, 1);
    }
}

TEST_CASE("T3-PARSE-5 operator expressions have the specified AST shape") {
    SECTION("!a == b is Binary(Equal, Unary(Not, a), b)") {
        const Program program = parseSource(inMain("return !a == b"));
        const auto& binary = nodeAs<BinaryExpr>(returnedValue(program), NodeKind::BinaryExpr);
        CHECK(binary.op() == BinaryOperator::Equal);
        CHECK(binary.location() == SourceLocation{2, 12});
        CHECK(binary.operatorLocation() == SourceLocation{2, 15});
        const auto& unary = nodeAs<UnaryExpr>(binary.left(), NodeKind::UnaryExpr);
        CHECK(unary.op() == UnaryOperator::Not);
        CHECK(unary.location() == SourceLocation{2, 12});
        checkIdentifier(unary.operand(), "a");
        CHECK(unary.operand().location() == SourceLocation{2, 13});
        checkIdentifier(binary.right(), "b");
        CHECK(binary.right().location() == SourceLocation{2, 18});
    }
    SECTION("a == b != c is Binary(NotEqual, Binary(Equal, a, b), c)") {
        const Program program = parseSource(inMain("return a == b != c"));
        const auto& outer = nodeAs<BinaryExpr>(returnedValue(program), NodeKind::BinaryExpr);
        CHECK(outer.op() == BinaryOperator::NotEqual);
        CHECK(outer.location() == SourceLocation{2, 12});
        CHECK(outer.operatorLocation() == SourceLocation{2, 19});
        const auto& inner = nodeAs<BinaryExpr>(outer.left(), NodeKind::BinaryExpr);
        CHECK(inner.op() == BinaryOperator::Equal);
        CHECK(inner.operatorLocation() == SourceLocation{2, 14});
        checkIdentifier(inner.left(), "a");
        checkIdentifier(inner.right(), "b");
        checkIdentifier(outer.right(), "c");
    }
    SECTION("a == b == c is left-associative") {
        const Program program = parseSource(inMain("return a == b == c"));
        const auto& outer = nodeAs<BinaryExpr>(returnedValue(program), NodeKind::BinaryExpr);
        CHECK(outer.op() == BinaryOperator::Equal);
        const auto& inner = nodeAs<BinaryExpr>(outer.left(), NodeKind::BinaryExpr);
        CHECK(inner.op() == BinaryOperator::Equal);
        checkIdentifier(inner.left(), "a");
        checkIdentifier(inner.right(), "b");
        checkIdentifier(outer.right(), "c");
    }
    SECTION("a == !b is Binary(Equal, a, Unary(Not, b))") {
        const Program program = parseSource(inMain("return a == !b"));
        const auto& binary = nodeAs<BinaryExpr>(returnedValue(program), NodeKind::BinaryExpr);
        CHECK(binary.op() == BinaryOperator::Equal);
        checkIdentifier(binary.left(), "a");
        const auto& unary = nodeAs<UnaryExpr>(binary.right(), NodeKind::UnaryExpr);
        CHECK(unary.op() == UnaryOperator::Not);
        CHECK(unary.location() == SourceLocation{2, 17});
        checkIdentifier(unary.operand(), "b");
    }
    SECTION("!f(!a) is Unary(Not, Call(f, Unary(Not, a)))") {
        const Program program = parseSource(inMain("return !f(!a)"));
        const auto& outer = nodeAs<UnaryExpr>(returnedValue(program), NodeKind::UnaryExpr);
        CHECK(outer.op() == UnaryOperator::Not);
        const auto& call = nodeAs<CallExpr>(outer.operand(), NodeKind::CallExpr);
        CHECK(call.callee().name() == "f");
        REQUIRE(call.arguments().size() == 1);
        const auto& inner = nodeAs<UnaryExpr>(call.arguments()[0].get(), NodeKind::UnaryExpr);
        CHECK(inner.op() == UnaryOperator::Not);
        CHECK(inner.location() == SourceLocation{2, 15});
        checkIdentifier(inner.operand(), "a");
    }
    SECTION("true and false are BooleanLiteral operands") {
        const Program program = parseSource(inMain("return !true != false"));
        const auto& binary = nodeAs<BinaryExpr>(returnedValue(program), NodeKind::BinaryExpr);
        CHECK(binary.op() == BinaryOperator::NotEqual);
        const auto& unary = nodeAs<UnaryExpr>(binary.left(), NodeKind::UnaryExpr);
        const auto& trueLiteral = nodeAs<BooleanLiteral>(unary.operand(), NodeKind::BooleanLiteral);
        CHECK(trueLiteral.value());
        CHECK(trueLiteral.location() == SourceLocation{2, 13});
        const auto& falseLiteral = nodeAs<BooleanLiteral>(binary.right(), NodeKind::BooleanLiteral);
        CHECK_FALSE(falseLiteral.value());
        CHECK(falseLiteral.location() == SourceLocation{2, 21});
    }
}

TEST_CASE("Plan 0003 function-scope declarations and assignment parse with their parts") {
    const Program program = parseSource("function main():\n    let x: Integer = 1\n"
                                        "    const c: ?String\n    symbol s\n    x = 2\n"
                                        "    let b: Boolean = a == !c\n");
    const std::vector<std::unique_ptr<ASTNode>>& statements = mainStatements(program);
    REQUIRE(statements.size() == 5);

    const auto& let = nodeAs<LetDecl>(statements[0].get(), NodeKind::LetDecl);
    CHECK(let.location() == SourceLocation{2, 5});
    CHECK(let.name() == "x");
    CHECK(let.nameLocation() == SourceLocation{2, 9});
    CHECK(let.type() == DeclaredType{TypeName::Integer, false});
    const auto& letValue = nodeAs<IntegerLiteral>(let.initializer(), NodeKind::IntegerLiteral);
    CHECK(letValue.value() == 1);
    CHECK(letValue.location() == SourceLocation{2, 22});

    const auto& constant = nodeAs<ConstDecl>(statements[1].get(), NodeKind::ConstDecl);
    CHECK(constant.location() == SourceLocation{3, 5});
    CHECK(constant.name() == "c");
    CHECK(constant.nameLocation() == SourceLocation{3, 11});
    CHECK(constant.type() == DeclaredType{TypeName::String, true});
    CHECK(constant.initializer() == nullptr);

    const auto& symbol = nodeAs<SymbolDecl>(statements[2].get(), NodeKind::SymbolDecl);
    CHECK(symbol.name() == "s");
    CHECK(symbol.location() == SourceLocation{4, 12});

    const auto& assignment = nodeAs<AssignStmt>(statements[3].get(), NodeKind::AssignStmt);
    CHECK(assignment.location() == SourceLocation{5, 5});
    CHECK(assignment.name() == "x");
    const auto& assigned = nodeAs<IntegerLiteral>(assignment.value(), NodeKind::IntegerLiteral);
    CHECK(assigned.value() == 2);
    CHECK(assigned.location() == SourceLocation{5, 9});

    const auto& booleanLet = nodeAs<LetDecl>(statements[4].get(), NodeKind::LetDecl);
    CHECK(booleanLet.type() == DeclaredType{TypeName::Boolean, false});
    const auto& equality = nodeAs<BinaryExpr>(booleanLet.initializer(), NodeKind::BinaryExpr);
    CHECK(equality.op() == BinaryOperator::Equal);
    checkIdentifier(equality.left(), "a");
    CHECK(nodeAs<UnaryExpr>(equality.right(), NodeKind::UnaryExpr).op() == UnaryOperator::Not);
}

TEST_CASE("T3-PARSE-6 Boolean and Symbol types parse; malformed type annotations fail") {
    SECTION("as parameter, return, let, and const types") {
        for (const DeclarableTypeCase& typeCase : kPlan0003Types) {
            INFO("type " << typeCase.spelling);
            const std::string spelling(typeCase.spelling);
            const std::string source = "function f(p: " + spelling + "): " + spelling +
                                       ":\n    let v: " + spelling + " = p\n    const c: " +
                                       spelling + " = p\n    return p\n";
            const Program program = parseSource(source);
            REQUIRE(program.size() == 1);
            const std::shared_ptr<const FunctionDecl> f = asFunctionDecl(program[0]);
            REQUIRE(f != nullptr);
            REQUIRE(f->params().size() == 1);
            CHECK(f->params()[0]->type() == typeCase.expected);
            CHECK(f->returnType() == std::optional<DeclaredType>{typeCase.expected});
            const std::vector<std::unique_ptr<ASTNode>>& statements = f->body().statements();
            REQUIRE(statements.size() == 3);
            CHECK(nodeAs<LetDecl>(statements[0].get(), NodeKind::LetDecl).type() ==
                  typeCase.expected);
            CHECK(nodeAs<ConstDecl>(statements[1].get(), NodeKind::ConstDecl).type() ==
                  typeCase.expected);
        }
    }
    SECTION("nullable let declarations without an initializer") {
        const Program program =
            parseSource("function main():\n    let x: ?Boolean\n    let s: ?Symbol\n");
        const std::vector<std::unique_ptr<ASTNode>>& statements = mainStatements(program);
        REQUIRE(statements.size() == 2);
        const auto& x = nodeAs<LetDecl>(statements[0].get(), NodeKind::LetDecl);
        CHECK(x.type() == DeclaredType{TypeName::Boolean, true});
        CHECK(x.initializer() == nullptr);
        const auto& s = nodeAs<LetDecl>(statements[1].get(), NodeKind::LetDecl);
        CHECK(s.type() == DeclaredType{TypeName::Symbol, true});
        CHECK(s.initializer() == nullptr);
    }
    SECTION("module-level const types") {
        const Program program =
            parseSource("const b: Boolean = true\nconst n: ?Symbol\nfunction main():\n    pass\n");
        REQUIRE(program.size() == 3);
        CHECK(nodeAs<ConstDecl>(*program[0], NodeKind::ConstDecl).type() ==
              DeclaredType{TypeName::Boolean, false});
        CHECK(nodeAs<ConstDecl>(*program[1], NodeKind::ConstDecl).type() ==
              DeclaredType{TypeName::Symbol, true});
    }
    SECTION("malformed let and const types") {
        constexpr std::array<StatementErrorCase, 5> cases{{
            {"let x: ? Boolean", TokenKind::Question, 1},
            {"let x: ?null", TokenKind::KeywordNull, 1},
            {"let x: Float = 1", TokenKind::TypeFloat, 1},
            {"const x: ? Symbol", TokenKind::Question, 1},
            {"const x: ?null", TokenKind::KeywordNull, 1},
        }};
        for (const StatementErrorCase& errorCase : cases) {
            [[maybe_unused]] const SyntaxFailure failure = checkSyntaxErrorAtToken(
                inMain(errorCase.statement), errorCase.kind, errorCase.occurrence);
        }
    }
}

TEST_CASE("T3-PARSE-7 f() = 1 fails at = and 1 = x fails at 1") {
    [[maybe_unused]] const SyntaxFailure callTarget =
        checkSyntaxErrorAtToken(inMain("f() = 1"), TokenKind::Assign, 1);
    [[maybe_unused]] const SyntaxFailure literalTarget =
        checkSyntaxErrorAtToken(inMain("1 = x"), TokenKind::IntegerLiteral, 1);
}

TEST_CASE("T3-PARSE-8 echo(true) and echo(false) fail at the argument; typeof(true) parses") {
    SECTION("echo literal arguments") {
        const SyntaxFailure trueFailure =
            checkSyntaxErrorAtToken(inMain("echo(true)"), TokenKind::KeywordTrue, 1);
        CHECK(trueFailure.location == SourceLocation{2, 10});
        const SyntaxFailure falseFailure =
            checkSyntaxErrorAtToken(inMain("echo(false)"), TokenKind::KeywordFalse, 1);
        CHECK(falseFailure.location == SourceLocation{2, 10});
    }
    SECTION("typeof(true) as a statement") {
        const Program program = parseSource(inMain("typeof(true)"));
        const std::vector<std::unique_ptr<ASTNode>>& statements = mainStatements(program);
        REQUIRE(statements.size() == 1);
        const auto& statement = nodeAs<ExprStmt>(statements[0].get(), NodeKind::ExprStmt);
        const auto& typeOf = nodeAs<TypeofCall>(statement.expression(), NodeKind::TypeofCall);
        const auto& literal = nodeAs<BooleanLiteral>(typeOf.argument(), NodeKind::BooleanLiteral);
        CHECK(literal.value());
        CHECK(literal.location() == SourceLocation{2, 12});
    }
    SECTION("typeof(false) as a returned expression") {
        const Program program = parseSource(inMain("return typeof(false)"));
        const auto& typeOf = nodeAs<TypeofCall>(returnedValue(program), NodeKind::TypeofCall);
        CHECK_FALSE(
            nodeAs<BooleanLiteral>(typeOf.argument(), NodeKind::BooleanLiteral).value());
    }
    SECTION("echo of a non-literal Boolean expression is not a syntax error") {
        const Program program = parseSource(inMain("echo(!true)"));
        const std::vector<std::unique_ptr<ASTNode>>& statements = mainStatements(program);
        REQUIRE(statements.size() == 1);
        const auto& statement = nodeAs<ExprStmt>(statements[0].get(), NodeKind::ExprStmt);
        const auto& echo = nodeAs<EchoCall>(statement.expression(), NodeKind::EchoCall);
        CHECK(echo.argument().kind() == NodeKind::UnaryExpr);
    }
}

TEST_CASE("T3-PARSE-9 module level accepts const and symbol mixed with functions in any order") {
    const Program program =
        parseSource("symbol s\nconst c: ?String\nfunction main():\n    pass\n"
                    "const d: Boolean = true\nsymbol t\nfunction f():\n    pass\n"
                    "const e: Symbol = s\n");
    REQUIRE(program.size() == 7);
    constexpr std::array<NodeKind, 7> expectedKinds{
        NodeKind::SymbolDecl, NodeKind::ConstDecl, NodeKind::FunctionDecl, NodeKind::ConstDecl,
        NodeKind::SymbolDecl, NodeKind::FunctionDecl, NodeKind::ConstDecl};
    for (std::size_t index = 0; index < expectedKinds.size(); ++index) {
        INFO("item " << index);
        REQUIRE(program[index] != nullptr);
        CHECK(program[index]->kind() == expectedKinds[index]);
    }
    CHECK(nodeAs<SymbolDecl>(*program[0], NodeKind::SymbolDecl).name() == "s");
    CHECK(nodeAs<ConstDecl>(*program[1], NodeKind::ConstDecl).initializer() == nullptr);
    const auto& d = nodeAs<ConstDecl>(*program[3], NodeKind::ConstDecl);
    CHECK(d.name() == "d");
    CHECK(nodeAs<BooleanLiteral>(d.initializer(), NodeKind::BooleanLiteral).value());
    CHECK(nodeAs<SymbolDecl>(*program[4], NodeKind::SymbolDecl).name() == "t");
    const std::shared_ptr<const FunctionDecl> f = asFunctionDecl(program[5]);
    REQUIRE(f != nullptr);
    CHECK(f->name() == "f");
    const auto& e = nodeAs<ConstDecl>(*program[6], NodeKind::ConstDecl);
    CHECK(e.location() == SourceLocation{9, 1});
    CHECK(nodeAs<Identifier>(e.initializer(), NodeKind::Identifier).name() == "s");
}

TEST_CASE("T3-PARSE-10 a ! directly followed by ! is an error at the second !") {
    SECTION("in a function body") {
        constexpr std::array<std::string_view, 5> statements{
            "return !!a", "return ! !a", "let b: Boolean = !!true", "f(!!a)",
            "return a == !!b"};
        for (const std::string_view statement : statements) {
            const SyntaxFailure failure =
                checkSyntaxErrorAtToken(inMain(statement), TokenKind::Bang, 2);
            CHECK(failure.message == "repeated ! is not allowed");
        }
    }
    SECTION("in a module-level initializer") {
        const SyntaxFailure failure = checkSyntaxErrorAtToken(
            "const b: Boolean = !!true\nfunction main():\n    pass\n", TokenKind::Bang, 2);
        CHECK(failure.message == "repeated ! is not allowed");
    }
}

TEST_CASE("T3-LEX-1 valid names and near words are accepted in every name position") {
    std::vector<std::string_view> names(kValidNames.begin(), kValidNames.end());
    names.insert(names.end(), kNearWords.begin(), kNearWords.end());
    for (const std::string_view name : names) {
        INFO("name " << name);
        const std::string n(name);
        const Program program =
            parseSource("const " + n + ": Integer = 1\nsymbol " + n + "\nfunction " + n + "(" +
                        n + ": Integer):\n    let " + n + ": Integer = 1\n    const " + n +
                        ": Integer = 1\n    symbol " + n + "\n    " + n + " = 2\n");
        REQUIRE(program.size() == 3);
        CHECK(nodeAs<ConstDecl>(*program[0], NodeKind::ConstDecl).name() == name);
        CHECK(nodeAs<SymbolDecl>(*program[1], NodeKind::SymbolDecl).name() == name);
        const std::shared_ptr<const FunctionDecl> declaration = asFunctionDecl(program[2]);
        REQUIRE(declaration != nullptr);
        CHECK(declaration->name() == name);
        REQUIRE(declaration->params().size() == 1);
        CHECK(declaration->params()[0]->name() == name);
        const std::vector<std::unique_ptr<ASTNode>>& statements = declaration->body().statements();
        REQUIRE(statements.size() == 4);
        CHECK(nodeAs<LetDecl>(statements[0].get(), NodeKind::LetDecl).name() == name);
        CHECK(nodeAs<ConstDecl>(statements[1].get(), NodeKind::ConstDecl).name() == name);
        CHECK(nodeAs<SymbolDecl>(statements[2].get(), NodeKind::SymbolDecl).name() == name);
        CHECK(nodeAs<AssignStmt>(statements[3].get(), NodeKind::AssignStmt).name() == name);
    }
}

TEST_CASE("T3-LEX-3 a plan 0003 reserved word in a name position is a syntax error at the word") {
    for (const std::string_view word : kPlan0003ReservedWords) {
        INFO("word " << word);
        const std::string w(word);
        struct PositionCase {
            std::string source;
            SourceLocation expected;
        };
        const std::array<PositionCase, 7> positions{{
            {"function " + w + "():\n    pass\n", SourceLocation{1, 10}},
            {"function f(" + w + ": Integer):\n    pass\n", SourceLocation{1, 12}},
            {inMain("let " + w + ": Integer = 1"), SourceLocation{2, 9}},
            {inMain("const " + w + ": Integer = 1"), SourceLocation{2, 11}},
            {inMain("symbol " + w), SourceLocation{2, 12}},
            {"const " + w + ": Integer = 1\nfunction main():\n    pass\n", SourceLocation{1, 7}},
            {"symbol " + w + "\nfunction main():\n    pass\n", SourceLocation{1, 8}},
        }};
        for (const PositionCase& position : positions) {
            INFO("source: " << position.source);
            const SyntaxFailure failure = requireValue(syntaxFailure(position.source));
            CHECK(failure.location == position.expected);
        }
    }
}

TEST_CASE("T3-LEX-5 negative literals parse at the new expression starts; x = - 1 fails at -") {
    SECTION("x = - 1") {
        [[maybe_unused]] const SyntaxFailure failure =
            checkSyntaxErrorAtToken(inMain("x = - 1"), TokenKind::Minus, 1);
    }
    SECTION("accepted negative literals") {
        const Program program =
            parseSource("function main():\n    let x: Integer = -1\n"
                        "    x = -9223372036854775808\n    let b: Boolean = x == -1\n"
                        "    b = x != -1\n    b = !-1\n");
        const std::vector<std::unique_ptr<ASTNode>>& statements = mainStatements(program);
        REQUIRE(statements.size() == 5);
        const auto& let = nodeAs<LetDecl>(statements[0].get(), NodeKind::LetDecl);
        CHECK(nodeAs<IntegerLiteral>(let.initializer(), NodeKind::IntegerLiteral).value() == -1);
        const auto& minimum = nodeAs<AssignStmt>(statements[1].get(), NodeKind::AssignStmt);
        CHECK(nodeAs<IntegerLiteral>(minimum.value(), NodeKind::IntegerLiteral).value() ==
              std::numeric_limits<std::int64_t>::min());
        const auto& equal = nodeAs<BinaryExpr>(
            nodeAs<LetDecl>(statements[2].get(), NodeKind::LetDecl).initializer(),
            NodeKind::BinaryExpr);
        CHECK(nodeAs<IntegerLiteral>(equal.right(), NodeKind::IntegerLiteral).value() == -1);
        const auto& notEqual = nodeAs<BinaryExpr>(
            nodeAs<AssignStmt>(statements[3].get(), NodeKind::AssignStmt).value(),
            NodeKind::BinaryExpr);
        CHECK(notEqual.op() == BinaryOperator::NotEqual);
        CHECK(nodeAs<IntegerLiteral>(notEqual.right(), NodeKind::IntegerLiteral).value() == -1);
        const auto& negation = nodeAs<UnaryExpr>(
            nodeAs<AssignStmt>(statements[4].get(), NodeKind::AssignStmt).value(),
            NodeKind::UnaryExpr);
        CHECK(nodeAs<IntegerLiteral>(negation.operand(), NodeKind::IntegerLiteral).value() == -1);
    }
}

TEST_CASE("T3-LEX-6 ; continuation after =, after ==, and between let x: and the type parses") {
    const Program program =
        parseSource("function main():\n    let x: Integer =;\n        -1\n"
                    "    let y:;\n        Integer = 1\n    return x ==;\n        y\n");
    const std::vector<std::unique_ptr<ASTNode>>& statements = mainStatements(program);
    REQUIRE(statements.size() == 3);
    const auto& x = nodeAs<LetDecl>(statements[0].get(), NodeKind::LetDecl);
    CHECK(x.name() == "x");
    const auto& xValue = nodeAs<IntegerLiteral>(x.initializer(), NodeKind::IntegerLiteral);
    CHECK(xValue.value() == -1);
    CHECK(xValue.location() == SourceLocation{3, 9});
    const auto& y = nodeAs<LetDecl>(statements[1].get(), NodeKind::LetDecl);
    CHECK(y.type() == DeclaredType{TypeName::Integer, false});
    const ASTNode* returned =
        nodeAs<ReturnStmt>(statements[2].get(), NodeKind::ReturnStmt).value();
    const auto& equality = nodeAs<BinaryExpr>(returned, NodeKind::BinaryExpr);
    checkIdentifier(equality.left(), "x");
    checkIdentifier(equality.right(), "y");
    CHECK(equality.right().location() == SourceLocation{7, 9});
}

TEST_CASE("T3-SYM-5 symbol x = 1 is a syntax error at =") {
    [[maybe_unused]] const SyntaxFailure inFunction =
        checkSyntaxErrorAtToken(inMain("symbol x = 1"), TokenKind::Assign, 1);
    [[maybe_unused]] const SyntaxFailure atModuleLevel = checkSyntaxErrorAtToken(
        "symbol x = 1\nfunction main():\n    pass\n", TokenKind::Assign, 1);
}

TEST_CASE("T3-NEST-1 a function-body equality chain is accepted at 255 operators, not 256") {
    // The body is level 1; each operator adds a level, so operator 255 opens level 256.
    for (const std::string_view op : {std::string_view{" == a"}, std::string_view{" != a"}}) {
        INFO("operator" << op);
        const Program program = parseSource(inMain("return a" + repeated(op, 255)));
        const ASTNode* node = &returnedValue(program);
        std::size_t depth = 0;
        while (node->kind() == NodeKind::BinaryExpr) {
            node = &static_cast<const BinaryExpr&>(*node).left();
            ++depth;
        }
        CHECK(depth == 255);
        checkIdentifier(*node, "a");

        const TokenKind kind = op == " == a" ? TokenKind::EqualEqual : TokenKind::BangEqual;
        [[maybe_unused]] const SyntaxFailure failure =
            checkSyntaxErrorAtToken(inMain("return a" + repeated(op, 256)), kind, 256);
    }
}

TEST_CASE("T3-NEST-2 ! through call argument lists is accepted at level 256, not 257") {
    // From level 1, openers alternate `!` and `(`; the 255th opener opens level 256.
    SECTION("accepted: the innermost ! opens level 256") {
        const Program program =
            parseSource(inMain("return " + repeated("!f(", 127) + "!a" + repeated(")", 127)));
        CHECK(returnedValue(program).kind() == NodeKind::UnaryExpr);
    }
    SECTION("rejected at the ( that opens level 257") {
        // Token 1 of LeftParen is in the `function main():` header.
        [[maybe_unused]] const SyntaxFailure failure = checkSyntaxErrorAtToken(
            inMain("return " + repeated("!f(", 128) + "a" + repeated(")", 128)),
            TokenKind::LeftParen, 129);
    }
    SECTION("rejected at the ! that opens level 257") {
        [[maybe_unused]] const SyntaxFailure failure = checkSyntaxErrorAtToken(
            inMain("return f(" + repeated("!f(", 127) + "!a" + repeated(")", 128)),
            TokenKind::Bang, 128);
    }
}

TEST_CASE("T3-NEST-3 100,000 ! tokens and a 100,000-operator chain fail without recursion") {
    constexpr std::size_t kOperators = 100'000;
    SECTION("100,000 ! tokens fail at the second !") {
        const SyntaxFailure failure = checkSyntaxErrorAtToken(
            inMain("return " + repeated("!", kOperators) + "a"), TokenKind::Bang, 2);
        CHECK(failure.message == "repeated ! is not allowed");
    }
    SECTION("100,000 ! tokens in a let initializer fail at the second !") {
        const SyntaxFailure failure = checkSyntaxErrorAtToken(
            inMain("let b: Boolean = " + repeated("!", kOperators) + "true"), TokenKind::Bang, 2);
        CHECK(failure.message == "repeated ! is not allowed");
    }
    SECTION("a 100,000-operator chain in a function body fails at operator 256") {
        [[maybe_unused]] const SyntaxFailure failure = checkSyntaxErrorAtToken(
            inMain("return a" + repeated(" == a", kOperators)), TokenKind::EqualEqual, 256);
    }
    SECTION("a 100,000-operator chain in a module-level initializer fails at operator 257") {
        [[maybe_unused]] const SyntaxFailure failure = checkSyntaxErrorAtToken(
            "const c: Boolean = a" + repeated(" != a", kOperators) +
                "\nfunction main():\n    pass\n",
            TokenKind::BangEqual, 257);
    }
}

TEST_CASE("T3-NEST-4 mixed call, !, and equality nesting is accepted at 256, not 257") {
    // Level 1 is the body; 200 call argument lists reach level 201.
    SECTION("a 55-operator chain inside 200 calls") {
        const Program program = parseSource(inMain("return " + repeated("f(", 200) + "a" +
                                                   repeated(" == a", 55) + repeated(")", 200)));
        CHECK(returnedValue(program).kind() == NodeKind::CallExpr);
        [[maybe_unused]] const SyntaxFailure failure = checkSyntaxErrorAtToken(
            inMain("return " + repeated("f(", 200) + "a" + repeated(" == a", 56) +
                   repeated(")", 200)),
            TokenKind::EqualEqual, 56);
    }
    SECTION("a 55-operator chain inside 100 !-prefixed calls") {
        const Program program = parseSource(inMain("return " + repeated("!f(", 100) + "a" +
                                                   repeated(" != a", 55) + repeated(")", 100)));
        CHECK(returnedValue(program).kind() == NodeKind::UnaryExpr);
        [[maybe_unused]] const SyntaxFailure failure = checkSyntaxErrorAtToken(
            inMain("return " + repeated("!f(", 100) + "a" + repeated(" != a", 56) +
                   repeated(")", 100)),
            TokenKind::BangEqual, 56);
    }
    SECTION("! and calls inside 200 calls") {
        const Program program = parseSource(inMain("return " + repeated("f(", 200) +
                                                   repeated("!g(", 27) + "!a" +
                                                   repeated(")", 227)));
        CHECK(returnedValue(program).kind() == NodeKind::CallExpr);
        [[maybe_unused]] const SyntaxFailure atBang = checkSyntaxErrorAtToken(
            inMain("return " + repeated("f(", 201) + repeated("!g(", 27) + "!a" +
                   repeated(")", 228)),
            TokenKind::Bang, 28);
        // Token 1 of LeftParen is in the `function main():` header.
        [[maybe_unused]] const SyntaxFailure atParen = checkSyntaxErrorAtToken(
            inMain("return " + repeated("f(", 200) + repeated("!g(", 28) + "a" +
                   repeated(")", 228)),
            TokenKind::LeftParen, 229);
    }
}

TEST_CASE("T3-NEST-6 a module-level initializer chain is accepted at 256 operators, not 257") {
    // A module-level initializer starts at level 0, so operator 256 opens level 256.
    const std::string tail = "\nfunction main():\n    pass\n";
    const Program program = parseSource("const c: Boolean = 1" + repeated(" == 1", 256) + tail);
    REQUIRE(program.size() == 2);
    const auto& constant = nodeAs<ConstDecl>(*program[0], NodeKind::ConstDecl);
    CHECK(constant.initializer() != nullptr);
    [[maybe_unused]] const SyntaxFailure failure = checkSyntaxErrorAtToken(
        "const c: Boolean = 1" + repeated(" == 1", 257) + tail, TokenKind::EqualEqual, 257);
}

} // namespace philios::test
