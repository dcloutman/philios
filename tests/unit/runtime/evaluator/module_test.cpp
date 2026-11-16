// Unit tests for src/runtime/evaluator/module.hpp (plan 0001 §3.9, D3; T-OWN-1, T-OWN-2, and the
// module part of T-OWN-5, task 7).

#include "runtime/evaluator/module.hpp"
#include "common/errors.hpp"
#include "runtime/environment/environment.hpp"
#include "runtime/evaluator/execution_context.hpp"
#include "runtime/object_model/object.hpp"

#include "support/require_value.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace philios::test {
namespace {

static_assert(std::is_nothrow_destructible_v<Module>);
static_assert(noexcept(std::declval<Module&>().release()));
static_assert(!std::is_copy_constructible_v<Module>);
static_assert(!std::is_move_constructible_v<Module>);

constexpr const char* kTwoFunctions = "function f(): Integer:\n    return 1\n"
                                      "function main():\n    return f()\n";

/// Loads @p source and discards the module (for throw checks).
void loadAndDiscard(ExecutionContext& context, std::string source) {
    [[maybe_unused]] const std::unique_ptr<Module> module =
        Module::load(context, std::move(source));
}

} // namespace

TEST_CASE("T-OWN-1 Functions and the module environment expire after Module destruction") {
    ExecutionContext context;
    std::unique_ptr<Module> module = Module::load(context, kTwoFunctions);
    REQUIRE(module != nullptr);
    REQUIRE(module->environment() != nullptr);

    const std::weak_ptr<Environment> environmentObserver = module->environment();
    std::weak_ptr<Object> functionObserver;
    {
        const std::shared_ptr<Object> function = requireValue(module->environment()->lookup("f"));
        CHECK(function->objectType() == ObjectType::Function);
        functionObserver = function;
    }
    CHECK_FALSE(functionObserver.expired());

    module.reset();
    CHECK(functionObserver.expired());
    CHECK(environmentObserver.expired());
}

TEST_CASE("T-OWN-1 Module::release() breaks the cycle and is safe to call twice") {
    ExecutionContext context;
    std::unique_ptr<Module> module = Module::load(context, kTwoFunctions);
    REQUIRE(module != nullptr);
    const std::weak_ptr<Environment> environmentObserver = module->environment();
    std::weak_ptr<Object> functionObserver;
    {
        const std::shared_ptr<Object> function =
            requireValue(module->environment()->lookup("main"));
        functionObserver = function;
    }

    module->release();
    CHECK(functionObserver.expired());
    CHECK(environmentObserver.expired());
    module->release();
    module.reset();
}

TEST_CASE("T-OWN-2 a module that fails to load is not returned") {
    ExecutionContext context;
    SECTION("declaration error") {
        CHECK_THROWS_AS(loadAndDiscard(context, "function main():\n    return nope()\n"),
                        DeclarationError);
    }
    SECTION("syntax error") {
        CHECK_THROWS_AS(loadAndDiscard(context, "function main():\n"), SyntaxError);
    }
    SECTION("lexical error") {
        CHECK_THROWS_AS(loadAndDiscard(context, "function main():\n    return -1\n"),
                        LexicalError);
    }
}

TEST_CASE("T-OWN-5 a module environment holding Functions is destroyed cleanly") {
    ExecutionContext context;
    std::unique_ptr<Module> module =
        Module::load(context, "function a(): Integer:\n    return b()\n"
                              "function b(): Integer:\n    return a()\n"
                              "function main():\n    pass\n");
    REQUIRE(module != nullptr);
    module.reset();
    CHECK(module == nullptr);
}

} // namespace philios::test
