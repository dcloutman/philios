#include "runtime/evaluator/call.hpp"

#include "common/errors.hpp"
#include "frontend/ast/ast.hpp"
#include "runtime/environment/environment.hpp"
#include "runtime/evaluator/completion.hpp"
#include "runtime/evaluator/execution_context.hpp"
#include "runtime/evaluator/type_check.hpp"
#include "runtime/object_model/function.hpp"
#include "runtime/object_model/integer.hpp"
#include "runtime/object_model/object.hpp"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace philios {

std::shared_ptr<Object> callFunction(ExecutionContext& context, std::shared_ptr<Object> callee,
                                     std::vector<std::shared_ptr<Object>> arguments,
                                     const CallExpr& callSite) {
    const SourceLocation calleeLocation = callSite.callee().location();

    if (callee == nullptr) {
        throw InternalError("callFunction: null callee");
    }
    for (const std::shared_ptr<Object>& argument : arguments) {
        if (argument == nullptr) {
            throw InternalError("callFunction: null argument");
        }
    }

    const CallDepthGuard callDepthGuard(context, calleeLocation);

    if (callee->objectType() != ObjectType::Function) {
        throw RuntimeError(RuntimeErrorKind::NotCallable, calleeLocation, "callee is not callable");
    }

    const std::shared_ptr<Function> function = std::static_pointer_cast<Function>(std::move(callee));
    const std::shared_ptr<const FunctionDecl> declaration = function->declaration();
    const std::shared_ptr<Environment> closureEnvironment = function->closureEnv();
    if (declaration == nullptr || closureEnvironment == nullptr) {
        throw InternalError("callFunction: malformed Function object");
    }

    const std::vector<std::unique_ptr<Param>>& parameters = declaration->params();
    if (parameters.size() != arguments.size()) {
        throw RuntimeError(RuntimeErrorKind::ArgumentCount, calleeLocation,
                           "wrong number of arguments");
    }

    const std::vector<std::unique_ptr<ASTNode>>& callArguments = callSite.arguments();
    if (callArguments.size() != arguments.size()) {
        throw InternalError("callFunction: call-site argument list mismatch");
    }

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::unique_ptr<Param>& parameter = parameters[index];
        const std::unique_ptr<ASTNode>& argumentNode = callArguments[index];
        if (parameter == nullptr || argumentNode == nullptr) {
            throw InternalError("callFunction: null parameter or argument node");
        }
        if (!matchesDeclaredType(*arguments[index], parameter->type())) {
            throw RuntimeError(RuntimeErrorKind::ArgumentType, argumentNode->location(),
                               "argument type does not match parameter type");
        }
    }

    const std::shared_ptr<Environment> callEnvironment =
        std::make_shared<Environment>(closureEnvironment);
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::unique_ptr<Param>& parameter = parameters[index];
        if (parameter == nullptr) {
            throw InternalError("callFunction: null parameter");
        }
        callEnvironment->define(parameter->name(), arguments[index]);
    }

    const Completion completion = declaration->body().evaluate(context, callEnvironment);
    if (completion.value == nullptr) {
        throw InternalError("callFunction: null completion value");
    }

    const bool isMain = declaration->name() == std::string_view("main");

    std::shared_ptr<Object> result;
    // A return-type mismatch is reported at the executed `return` (§4). The fall-off-the-end
    // results below (main's Integer 0 and a null function's null) always match their type.
    SourceLocation returnLocation = declaration->location();
    if (completion.kind == CompletionKind::Return) {
        result = completion.value;
        returnLocation = completion.location;
    } else if (completion.kind == CompletionKind::Normal) {
        if (isMain) {
            result = std::make_shared<Integer>(0);
        } else {
            std::optional<DeclaredType> declaredType = declaration->returnType();
            if (!declaredType.has_value()) {
                throw InternalError("callFunction: non-main function has no return annotation");
            }
            // Only a `null` function returns `null` by falling off its end; a `?T` function
            // falling off its end is the missing-return error (plan 0002 D29).
            if (*declaredType == DeclaredType{TypeName::Null, false}) {
                result = context.nullObject();
            } else {
                throw RuntimeError(RuntimeErrorKind::MissingReturn, declaration->location(),
                                   "function ended without return");
            }
        }
    } else {
        throw InternalError("callFunction: invalid completion kind");
    }

    if (result == nullptr) {
        throw InternalError("callFunction: null result");
    }

    const DeclaredType declaredReturn = [&]() -> DeclaredType {
        if (isMain) {
            return DeclaredType{TypeName::Integer, false};
        }
        const std::optional<DeclaredType> declaredType = declaration->returnType();
        if (!declaredType.has_value()) {
            throw InternalError("callFunction: non-main function has no return annotation");
        }
        return *declaredType;
    }();

    if (!matchesDeclaredType(*result, declaredReturn)) {
        throw RuntimeError(RuntimeErrorKind::ReturnType, returnLocation,
                           "return value type does not match declaration");
    }

    return result;
}

} // namespace philios
