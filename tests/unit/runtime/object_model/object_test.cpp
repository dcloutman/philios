// Unit tests for src/runtime/object_model/object.hpp (plan 0002 §3.4, D19; T2-TYPE-7, task 3).
//
// The draft's inheritance requirements, checked at compile time: Integer and String derive from
// ImmutableObject; ImmutableObject, NullObject, and Function derive from Object; NullObject and
// Function do not derive from ImmutableObject; Object is abstract. No object-model class is added
// or changed by plan 0002.
//
// Plan 0003 task 4 (§4.3, §4.9, §4.14.2, §6.3) adds Boolean (immutable, compared by value; derives
// from ImmutableObject) and Symbol (pure identity with no state; derives from Object directly,
// like NullObject), with their ObjectType tags.

#include "runtime/object_model/object.hpp"

#include "runtime/object_model/boolean.hpp"
#include "runtime/object_model/function.hpp"
#include "runtime/object_model/integer.hpp"
#include "runtime/object_model/null_object.hpp"
#include "runtime/object_model/string_object.hpp"
#include "runtime/object_model/symbol.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <memory>
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

namespace {

// Plan 0003 task 4: inheritance, finality, and construction contracts of the new classes.
static_assert(std::is_base_of_v<ImmutableObject, Boolean>);
static_assert(std::is_base_of_v<Object, Symbol>);
static_assert(!std::is_base_of_v<ImmutableObject, Symbol>);
static_assert(std::is_final_v<Boolean>);
static_assert(std::is_final_v<Symbol>);

static_assert(std::is_nothrow_constructible_v<Boolean, bool>);
static_assert(!std::is_convertible_v<bool, Boolean>); // explicit
static_assert(!std::is_default_constructible_v<Boolean>);
static_assert(std::is_nothrow_default_constructible_v<Symbol>);

// Not copyable or movable, like Object (CERT OOP51).
template <typename T>
constexpr bool kNeitherCopyableNorMovable =
    !std::is_copy_constructible_v<T> && !std::is_copy_assignable_v<T> &&
    !std::is_move_constructible_v<T> && !std::is_move_assignable_v<T>;

static_assert(kNeitherCopyableNorMovable<Boolean>);
static_assert(kNeitherCopyableNorMovable<Symbol>);

// A Symbol is pure identity (§4.14.2): it carries no state beyond the Object base, so it is the
// same size as the stateless NullObject (no name, AST pointer, or source view).
static_assert(sizeof(Symbol) == sizeof(NullObject));

} // namespace

TEST_CASE("Plan 0003 Boolean and Symbol meet the object-model class requirements") {
    // The static_asserts above are the test; this case records it in the test report.
    STATIC_CHECK(std::is_base_of_v<ImmutableObject, Boolean>);
    STATIC_CHECK(std::is_base_of_v<Object, Symbol>);
    STATIC_CHECK_FALSE(std::is_base_of_v<ImmutableObject, Symbol>);
    STATIC_CHECK(kNeitherCopyableNorMovable<Boolean>);
    STATIC_CHECK(kNeitherCopyableNorMovable<Symbol>);
    STATIC_CHECK(sizeof(Symbol) == sizeof(NullObject));
}

TEST_CASE("Plan 0003 Boolean and Symbol report their object types") {
    SECTION("directly") {
        CHECK(Boolean(true).objectType() == ObjectType::Boolean);
        CHECK(Boolean(false).objectType() == ObjectType::Boolean);
        CHECK(Symbol().objectType() == ObjectType::Symbol);
    }

    SECTION("through an owning Object reference") {
        const std::shared_ptr<const Object> trueValue = std::make_shared<Boolean>(true);
        const std::shared_ptr<const Object> falseValue = std::make_shared<Boolean>(false);
        const std::shared_ptr<const Object> symbol = std::make_shared<Symbol>();
        CHECK(trueValue->objectType() == ObjectType::Boolean);
        CHECK(falseValue->objectType() == ObjectType::Boolean);
        CHECK(symbol->objectType() == ObjectType::Symbol);
    }

    SECTION("the new tags are distinct from every existing tag") {
        for (const ObjectType existing :
             {ObjectType::Integer, ObjectType::String, ObjectType::Null, ObjectType::Function}) {
            CHECK(existing != ObjectType::Boolean);
            CHECK(existing != ObjectType::Symbol);
        }
        CHECK(ObjectType::Boolean != ObjectType::Symbol);
    }
}

TEST_CASE("Plan 0003 Boolean holds the value it was constructed with") {
    const Boolean trueValue(true);
    const Boolean falseValue(false);
    CHECK(trueValue.value());
    CHECK_FALSE(falseValue.value());

    // Reading the value does not change it (immutable).
    CHECK(trueValue.value());
    CHECK_FALSE(falseValue.value());

    // Each construction is a separate object (no canonical instances); equal values stay equal.
    const auto first = std::make_shared<const Boolean>(true);
    const auto second = std::make_shared<const Boolean>(true);
    CHECK(first.get() != second.get());
    CHECK(first->value());
    CHECK(second->value());
}

TEST_CASE("Plan 0003 distinct Symbol objects are distinct instances") {
    const auto first = std::make_shared<const Symbol>();
    const auto second = std::make_shared<const Symbol>();
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    CHECK(first.get() != second.get());

    // An owning copy refers to the same symbol (identity is preserved by copies).
    const std::shared_ptr<const Object> copy = first;
    CHECK(copy.get() == first.get());
    CHECK(copy.get() != second.get());

    // Symbols are owned by the references that hold them (§6.3): an owning copy keeps the symbol
    // alive after the original reference is released, and it expires with the last owner.
    std::shared_ptr<const Symbol> original = std::make_shared<const Symbol>();
    const std::weak_ptr<const Symbol> observer = original;
    std::shared_ptr<const Object> holder = original;
    original.reset();
    CHECK_FALSE(observer.expired());
    CHECK(holder->objectType() == ObjectType::Symbol);
    holder.reset();
    CHECK(observer.expired());
}

TEST_CASE("Plan 0003 Integer::equals compares exact stored values") {
    CHECK(Integer{1}.equals(Integer{1}));
    CHECK_FALSE(Integer{1}.equals(Integer{2}));

    constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    CHECK(Integer{kMin}.equals(Integer{kMin}));
    CHECK(Integer{kMax}.equals(Integer{kMax}));
    CHECK_FALSE(Integer{kMin}.equals(Integer{0}));
    CHECK_FALSE(Integer{kMax}.equals(Integer{-1}));
}

} // namespace philios::test
