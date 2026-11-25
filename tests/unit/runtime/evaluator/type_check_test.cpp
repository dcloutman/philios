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
#include "runtime/object_model/boolean.hpp"
#include "runtime/object_model/function.hpp"
#include "runtime/object_model/integer.hpp"
#include "runtime/object_model/null_object.hpp"
#include "runtime/object_model/object.hpp"
#include "runtime/object_model/string_object.hpp"
#include "runtime/object_model/symbol.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
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

// ---------------------------------------------------------------------------------------------
// Plan 0003 task 5: Boolean and Symbol acceptance, script type names, and valuesEqual (§4.3,
// §4.11.2, §4.11.3, §6.2, §6.3; D11, D21; T3-TYPE-1, T3-TYPE-2, T3-EQ-1 to T3-EQ-4, T3-SYM-6).
// ---------------------------------------------------------------------------------------------

namespace {

// Column indices of the six runtime classes in kAcceptance3 and RuntimeValues3.
constexpr std::size_t kInteger3 = 0;
constexpr std::size_t kString3 = 1;
constexpr std::size_t kFunction3 = 2;
constexpr std::size_t kNull3 = 3;
constexpr std::size_t kBoolean3 = 4;
constexpr std::size_t kSymbol3 = 5;
constexpr std::size_t kColumnCount3 = 6;

/// One row of the plan 0003 §4.3 acceptance table, over the six runtime classes.
struct AcceptanceRow3 {
    std::string_view spelling;
    DeclaredType declared;
    std::array<bool, kColumnCount3> accepts; ///< Indexed by the `*3` column constants.
};

/// Plan 0003 §4.3: the new and changed declarable types against Integer, String, Function,
/// NullObject, Boolean, Symbol.
constexpr std::array<AcceptanceRow3, 6> kAcceptance3{{
    {"Boolean", DeclaredType{TypeName::Boolean, false}, {false, false, false, false, true, false}},
    {"?Boolean", DeclaredType{TypeName::Boolean, true}, {false, false, false, true, true, false}},
    {"Symbol", DeclaredType{TypeName::Symbol, false}, {false, false, false, false, false, true}},
    {"?Symbol", DeclaredType{TypeName::Symbol, true}, {false, false, false, true, false, true}},
    {"Object", DeclaredType{TypeName::Object, false}, {true, true, true, false, true, true}},
    {"?Object", DeclaredType{TypeName::Object, true}, {true, true, true, true, true, true}},
}};

/// One value of each of the six runtime classes, indexed by the `*3` column constants.
struct RuntimeValues3 {
    RuntimeValues base;
    std::shared_ptr<Object> boolean = std::make_shared<Boolean>(true);
    std::shared_ptr<Object> symbol = std::make_shared<Symbol>();

    [[nodiscard]] const Object& at(std::size_t column) const {
        switch (column) {
        case kBoolean3:
            return *boolean;
        case kSymbol3:
            return *symbol;
        default:
            return base.at(column);
        }
    }
};

constexpr std::array<std::string_view, kColumnCount3> kColumnNames3{
    "Integer", "String", "Function", "NullObject", "Boolean", "Symbol"};

/// A new Function value with a `pass` body and an empty closure environment.
[[nodiscard]] std::shared_ptr<Object> makePassFunction() {
    return std::make_shared<Function>(makePassFunctionDecl(),
                                      std::make_shared<Environment>(nullptr));
}

} // namespace

TEST_CASE("T3-TYPE-1 T3-TYPE-2 matchesDeclaredType follows the plan 0003 acceptance table") {
    const RuntimeValues3 values;
    std::size_t checked = 0;
    for (const AcceptanceRow3& row : kAcceptance3) {
        for (std::size_t column = 0; column < kColumnCount3; ++column) {
            INFO("declared " << row.spelling << ", runtime class " << kColumnNames3.at(column));
            CHECK(matchesDeclaredType(values.at(column), row.declared) == row.accepts.at(column));
            ++checked;
        }
    }
    CHECK(checked == 36);
}

