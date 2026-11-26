// Unit tests for src/analysis/semantic_analyzer/validator.hpp (plan 0001 §3.5; task 7).
//
// Per §8, the error cases of T-DECL-3, -4, -6, -7 (and T-DECL-1, -2, -5) are CLI golden tests;
// this file checks that the accepted cases validate.
//
// Plan 0002 (§3.4, D23, D29; task 3): the accepted cases of T2-TYPE-5 and T2-TYPE-9 and the new
// declarable types in non-`main` functions. Their error cases are CLI golden tests.
//
// Plan 0003 (§4.5, §4.5.1, §4.6–§4.9, §5.3; task 6): scopes and names. Each error case checks the
// class (`declaration`), the line and column, and the §5.3 message; each valid case checks that
// validation succeeds.
//
// Plan 0003 (§4.7 rule 5, §4.9, §4.10, §4.11.1, §4.11.5, §4.14.1, §5.3; task 7): static types.
// Error cases check the class, the line and column, and the §5.3 message; T3-ARCH-7 is a
// compile-time check of the `validateProgram` signature (AD1).

#include "analysis/semantic_analyzer/validator.hpp"
#include "common/errors.hpp"
#include "frontend/ast/ast.hpp"
#include "support/require_value.hpp"
#include "support/source_pipeline.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace philios::test {
namespace {

void parseAndValidate(std::string_view source) {
    const Program program = parseSource(source);
    validateProgram(program);
}

} // namespace

TEST_CASE("T-DECL-3 main annotated Integer or with no annotation is accepted") {
    CHECK_NOTHROW(parseAndValidate("function main(): Integer:\n    return 0\n"));
    CHECK_NOTHROW(parseAndValidate("function main():\n    return 0\n"));
}

TEST_CASE("T-DECL-4 a called null function with a bare return is accepted") {
    CHECK_NOTHROW(parseAndValidate("function n(): null:\n    return\n"
                                   "function main():\n    n()\n"));
}

TEST_CASE("T-DECL-6 a blank and a comment-only line after return are accepted") {
    CHECK_NOTHROW(parseAndValidate("function one(): Integer:\n    return 1\n\n    // done\n"
                                   "function main():\n    return one()\n"));
}

TEST_CASE("T-DECL-7 parameters and forward references to functions are accepted") {
    CHECK_NOTHROW(parseAndValidate("function main():\n    return helper(7)\n"
                                   "function helper(x: Integer): Integer:\n    return apply(id, x)\n"
                                   "function apply(f: Function, x: Integer): Integer:\n"
                                   "    return f(x)\n"
                                   "function id(x: Integer): Integer:\n    return x\n"));
}

TEST_CASE("T2-TYPE-5 a never-called ?Integer function whose body is a bare return is accepted") {
    CHECK_NOTHROW(parseAndValidate("function f(): ?Integer:\n    return\n"
                                   "function main():\n    return 0\n"));
}

TEST_CASE("T2-TYPE-9 a bare return is accepted in every ?T function (D29)") {
    CHECK_NOTHROW(parseAndValidate("function a(): ?Integer:\n    return\n"
                                   "function b(): ?String:\n    pass\n    return\n"
                                   "function c(): ?Function:\n    return\n"
                                   "function d(): ?Object:\n    return\n"
                                   "function main():\n    b()\n"));
}

TEST_CASE("T2-TYPE-8 the new declarable types are accepted on non-main functions") {
    CHECK_NOTHROW(parseAndValidate(
        "function f(a: Object, b: ?Integer, c: ?String, d: ?Function, e: ?Object): Object:\n"
        "    return a\n"
        "function g(x: ?Object): ?Object:\n    return x\n"
        "function main():\n    return 0\n"));
}

