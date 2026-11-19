// Unit tests for src/runtime/evaluator/type_check.hpp (plan 0002 §3.4, D9, D12; T2-TYPE-6,
// task 3).
//
// matchesDeclaredType is checked on all 36 pairs of the four runtime classes and the nine
// declarable types against the §3.4 acceptance table; scriptTypeName is checked on the four
// runtime classes.

#include "runtime/evaluator/type_check.hpp"

#include "common/errors.hpp"
#include "frontend/ast/ast.hpp"
#include "runtime/environment/environment.hpp"
#include "runtime/object_model/function.hpp"
#include "runtime/object_model/integer.hpp"
#include "runtime/object_model/null_object.hpp"
#include "runtime/object_model/object.hpp"
#include "runtime/object_model/string_object.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace philios::test {
namespace {

// Column indices of the four runtime classes in kAcceptance and RuntimeValues.
constexpr std::size_t kInteger = 0;
constexpr std::size_t kString = 1;
constexpr std::size_t kFunction = 2;
constexpr std::size_t kNull = 3;
constexpr std::size_t kColumnCount = 4;

/// One row of the plan 0002 §3.4 acceptance table: a declarable type and, per runtime class,
/// whether it is accepted.
struct AcceptanceRow {
    std::string_view spelling;
    DeclaredType declared;
    std::array<bool, kColumnCount> accepts; ///< Indexed by the column constants.
};

/// Plan 0002 §3.4: the nine declarable types against Integer, String, Function, NullObject.
constexpr std::array<AcceptanceRow, 9> kAcceptance{{
    {"Integer", DeclaredType{TypeName::Integer, false}, {true, false, false, false}},
    {"String", DeclaredType{TypeName::String, false}, {false, true, false, false}},
    {"Function", DeclaredType{TypeName::Function, false}, {false, false, true, false}},
    {"Object", DeclaredType{TypeName::Object, false}, {true, true, true, false}},
    {"null", DeclaredType{TypeName::Null, false}, {false, false, false, true}},
    {"?Integer", DeclaredType{TypeName::Integer, true}, {true, false, false, true}},
    {"?String", DeclaredType{TypeName::String, true}, {false, true, false, true}},
    {"?Function", DeclaredType{TypeName::Function, true}, {false, false, true, true}},
    {"?Object", DeclaredType{TypeName::Object, true}, {true, true, true, true}},
}};

/// A `function f(): null:` declaration with a `pass` body, for building a Function value.
[[nodiscard]] std::shared_ptr<const FunctionDecl> makePassFunctionDecl() {
    std::vector<std::unique_ptr<ASTNode>> statements;
    statements.push_back(std::make_unique<PassStmt>(SourceLocation{2, 5}));
    auto body = std::make_unique<Block>(SourceLocation{2, 5}, std::move(statements));
    return std::make_shared<const FunctionDecl>(SourceLocation{1, 10}, "f",
                                                std::vector<std::unique_ptr<Param>>{},
                                                DeclaredType{TypeName::Null, false},
                                                std::move(body));
}

/// One value of each runtime class, indexed by the column constants. The Function's closure
/// environment does not bind the Function, so no ownership cycle is created.
struct RuntimeValues {
    std::shared_ptr<Object> integer = std::make_shared<Integer>(1);
    std::shared_ptr<Object> string = std::make_shared<String>("s");
    std::shared_ptr<Object> function =
        std::make_shared<Function>(makePassFunctionDecl(), std::make_shared<Environment>(nullptr));
    std::shared_ptr<Object> null = std::make_shared<NullObject>();

    [[nodiscard]] const Object& at(std::size_t column) const {
        switch (column) {
        case kInteger:
            return *integer;
        case kString:
            return *string;
        case kFunction:
            return *function;
        case kNull:
            return *null;
        default:
            throw InternalError("RuntimeValues::at: invalid column");
        }
    }
};

constexpr std::array<std::string_view, kColumnCount> kColumnNames{"Integer", "String", "Function",
                                                                   "NullObject"};

} // namespace

TEST_CASE("T2-TYPE-6 matchesDeclaredType follows the acceptance table for all 36 pairs") {
    const RuntimeValues values;
    std::size_t checked = 0;
    for (const AcceptanceRow& row : kAcceptance) {
        for (std::size_t column = 0; column < kColumnCount; ++column) {
            INFO("declared " << row.spelling << ", runtime class " << kColumnNames.at(column));
            CHECK(matchesDeclaredType(values.at(column), row.declared) == row.accepts.at(column));
            ++checked;
        }
    }
    CHECK(checked == 36);
}

TEST_CASE("T2-TYPE-6 scriptTypeName returns the script-level name of each runtime class") {
    const RuntimeValues values;
    CHECK(scriptTypeName(values.at(kInteger)) == "Integer");
    CHECK(scriptTypeName(values.at(kString)) == "String");
    CHECK(scriptTypeName(values.at(kFunction)) == "Function");
    CHECK(scriptTypeName(values.at(kNull)) == "null");
}

TEST_CASE("T2-TYPE-6 matchesDeclaredType and scriptTypeName are noexcept") {
    STATIC_CHECK(noexcept(matchesDeclaredType(std::declval<const Object&>(),
                                              DeclaredType{TypeName::Object, true})));
    STATIC_CHECK(noexcept(scriptTypeName(std::declval<const Object&>())));
}

} // namespace philios::test