TEST_CASE("T3-TYPE-1 plan 0002 types other than Object reject Boolean and Symbol values") {
    const RuntimeValues3 values;
    for (const AcceptanceRow& row : kAcceptance) {
        const bool isObject = (row.declared.base == TypeName::Object);
        INFO("declared " << row.spelling);
        CHECK(matchesDeclaredType(values.at(kBoolean3), row.declared) == isObject);
        CHECK(matchesDeclaredType(values.at(kSymbol3), row.declared) == isObject);
    }
}

TEST_CASE("T3-TYPE-2 scriptTypeName returns Boolean and Symbol") {
    const RuntimeValues3 values;
    CHECK(scriptTypeName(values.at(kBoolean3)) == "Boolean");
    CHECK(scriptTypeName(Boolean{false}) == "Boolean");
    CHECK(scriptTypeName(values.at(kSymbol3)) == "Symbol");
    CHECK(scriptTypeName(values.at(kInteger3)) == "Integer");
    CHECK(scriptTypeName(values.at(kNull3)) == "null");
}

TEST_CASE("T3-EQ-1 valuesEqual compares Integer, String, Boolean, and null by value") {
    // Every operand is a distinct object, so value (not identity) comparison is exercised.
    CHECK(valuesEqual(Integer{1}, Integer{1}));
    CHECK_FALSE(valuesEqual(Integer{1}, Integer{2}));
    CHECK(valuesEqual(String{"a"}, String{"a"}));
    CHECK(valuesEqual(String{""}, String{""}));
    CHECK_FALSE(valuesEqual(String{"a"}, String{"b"}));
    CHECK_FALSE(valuesEqual(String{"a"}, String{"ab"}));
    CHECK_FALSE(valuesEqual(String{"ab"}, String{"a"}));
    CHECK(valuesEqual(Boolean{true}, Boolean{true}));
    CHECK(valuesEqual(Boolean{false}, Boolean{false}));
    CHECK_FALSE(valuesEqual(Boolean{true}, Boolean{false}));
    CHECK_FALSE(valuesEqual(Boolean{false}, Boolean{true}));
    CHECK(valuesEqual(NullObject{}, NullObject{}));
    const NullObject null;
    CHECK(valuesEqual(null, null));
}

TEST_CASE("T3-EQ-1 valuesEqual compares Integer extremes exactly (plan 0003 §6.2)") {
    constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    CHECK(valuesEqual(Integer{kMin}, Integer{kMin}));
    CHECK(valuesEqual(Integer{kMax}, Integer{kMax}));
    // `9223372036854775807 != -1` is true.
    CHECK_FALSE(valuesEqual(Integer{kMax}, Integer{-1}));
    CHECK_FALSE(valuesEqual(Integer{kMin}, Integer{kMax}));
    CHECK_FALSE(valuesEqual(Integer{kMin}, Integer{0}));
}

TEST_CASE("T3-EQ-2 valuesEqual is false when the runtime classes differ") {
    const std::shared_ptr<Object> helper = makePassFunction();
    const Integer one{1};
    const Integer zero{0};
    const String oneString{"1"};
    const String empty{""};
    const NullObject null;
    const Boolean trueValue{true};

    struct Pair {
        std::string_view description;
        const Object& left;
        const Object& right;
    };
    const std::array<Pair, 5> pairs{{
        {R"(1 == "1")", one, oneString},
        {"null == 0", null, zero},
        {"true == 1", trueValue, one},
        {R"("" == null)", empty, null},
        {"helper == 1", *helper, one},
    }};
    for (const Pair& pair : pairs) {
        INFO(pair.description);
        CHECK_FALSE(valuesEqual(pair.left, pair.right));
        CHECK_FALSE(valuesEqual(pair.right, pair.left));
    }
}

