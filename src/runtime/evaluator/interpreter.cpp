#include "runtime/evaluator/interpreter.hpp"

#include "common/errors.hpp"
#include "frontend/ast/ast.hpp"
#include "runtime/environment/environment.hpp"
#include "runtime/evaluator/call.hpp"
#include "runtime/evaluator/execution_context.hpp"
#include "runtime/evaluator/module.hpp"
#include "runtime/object_model/function.hpp"
#include "runtime/object_model/integer.hpp"
#include "runtime/object_model/object.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace philios {

int runMain(ExecutionContext& context, const Module& module) {
    const std::shared_ptr<Environment> environment = module.environment();
    if (environment == nullptr) {
        throw InternalError("runMain: module environment is null");
    }

    const std::optional<std::shared_ptr<Object>> mainBinding = environment->lookup("main");
    if (!mainBinding.has_value() || *mainBinding == nullptr) {
        throw InternalError("runMain: main is not bound");
    }
    if ((*mainBinding)->objectType() != ObjectType::Function) {
        throw InternalError("runMain: main binding is not a Function");
    }

    const std::shared_ptr<Function> mainFunction =
        std::static_pointer_cast<Function>(*mainBinding);
    const std::shared_ptr<const FunctionDecl> declaration = mainFunction->declaration();
    if (declaration == nullptr) {
        throw InternalError("runMain: main Function has no declaration");
    }

    CallExpr mainCall(std::make_unique<Identifier>(declaration->location(), "main"),
                      std::vector<std::unique_ptr<ASTNode>>{});
    const std::shared_ptr<Object> result =
        callFunction(context, *mainBinding, std::vector<std::shared_ptr<Object>>{}, mainCall);

    if (result == nullptr) {
        throw InternalError("runMain: null result");
    }
    // callFunction already reported any non-Integer result of `main` at the executed `return`
    // (§4), so a non-Integer here is an invariant failure, not a second, differently located
    // return-type error.
    if (result->objectType() != ObjectType::Integer) {
        throw InternalError("runMain: main returned a non-Integer after the return-type check");
    }

    return static_cast<const Integer&>(*result).exitStatus();
}

} // namespace philios
