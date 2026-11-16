// Unit tests for src/frontend/lexer/integer_literal.hpp (plan 0001 §3.3; T-LEX-8 unit, task 5).

#include "frontend/lexer/integer_literal.hpp"

#include "support/require_value.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace philios::test {

TEST_CASE("T-LEX-8 parseDecimalInteger accepts the int64 range and rejects overflow") {
    const std::int64_t maximum = requireValue(parseDecimalInteger("9223372036854775807"));
    CHECK(maximum == INT64_MAX);

    const std::int64_t zero = requireValue(parseDecimalInteger("0"));
    CHECK(zero == 0);

    CHECK_FALSE(parseDecimalInteger("9223372036854775808").has_value());
    CHECK_FALSE(parseDecimalInteger(std::string(1000, '9')).has_value());
}

} // namespace philios::test
