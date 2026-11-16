// Unit tests for src/runtime/environment/environment.hpp (plan 0001 §3.6, §3.9, D15, D38;
// T-DECL-9 and the environment parts of T-OWN-4 and T-OWN-5, task 7).

#include "runtime/environment/environment.hpp"
#include "runtime/object_model/integer.hpp"
#include "runtime/object_model/object.hpp"

#include "support/require_value.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace philios::test {
namespace {

static_assert(std::is_nothrow_destructible_v<Environment>);
static_assert(noexcept(std::declval<Environment&>().releaseBindings()));

} // namespace

TEST_CASE("Environment define and lookup return an owning copy of the bound value") {
    const auto environment = std::make_shared<Environment>(nullptr);
    const std::shared_ptr<Object> value = std::make_shared<Integer>(1);
    environment->define("x", value);
    const std::shared_ptr<Object> found = requireValue(environment->lookup("x"));
    CHECK(found == value);
    CHECK_FALSE(environment->lookup("y").has_value());
}

TEST_CASE("T-DECL-9 a local binding is consulted before the module binding") {
    const auto moduleEnvironment = std::make_shared<Environment>(nullptr);
    const std::shared_ptr<Object> moduleValue = std::make_shared<Integer>(1);
    const std::shared_ptr<Object> localValue = std::make_shared<Integer>(2);
    const std::shared_ptr<Object> onlyInModule = std::make_shared<Integer>(3);
    moduleEnvironment->define("x", moduleValue);
    moduleEnvironment->define("m", onlyInModule);

    const auto localEnvironment = std::make_shared<Environment>(moduleEnvironment);
    localEnvironment->define("x", localValue);

    const std::shared_ptr<Object> x = requireValue(localEnvironment->lookup("x"));
    CHECK(x == localValue);
    const std::shared_ptr<Object> m = requireValue(localEnvironment->lookup("m"));
    CHECK(m == onlyInModule);
    CHECK_FALSE(localEnvironment->lookup("absent").has_value());
}

TEST_CASE("releaseBindings removes every binding and is safe to call twice") {
    const auto environment = std::make_shared<Environment>(nullptr);
    const std::weak_ptr<Object> observer = [&environment] {
        const std::shared_ptr<Object> value = std::make_shared<Integer>(5);
        environment->define("v", value);
        return std::weak_ptr<Object>(value);
    }();
    CHECK_FALSE(observer.expired());
    environment->releaseBindings();
    CHECK(observer.expired());
    CHECK_FALSE(environment->lookup("v").has_value());
    environment->releaseBindings();
}

TEST_CASE("T-OWN-4 a chain of 200,000 environments is released without recursion") {
    constexpr std::size_t kChainLength = 200'000;
    auto environment = std::make_shared<Environment>(nullptr);
    for (std::size_t link = 0; link < kChainLength; ++link) {
        environment = std::make_shared<Environment>(std::move(environment));
    }
    environment.reset();
    CHECK(environment == nullptr);
}

} // namespace philios::test
