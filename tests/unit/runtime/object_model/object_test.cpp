// Unit tests for src/runtime/object_model/object.hpp (plan 0002 §3.4, D19; T2-TYPE-7, task 3).
//
// The draft's inheritance requirements, checked at compile time: Integer and String derive from
// ImmutableObject; ImmutableObject, NullObject, and Function derive from Object; NullObject and
// Function do not derive from ImmutableObject; Object is abstract. No object-model class is added
// or changed by plan 0002.

#include "runtime/object_model/object.hpp"

#include "runtime/object_model/function.hpp"
#include "runtime/object_model/integer.hpp"
#include "runtime/object_model/null_object.hpp"
#include "runtime/object_model/string_object.hpp"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

namespace philios::test {
namespace {

static_assert(std::is_base_of_v<ImmutableObject, Integer>);
static_assert(std::is_base_of_v<ImmutableObject, String>);

static_assert(std::is_base_of_v<Object, ImmutableObject>);
static_assert(std::is_base_of_v<Object, NullObject>);
static_assert(std::is_base_of_v<Object, Function>);

static_assert(!std::is_base_of_v<ImmutableObject, NullObject>);
static_assert(!std::is_base_of_v<ImmutableObject, Function>);

static_assert(std::is_abstract_v<Object>);

} // namespace

TEST_CASE("T2-TYPE-7 the object-model inheritance requirements hold") {
    // The static_asserts above are the test; this case records it in the test report.
    STATIC_CHECK(std::is_base_of_v<ImmutableObject, Integer>);
    STATIC_CHECK(std::is_base_of_v<ImmutableObject, String>);
    STATIC_CHECK(std::is_base_of_v<Object, ImmutableObject>);
    STATIC_CHECK(std::is_base_of_v<Object, NullObject>);
    STATIC_CHECK(std::is_base_of_v<Object, Function>);
    STATIC_CHECK_FALSE(std::is_base_of_v<ImmutableObject, NullObject>);
    STATIC_CHECK_FALSE(std::is_base_of_v<ImmutableObject, Function>);
    STATIC_CHECK(std::is_abstract_v<Object>);
}

} // namespace philios::test
