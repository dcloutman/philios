// Unit tests for src/analysis/semantic_analyzer/validator.hpp (plan 0001 §3.5; task 7).
//
// Per §8, the error cases of T-DECL-3, -4, -6, -7 (and T-DECL-1, -2, -5) are CLI golden tests;
// this file checks that the accepted cases validate.

#include "analysis/semantic_analyzer/validator.hpp"
#include "frontend/ast/ast.hpp"
#include "support/source_pipeline.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

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

} // namespace philios::test