namespace {

// Plan 0003 §5.3 declaration messages.
constexpr std::string_view kInitializerRequired = "initializer required";
constexpr std::string_view kDuplicateDeclaration = "duplicate declaration";
constexpr std::string_view kModuleLevelName = "name must not match a module-level declaration";
constexpr std::string_view kUsedBeforeDeclaration = "used before declaration";
constexpr std::string_view kNotAllowedInModuleInitializer =
    "not allowed in a module-level initializer";
constexpr std::string_view kUndefinedIdentifier = "undefined identifier";
constexpr std::string_view kMainName = "main may be used only as its declaration name";
constexpr std::string_view kCannotAssignName = "cannot assign to this name";
constexpr std::string_view kCannotAssignConstant = "cannot assign to constant";

/// A DeclarationError's class, location, and message (plan 0003 §5.3).
struct DeclarationFailure {
    ErrorClass errorClass{ErrorClass::Declaration};
    SourceLocation location{};
    std::string message{};
};

/// The DeclarationError raised by validating @p source, or no value if validation succeeds.
/// Parsing must succeed. Any other exception (a SyntaxError from a malformed test source, or an
/// unexpected InternalError from the validator) propagates and fails the
/// test case without aborting the test binary.
[[nodiscard]] std::optional<DeclarationFailure> declarationFailure(std::string_view source) {
    const Program program = parseSource(source);
    try {
        validateProgram(program);
    } catch (const DeclarationError& error) {
        return DeclarationFailure{error.errorClass(), error.location(), error.what()};
    }
    return std::nullopt;
}

/// Requires that validating @p source fails with a declaration error at @p location with
/// @p message.
void checkDeclarationError(std::string_view source, SourceLocation location,
                           std::string_view message) {
    INFO("source:\n" << source);
    const DeclarationFailure failure = requireValue(declarationFailure(source));
    CHECK(failure.errorClass == ErrorClass::Declaration);
    CHECK(failure.location.line == location.line);
    CHECK(failure.location.column == location.column);
    CHECK(failure.message == message);
}

/// A trailing `main` that does nothing (one line plus its body).
constexpr std::string_view kTrailingMain = "function main():\n    return 0\n";

} // namespace