TEST_CASE("T3-EQ-2 valuesEqual is false for every pair of distinct runtime classes") {
    const RuntimeValues3 values;
    for (std::size_t left = 0; left < kColumnCount3; ++left) {
        for (std::size_t right = 0; right < kColumnCount3; ++right) {
            if (left == right) {
                continue;
            }
            INFO(kColumnNames3.at(left) << " == " << kColumnNames3.at(right));
            CHECK_FALSE(valuesEqual(values.at(left), values.at(right)));
        }
    }
}

TEST_CASE("T3-EQ-3 valuesEqual compares Function values by identity") {
    const std::shared_ptr<Object> helper = makePassFunction();
    const std::shared_ptr<Object> other = makePassFunction();
    CHECK(valuesEqual(*helper, *helper));
    CHECK_FALSE(valuesEqual(*helper, *other));
    CHECK_FALSE(valuesEqual(*other, *helper));

    // A Function passed through a parameter is an owning copy of the same instance.
    const std::shared_ptr<Object> passed = helper;
    CHECK(valuesEqual(*passed, *helper));
}

TEST_CASE("T3-EQ-3 valuesEqual ignores static types (D21)") {
    // `let o: Object = 1` then `o == 1`: the static type of `o` is not an input.
    const std::shared_ptr<Object> o = std::make_shared<Integer>(1);
    CHECK(valuesEqual(*o, Integer{1}));

    // Two String values with the same bytes built from different literals are equal.
    const std::shared_ptr<Object> first = std::make_shared<String>("same");
    const std::shared_ptr<Object> second = std::make_shared<String>("same");
    CHECK(valuesEqual(*first, *second));
}

TEST_CASE("T3-SYM-6 valuesEqual compares Symbol values by identity") {
    const std::shared_ptr<Object> a = std::make_shared<Symbol>();
    const std::shared_ptr<Object> b = std::make_shared<Symbol>();
    CHECK(valuesEqual(*a, *a));
    CHECK_FALSE(valuesEqual(*a, *b)); // `a == b` is false, so `a != b` is true.
    CHECK_FALSE(valuesEqual(*a, Integer{1}));
    CHECK_FALSE(valuesEqual(*a, NullObject{}));
    CHECK_FALSE(valuesEqual(NullObject{}, *a));

    // A Symbol variable holding `a`, passed through a parameter and returned, is an owning copy
    // of the same instance.
    const std::shared_ptr<Object> variable = a;
    const std::shared_ptr<Object> parameter = variable;
    const std::shared_ptr<Object> returned = parameter;
    CHECK(valuesEqual(*returned, *a));
}

TEST_CASE("T3-EQ-4 valuesEqual composes left-associative chains through Boolean results") {
    // Each step feeds a new Boolean holding the previous result into the next comparison, as the
    // evaluator does for `((a == b) == c)`.
    SECTION("1 == 1 == true is true") {
        const Boolean step{valuesEqual(Integer{1}, Integer{1})};
        CHECK(valuesEqual(step, Boolean{true}));
    }
    SECTION("1 == 2 == false is true") {
        const Boolean step{valuesEqual(Integer{1}, Integer{2})};
        CHECK(valuesEqual(step, Boolean{false}));
    }
    SECTION("true != false != true is false") {
        const Boolean step{!valuesEqual(Boolean{true}, Boolean{false})};
        CHECK(step.value());
        CHECK_FALSE(!valuesEqual(step, Boolean{true}));
    }
    SECTION("1 == 1 == 1 is false (Boolean is not Integer)") {
        const Boolean step{valuesEqual(Integer{1}, Integer{1})};
        CHECK(step.value());
        CHECK_FALSE(valuesEqual(step, Integer{1}));
    }
}

TEST_CASE("T3-EQ valuesEqual is noexcept") {
    STATIC_CHECK(
        noexcept(valuesEqual(std::declval<const Object&>(), std::declval<const Object&>())));
}

} // namespace philios::test
