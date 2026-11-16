// Unit tests for src/runtime/object_model/integer.hpp (plan 0001 §3.8, D20; T-RUN-4, task 3).

#include "runtime/object_model/integer.hpp"
#include "runtime/object_model/object.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>

namespace philios::test {

TEST_CASE("T-RUN-4 Integer exit-status conversion maps negatives and extremes") {
    CHECK(Integer(-1).exitStatus() == 255);
    CHECK(Integer(-255).exitStatus() == 1);
    CHECK(Integer(-256).exitStatus() == 0);
    CHECK(Integer(std::numeric_limits<std::int64_t>::min()).exitStatus() == 0);
    CHECK(Integer(std::numeric_limits<std::int64_t>::max()).exitStatus() == 255);
}

TEST_CASE("Integer exit status for the T-RUN-1 values 42, 256, 300 (unit supplement)") {
    CHECK(Integer(42).exitStatus() == 42);
    CHECK(Integer(256).exitStatus() == 0);
    CHECK(Integer(300).exitStatus() == 44);
    CHECK(Integer(0).exitStatus() == 0);
}

TEST_CASE("Integer reports its object type") {
    CHECK(Integer(7).objectType() == ObjectType::Integer);
}

} // namespace philios::test