TEST_CASE("T3-DECL-2 a non-nullable let or const without an initializer fails at the keyword") {
    constexpr std::array<std::string_view, 7> types{"String", "Integer", "Function", "Object",
                                                    "Boolean", "Symbol", "null"};
    for (const std::string_view type : types) {
        DYNAMIC_SECTION("let label: " << type << " in main") {
            checkDeclarationError("function main():\n    let label: " + std::string(type) + "\n",
                                  SourceLocation{2, 5}, kInitializerRequired);
        }
        DYNAMIC_SECTION("let label: " << type << " in an uncalled function") {
            checkDeclarationError("function u(): null:\n    let label: " + std::string(type) +
                                      "\n" + std::string(kTrailingMain),
                                  SourceLocation{2, 5}, kInitializerRequired);
        }
    }
    SECTION("const c: Integer in main") {
        checkDeclarationError("function main():\n    const c: Integer\n", SourceLocation{2, 5},
                              kInitializerRequired);
    }
    SECTION("const c: Integer in an uncalled function") {
        checkDeclarationError("function u(): null:\n    const c: Integer\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{2, 5}, kInitializerRequired);
    }
}

TEST_CASE("T3-DECL-3 a same-scope duplicate of any kinds fails at the second name (D19)") {
    SECTION("let/let") {
        checkDeclarationError("function main():\n    let a: Integer = 1\n    let a: Integer = 2\n",
                              SourceLocation{3, 9}, kDuplicateDeclaration);
    }
    SECTION("let/const") {
        checkDeclarationError(
            "function main():\n    let a: Integer = 1\n    const a: Integer = 2\n",
            SourceLocation{3, 11}, kDuplicateDeclaration);
    }
    SECTION("const/symbol") {
        checkDeclarationError("function main():\n    const a: Integer = 1\n    symbol a\n",
                              SourceLocation{3, 12}, kDuplicateDeclaration);
    }
    SECTION("symbol/let") {
        checkDeclarationError("function main():\n    symbol a\n    let a: Integer = 1\n",
                              SourceLocation{3, 9}, kDuplicateDeclaration);
    }
    SECTION("parameter/let") {
        checkDeclarationError("function f(a: Integer): null:\n    let a: Integer = 1\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{2, 9}, kDuplicateDeclaration);
    }
    SECTION("parameter/symbol") {
        checkDeclarationError("function f(a: Integer): null:\n    symbol a\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{2, 12}, kDuplicateDeclaration);
    }
    SECTION("module function/const") {
        checkDeclarationError("function a(): null:\n    pass\nconst a: Integer = 1\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{3, 7}, kDuplicateDeclaration);
    }
    SECTION("module const/function") {
        checkDeclarationError("const a: Integer = 1\nfunction a(): null:\n    pass\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{2, 10}, kDuplicateDeclaration);
    }
    SECTION("module const/symbol") {
        checkDeclarationError("const a: Integer = 1\nsymbol a\n" + std::string(kTrailingMain),
                              SourceLocation{2, 8}, kDuplicateDeclaration);
    }
    SECTION("module symbol/const") {
        checkDeclarationError("symbol a\nconst a: Integer = 1\n" + std::string(kTrailingMain),
                              SourceLocation{2, 7}, kDuplicateDeclaration);
    }
    SECTION("module symbol/function") {
        checkDeclarationError("symbol a\nfunction a(): null:\n    pass\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{2, 10}, kDuplicateDeclaration);
    }
    SECTION("module function/symbol") {
        checkDeclarationError("function a(): null:\n    pass\nsymbol a\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{3, 8}, kDuplicateDeclaration);
    }
}

TEST_CASE("T3-DECL-4 a use or assignment before the declaration fails at the use") {
    SECTION("echo(x) before let x") {
        checkDeclarationError("function main():\n    echo(x)\n    let x: String = \"a\"\n",
                              SourceLocation{2, 10}, kUsedBeforeDeclaration);
    }
    SECTION("x = \"a\" before let x: ?String") {
        checkDeclarationError("function main():\n    x = \"a\"\n    let x: ?String\n",
                              SourceLocation{2, 5}, kUsedBeforeDeclaration);
    }
    SECTION("own initializer") {
        checkDeclarationError("function main():\n    let x: Integer = x\n", SourceLocation{2, 22},
                              kUsedBeforeDeclaration);
    }
}

TEST_CASE("T3-DECL-5 main as a let, const, or symbol name or as an assignment target fails") {
    SECTION("let main") {
        checkDeclarationError("function main():\n    let main: Integer = 1\n", SourceLocation{2, 9},
                              kMainName);
    }
    SECTION("const main") {
        checkDeclarationError("function main():\n    const main: ?Integer\n",
                              SourceLocation{2, 11}, kMainName);
    }
    SECTION("symbol main") {
        checkDeclarationError("function main():\n    symbol main\n", SourceLocation{2, 12},
                              kMainName);
    }
    SECTION("main = 1") {
        checkDeclarationError("function main():\n    main = 1\n", SourceLocation{2, 5}, kMainName);
    }
}

TEST_CASE("T3-DECL-5 a module-level const or symbol named main fails at the name") {
    SECTION("const main without a main function") {
        checkDeclarationError("const main: Integer = 1\n", SourceLocation{1, 7}, kMainName);
    }
    SECTION("const main before the main function") {
        checkDeclarationError("const main: ?Integer\n" + std::string(kTrailingMain),
                              SourceLocation{1, 7}, kMainName);
    }
    SECTION("const main after the main function") {
        checkDeclarationError(std::string(kTrailingMain) + "const main: Integer = 1\n",
                              SourceLocation{3, 7}, kMainName);
    }
    SECTION("symbol main without a main function") {
        checkDeclarationError("symbol main\n", SourceLocation{1, 8}, kMainName);
    }
    SECTION("symbol main before the main function") {
        checkDeclarationError("symbol main\n" + std::string(kTrailingMain), SourceLocation{1, 8},
                              kMainName);
    }
    SECTION("symbol main after the main function") {
        checkDeclarationError(std::string(kTrailingMain) + "symbol main\n", SourceLocation{3, 8},
                              kMainName);
    }
}

TEST_CASE("T3-SCOPE-1 a parameter named like a module function fails at the parameter") {
    SECTION("existing fixture decl_2_parameter_named_like_function") {
        checkDeclarationError("function g(): null:\n    pass\nfunction f(g: Integer): null:\n"
                              "    pass\nfunction main():\n    pass\n",
                              SourceLocation{3, 12}, kModuleLevelName);
    }
    SECTION("the function's own name") {
        checkDeclarationError("function f(f: Integer): Integer:\n    return f\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{1, 12}, kModuleLevelName);
    }
}

TEST_CASE("T3-SCOPE-2 every function-scope kind fails against every module-level kind") {
    const std::string moduleItems =
        "function g(): null:\n    pass\nconst k: Integer = 1\nsymbol s\n"; // lines 1-4
    constexpr std::array<std::string_view, 3> names{"g", "k", "s"};
    for (const std::string_view nameView : names) {
        const std::string name(nameView);
        struct Case {
            std::string label;
            std::string function; // starts on line 5
            std::string call;
            SourceLocation location;
        };
        const std::array<Case, 4> cases{
            Case{"parameter " + name,
                 "function u(" + name + ": Integer): null:\n    pass\n", "u(1)",
                 SourceLocation{5, 12}},
            Case{"let " + name, "function u(): null:\n    let " + name + ": Integer = 1\n", "u()",
                 SourceLocation{6, 9}},
            Case{"const " + name, "function u(): null:\n    const " + name + ": ?String\n", "u()",
                 SourceLocation{6, 11}},
            Case{"symbol " + name, "function u(): null:\n    symbol " + name + "\n", "u()",
                 SourceLocation{6, 12}},
        };
        for (const Case& entry : cases) {
            DYNAMIC_SECTION(entry.label << " in an uncalled function") {
                checkDeclarationError(moduleItems + entry.function + std::string(kTrailingMain),
                                      entry.location, kModuleLevelName);
            }
            DYNAMIC_SECTION(entry.label << " in a called function") {
                checkDeclarationError(moduleItems + entry.function + "function main():\n    " +
                                          entry.call + "\n",
                                      entry.location, kModuleLevelName);
            }
        }
    }
}

TEST_CASE("T3-SCOPE-3 the module-level name rule is independent of declaration order") {
    SECTION("let limit before module const limit") {
        checkDeclarationError("function f(): null:\n    let limit: Integer = 5\n"
                              "const limit: Integer = 10\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{2, 9}, kModuleLevelName);
    }
    SECTION("parameter tag before module symbol tag") {
        checkDeclarationError("function f(tag: Symbol): null:\n    pass\nsymbol tag\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{1, 12}, kModuleLevelName);
    }
    SECTION("symbol h before module function h") {
        checkDeclarationError("function f(): null:\n    symbol h\nfunction h(): null:\n    pass\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{2, 12}, kModuleLevelName);
    }
}

TEST_CASE("T3-SCOPE-4 function-scope names are checked before the body") {
    SECTION("name error at the let x, not a type error at the earlier use") {
        checkDeclarationError("const x: String = \"a\"\nfunction f(): null:\n"
                              "    let y: Integer = x\n    let x: Integer = 2\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{4, 9}, kModuleLevelName);
    }
    SECTION("no module-level z: used before declaration at the first z") {
        checkDeclarationError("function main():\n    echo(z)\n    let z: String = \"a\"\n",
                              SourceLocation{2, 10}, kUsedBeforeDeclaration);
    }
}

TEST_CASE("T3-SCOPE-5 function-scope names are checked main, then duplicate, then module-level") {
    SECTION("parameter named main keeps the existing main error") {
        checkDeclarationError("function f(main: Integer): null:\n    pass\nfunction main():\n"
                              "    pass\n",
                              SourceLocation{1, 12}, kMainName);
    }
    SECTION("two parameters named like a module function fail at the first") {
        checkDeclarationError("function g(): null:\n    pass\n"
                              "function f(g: Integer, g: Integer): null:\n    pass\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{3, 12}, kModuleLevelName);
    }
    SECTION("parameter p and let p fail as a duplicate at the let name") {
        checkDeclarationError("function f(p: Integer): null:\n    let p: Integer = 1\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{2, 9}, kDuplicateDeclaration);
    }
}

TEST_CASE("T3-SCOPE-6 D19 duplicates still apply within each scope") {
    SECTION("parameter and let in one function") {
        checkDeclarationError("function f(n: Integer): null:\n    let n: Integer = 1\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{2, 9}, kDuplicateDeclaration);
    }
    SECTION("module const and module function") {
        checkDeclarationError("const n: Integer = 1\nfunction n(): null:\n    pass\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{2, 10}, kDuplicateDeclaration);
    }
}

TEST_CASE("T3-SCOPE-7 different functions may reuse local names; module names stay visible") {
    CHECK_NOTHROW(parseAndValidate("function a(): Integer:\n    let n: Integer = 1\n    return n\n"
                                   "function b(n: Integer): Integer:\n    return n\n"
                                   "function main():\n    return a()\n"));
    CHECK_NOTHROW(parseAndValidate("function l(): Integer:\n    return limit\n"
                                   "const limit: Integer = 10\n"
                                   "function main():\n    return l()\n"));
}

TEST_CASE("T3-MOD-2 a function declared before a module constant may use it (DA1 c)") {
    CHECK_NOTHROW(parseAndValidate("function show(): null:\n    echo(typeof(k))\n"
                                   "const k: Integer = 1\n"
                                   "function main():\n    show()\n"));
}

TEST_CASE("T3-MOD-3 a module-level initializer may not use a later or its own name") {
    SECTION("later constant") {
        checkDeclarationError("const a: Integer = b\nconst b: Integer = 1\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{1, 20}, kUsedBeforeDeclaration);
    }
    SECTION("own name") {
        checkDeclarationError("const a: Integer = a\n" + std::string(kTrailingMain),
                              SourceLocation{1, 20}, kUsedBeforeDeclaration);
    }
}

TEST_CASE("T3-MOD-4 calls, pseudo-calls, and function names are not allowed in module "
          "initializers") {
    const std::string functions = "function f(): Integer:\n    return 1\n"
                                  "function helper(): Integer:\n    return 1\n" +
                                  std::string(kTrailingMain);
    SECTION("call") {
        checkDeclarationError("const a: Integer = f()\n" + functions, SourceLocation{1, 20},
                              kNotAllowedInModuleInitializer);
    }
    SECTION("echo") {
        checkDeclarationError("const s: null = echo(\"x\")\n" + functions, SourceLocation{1, 17},
                              kNotAllowedInModuleInitializer);
    }
    SECTION("typeof") {
        checkDeclarationError("const t: String = typeof(1)\n" + functions,
                              SourceLocation{1, 19}, kNotAllowedInModuleInitializer);
    }
    SECTION("function name as a value") {
        checkDeclarationError("const h: Function = helper\n" + functions, SourceLocation{1, 21},
                              kNotAllowedInModuleInitializer);
    }
    SECTION("call as an equality operand") {
        checkDeclarationError("const b: Boolean = f() == 1\n" + functions,
                              SourceLocation{1, 20}, kNotAllowedInModuleInitializer);
    }
}

TEST_CASE("T3-MOD-5 a function may not assign to a module-level constant") {
    const std::string moduleItems = "const n: ?String\nconst k: Integer = 1\n"; // lines 1-2
    constexpr std::array<std::string_view, 2> assignments{"n = \"a\"", "k = 2"};
    for (const std::string_view assignment : assignments) {
        const std::string function =
            "function w(): null:\n    " + std::string(assignment) + "\n"; // target at 4:5
        DYNAMIC_SECTION(assignment << " in an uncalled function") {
            checkDeclarationError(moduleItems + function + std::string(kTrailingMain),
                                  SourceLocation{4, 5}, kCannotAssignConstant);
        }
        DYNAMIC_SECTION(assignment << " in a called function") {
            checkDeclarationError(moduleItems + function + "function main():\n    w()\n",
                                  SourceLocation{4, 5}, kCannotAssignConstant);
        }
    }
}

TEST_CASE("T3-CONST-1 assignment to a non-nullable constant fails at the target") {
    checkDeclarationError("function main():\n    const k: Integer = 1\n    k = 2\n",
                          SourceLocation{3, 5}, kCannotAssignConstant);
}

TEST_CASE("T3-CONST-2 a nullable constant accepts one non-null write") {
    const std::string body = "function main():\n    const n: ?String\n    echo(typeof(n))\n"
                             "    n = \"a\"\n    echo(n)\n";
    SECTION("one write validates") {
        CHECK_NOTHROW(parseAndValidate(body));
    }
    SECTION("a second write fails at the second target") {
        checkDeclarationError(body + "    n = \"b\"\n", SourceLocation{6, 5},
                              kCannotAssignConstant);
    }
}

TEST_CASE("T3-CONST-4 a nullable constant with a non-null initializer is already assigned") {
    checkDeclarationError("function main():\n    const n: ?String = \"x\"\n    n = \"y\"\n",
                          SourceLocation{3, 5}, kCannotAssignConstant);
}

TEST_CASE("T3-CONST-11 a null-typed constant validates and cannot be assigned") {
    const std::string body = "function main():\n    const z: null = null\n";
    SECTION("declaration validates") {
        CHECK_NOTHROW(parseAndValidate(body));
    }
    SECTION("a later z = null fails at the target") {
        checkDeclarationError(body + "    z = null\n", SourceLocation{3, 5},
                              kCannotAssignConstant);
    }
}

TEST_CASE("T3-ASSIGN-5 assignment to a non-assignable name fails at the target") {
    SECTION("undefined name") {
        checkDeclarationError("function main():\n    nope = 1\n", SourceLocation{2, 5},
                              kUndefinedIdentifier);
    }
    SECTION("module function name") {
        checkDeclarationError("function helper(): null:\n    pass\nfunction main():\n"
                              "    helper = 1\n",
                              SourceLocation{4, 5}, kCannotAssignName);
    }
    SECTION("module-level symbol") {
        checkDeclarationError("symbol s\nfunction main():\n    s = 1\n", SourceLocation{3, 5},
                              kCannotAssignName);
    }
    SECTION("main") {
        checkDeclarationError("function main():\n    main = 1\n", SourceLocation{2, 5}, kMainName);
    }
}

TEST_CASE("T3-SYM-5 assignment to a symbol fails at the target (D14)") {
    SECTION("sym = 1") {
        checkDeclarationError("function main():\n    symbol sym\n    sym = 1\n",
                              SourceLocation{3, 5}, kCannotAssignName);
    }
    SECTION("sym = sym") {
        checkDeclarationError("function main():\n    symbol sym\n    sym = sym\n",
                              SourceLocation{3, 5}, kCannotAssignName);
    }
}

namespace {

// Plan 0003 §5.3 static-type messages (task 7).
constexpr std::string_view kTypeMismatch = "type mismatch";
constexpr std::string_view kNullableForNonNullable = "nullable value for non-nullable target";
constexpr std::string_view kConstantNotNull = "constant value must not be null";
constexpr std::string_view kNotOperandBoolean = "operand of ! must be Boolean";
constexpr std::string_view kSymbolNotAllowed = "symbol not allowed here";

/// A source line under test, the expected column of its error, and a section name.
struct StaticCase {
    std::string_view name;
    std::string_view line;
    std::size_t column;
};

} // namespace

TEST_CASE("T3-ARCH-7 validateProgram returns nothing and writes no artifact (AD1)") {
    static_assert(std::is_same_v<decltype(&validateProgram), void (*)(const Program&)>);
    SUCCEED();
}

TEST_CASE("T3-DECL-6 a static type disjoint from the declared type fails at the initializer") {
    const std::string prefix = "function f(): Integer:\n    return 1\nfunction main():\n";
    constexpr std::array<StaticCase, 6> cases{{
        {"String to Integer", "    let x: Integer = \"a\"", 22},
        {"null to Integer", "    let x: Integer = null", 22},
        {"Integer to Boolean", "    let b: Boolean = 1", 22},
        {"null to Object", "    let o: Object = null", 21},
        {"Integer call to String", "    let s: String = f()", 21},
        {"Boolean to Integer", "    let i: Integer = true", 22},
    }};
    for (const StaticCase& item : cases) {
        DYNAMIC_SECTION(item.name) {
            checkDeclarationError(prefix + std::string(item.line) + "\n",
                                  SourceLocation{4, item.column}, kTypeMismatch);
        }
    }
}

TEST_CASE("T3-DECL-7 accepted subtypes validate") {
    const std::string prefix = "function helper(): null:\n    pass\nfunction main():\n";
    SECTION("Integer to Object") {
        CHECK_NOTHROW(parseAndValidate(prefix + "    let o: Object = 1\n"));
    }
    SECTION("Boolean to ?Object") {
        CHECK_NOTHROW(parseAndValidate(prefix + "    let o: ?Object = true\n"));
    }
    SECTION("function name to Function") {
        CHECK_NOTHROW(parseAndValidate(prefix + "    let f2: Function = helper\n"));
    }
    SECTION("equality to Boolean") {
        CHECK_NOTHROW(parseAndValidate(prefix + "    let b: Boolean = 1 == 2\n"));
    }
}

TEST_CASE("T3-DECL-9 a nullable value for a non-nullable target fails at the right-hand side") {
    // Lines 1-5; the statement under test starts on line 6.
    const std::string prefix = "function q(): ?Integer:\n    return null\n"
                               "function r(): ?Object:\n    return null\n"
                               "function t(n: ?String, p: Integer): null:\n";
    struct NullableCase {
        std::string_view name;
        std::string_view body;
        SourceLocation location;
    };
    constexpr std::array<NullableCase, 6> cases{{
        {"let Integer from ?Integer call", "    let i: Integer = q()\n", {6, 22}},
        {"const Integer from ?Integer call", "    const k: Integer = q()\n", {6, 24}},
        {"let Object from ?Object call", "    let o: Object = r()\n", {6, 21}},
        {"let Object from ?String parameter", "    let o: Object = n\n", {6, 21}},
        {"assignment to Integer let", "    let x: Integer = 1\n    x = q()\n", {7, 9}},
        {"assignment to Integer parameter", "    p = q()\n", {6, 9}},
    }};
    for (const NullableCase& item : cases) {
        DYNAMIC_SECTION(item.name << " in a called function") {
            checkDeclarationError(prefix + std::string(item.body) +
                                      "function main():\n    t(null, 1)\n",
                                  item.location, kNullableForNonNullable);
        }
        DYNAMIC_SECTION(item.name << " in an uncalled function") {
            checkDeclarationError(prefix + std::string(item.body) + std::string(kTrailingMain),
                                  item.location, kNullableForNonNullable);
        }
    }
    SECTION("nullable targets validate") {
        CHECK_NOTHROW(parseAndValidate(prefix + "    let j: ?Integer = q()\n" +
                                       "    let o: ?Object = n\n" + std::string(kTrailingMain)));
    }
    SECTION("a call through a Function parameter is dynamic and validates") {
        CHECK_NOTHROW(parseAndValidate("function h(g: Function): null:\n"
                                       "    let i: Integer = g()\n" +
                                       std::string(kTrailingMain)));
    }
}

TEST_CASE("T3-CONST-5 null written to a nullable constant fails (D8, DR-4)") {
    SECTION("unassigned constant: at null") {
        checkDeclarationError("function main():\n    const n: ?String\n    n = null\n",
                              SourceLocation{3, 9}, kConstantNotNull);
    }
    SECTION("after an assignment: at the target (DR-4)") {
        checkDeclarationError(
            "function main():\n    const n: ?String\n    n = \"a\"\n    n = null\n",
            SourceLocation{4, 5}, kCannotAssignConstant);
    }
}

TEST_CASE("T3-CONST-6 a statically nullable constant write fails at the right-hand side (D15)") {
    // Lines 1-5; the statement under test starts on line 6.
    const std::string prefix = "function r(): ?String:\n    return null\n"
                               "function e(): null:\n    pass\n"
                               "function t(p: Object): null:\n";
    const std::string declaration = "    const n: ?String\n"; // line 6
    constexpr std::array<StaticCase, 3> writes{{
        {"?String call", "    n = r()", 9},
        {"null call", "    n = e()", 9},
        {"echo", "    n = echo(\"x\")", 9},
    }};
    for (const StaticCase& item : writes) {
        DYNAMIC_SECTION(item.name) {
            checkDeclarationError(prefix + declaration + std::string(item.line) + "\n" +
                                      std::string(kTrailingMain),
                                  SourceLocation{7, item.column}, kConstantNotNull);
        }
    }
    SECTION("initializer from a ?String call") {
        checkDeclarationError(prefix + "    const m: ?String = r()\n" + std::string(kTrailingMain),
                              SourceLocation{6, 24}, kConstantNotNull);
    }
    SECTION("an Object parameter validates") {
        CHECK_NOTHROW(
            parseAndValidate(prefix + declaration + "    n = p\n" + std::string(kTrailingMain)));
    }
}

TEST_CASE("T3-ASSIGN-3 a disjoint assignment fails at the right-hand side") {
    SECTION("String to Integer") {
        checkDeclarationError("function main():\n    let x: Integer = 1\n    x = \"a\"\n",
                              SourceLocation{3, 9}, kTypeMismatch);
    }
    SECTION("Integer to Boolean") {
        checkDeclarationError("function main():\n    let b: Boolean = true\n    b = 1\n",
                              SourceLocation{3, 9}, kTypeMismatch);
    }
}

TEST_CASE("T3-ASSIGN-7 parameters are assignable and statically checked (DA2)") {
    SECTION("assignment to a String parameter validates") {
        CHECK_NOTHROW(parseAndValidate("function f(p: String): String:\n    p = \"b\"\n"
                                       "    return p\n"
                                       "function main():\n    let s: String = \"a\"\n"
                                       "    f(s)\n    echo(s)\n"));
    }
    SECTION("null to a ?String parameter validates") {
        CHECK_NOTHROW(parseAndValidate("function f(p: ?String): null:\n    p = null\n" +
                                       std::string(kTrailingMain)));
    }
    SECTION("Integer to a String parameter") {
        checkDeclarationError("function f(p: String): null:\n    p = 1\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{2, 9}, kTypeMismatch);
    }
    SECTION("null to a String parameter") {
        checkDeclarationError("function f(p: String): null:\n    p = null\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{2, 9}, kTypeMismatch);
    }
    SECTION("an Object value to a String parameter validates") {
        CHECK_NOTHROW(parseAndValidate("function f(p: String, o: Object): null:\n    p = o\n" +
                                       std::string(kTrailingMain)));
    }
}

TEST_CASE("T3-SYM-3 a bare symbol to an Object target fails at the name (D17)") {
    const std::string prefix = "symbol philiosIsRad\nfunction main():\n";
    SECTION("Object") {
        checkDeclarationError(prefix + "    let o: Object = philiosIsRad\n",
                              SourceLocation{3, 21}, kSymbolNotAllowed);
    }
    SECTION("?Object") {
        checkDeclarationError(prefix + "    let o: ?Object = philiosIsRad\n",
                              SourceLocation{3, 22}, kSymbolNotAllowed);
    }
    SECTION("a Symbol variable widens to Object") {
        CHECK_NOTHROW(parseAndValidate(prefix + "    let s: Symbol = philiosIsRad\n"
                                                "    let o: Object = s\n"
                                                "    echo(typeof(o))\n"));
    }
}

TEST_CASE("T3-SYM-4 a bare symbol outside the allowed contexts fails at the name (D18)") {
    // Lines 1-4; the statement under test is on line 5.
    const std::string prefix = "function f(x: Object): null:\n    pass\n"
                               "function main():\n    symbol sym\n";
    constexpr std::array<StaticCase, 5> cases{{
        {"echo argument", "    echo(sym)", 10},
        {"typeof argument", "    typeof(sym)", 12},
        {"call argument", "    f(sym)", 7},
        {"callee", "    sym()", 5},
        {"Integer initializer", "    let i: Integer = sym", 22},
    }};
    for (const StaticCase& item : cases) {
        DYNAMIC_SECTION(item.name) {
            checkDeclarationError(prefix + std::string(item.line) + "\n",
                                  SourceLocation{5, item.column}, kSymbolNotAllowed);
        }
    }
    SECTION("return value in a Symbol function") {
        checkDeclarationError("function g(): Symbol:\n    symbol sym\n    return sym\n" +
                                  std::string(kTrailingMain),
                              SourceLocation{3, 12}, kSymbolNotAllowed);
    }
    SECTION("operand of !") {
        // The §5.3 `!` row names a bare-symbol operand explicitly.
        checkDeclarationError(prefix + "    let b: Boolean = !sym\n", SourceLocation{5, 23},
                              kNotOperandBoolean);
    }
}

TEST_CASE("T3-OP-2 a non-Boolean or nullable ! operand fails at the operand") {
    // Lines 1-6; the statement under test is on line 7.
    const std::string prefix = "function f(): ?Boolean:\n    return null\n"
                               "function g(): Integer:\n    return 1\n"
                               "function t(p: ?Boolean, o: ?Object): null:\n    symbol sym\n";
    constexpr std::array<std::string_view, 9> operands{"1", "\"a\"", "null", "-1", "f()",
                                                       "p", "o", "g()", "sym"};
    for (const std::string_view operand : operands) {
        const std::string source =
            prefix + "    let b: Boolean = !" + std::string(operand) + "\n"; // operand at 7:23
        DYNAMIC_SECTION("!" << operand << " in a called function") {
            checkDeclarationError(source + "function main():\n    t(null, null)\n",
                                  SourceLocation{7, 23}, kNotOperandBoolean);
        }
        DYNAMIC_SECTION("!" << operand << " in an uncalled function") {
            checkDeclarationError(source + std::string(kTrailingMain), SourceLocation{7, 23},
                                  kNotOperandBoolean);
        }
    }
}

TEST_CASE("T3-LEX-5 !-1 is a declaration error at the negative literal") {
    checkDeclarationError("function main():\n    let b: Boolean = !-1\n", SourceLocation{2, 23},
                          kNotOperandBoolean);
}

TEST_CASE("T3-EQ-2 equality of differently typed operands passes validation") {
    constexpr std::array<std::string_view, 5> pairs{
        {"1 OP \"1\"", "null OP 0", "true OP 1", "\"\" OP null", "helper OP 1"}};
    constexpr std::array<std::string_view, 2> operators{"==", "!="};
    for (const std::string_view pair : pairs) {
        for (const std::string_view op : operators) {
            std::string expression(pair);
            expression.replace(expression.find("OP"), 2, op);
            DYNAMIC_SECTION(expression) {
                CHECK_NOTHROW(parseAndValidate("function helper(): null:\n    pass\n"
                                               "function main():\n    let b: Boolean = " +
                                               expression + "\n"));
            }
        }
    }
}

TEST_CASE("T3-MOD-9 static checks apply to module-level initializers") {
    SECTION("type mismatch") {
        checkDeclarationError("const x: Integer = \"a\"\n" + std::string(kTrailingMain),
                              SourceLocation{1, 20}, kTypeMismatch);
    }
    SECTION("bare symbol to Object") {
        checkDeclarationError("symbol s\nconst o: Object = s\n" + std::string(kTrailingMain),
                              SourceLocation{2, 19}, kSymbolNotAllowed);
    }
    SECTION("! operand") {
        checkDeclarationError("const b: Boolean = !1\n" + std::string(kTrailingMain),
                              SourceLocation{1, 21}, kNotOperandBoolean);
    }
}

} // namespace philios::test
