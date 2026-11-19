// Every ASTNode::evaluate definition for the concrete nodes declared in
// src/frontend/ast/ast.hpp (plan 0001 §3.9, D45; plan 0002 §3.5, §3.8 for EchoCall and
// TypeofCall).

#include "common/errors.hpp"
#include "frontend/ast/ast.hpp"
#include "runtime/environment/environment.hpp"
#include "runtime/evaluator/call.hpp"
#include "runtime/evaluator/completion.hpp"
#include "runtime/evaluator/execution_context.hpp"
#include "runtime/evaluator/type_check.hpp"
#include "runtime/object_model/integer.hpp"
#include "runtime/object_model/object.hpp"
#include "runtime/object_model/string_object.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace philios {

namespace {

// Native stack (plan §5, D24): kMaxEvalDepth bounds the number of nested evaluate() frames, so
// the stack T-SEC-4 needs is that count times the frame size of the evaluate() functions on the
// recursive path (CallExpr, Block, ReturnStmt, ExprStmt). Work that is not live across the nested
// evaluate() call is kept out of those frames: exceptions are built in the helpers below (each
// message std::string would otherwise get its own stack slot, and under AddressSanitizer its own
// redzones, in every nested frame), and CallExpr::evaluate hands callee evaluation and the call
// itself to helpers. Measured in plans/0001-implementation_notes.md (Pass D).

[[noreturn]] void throwInternal(std::string_view message) {
    throw InternalError(std::string(message));
}

[[noreturn]] void throwCallDepthExceeded(const CallExpr& callExpr) {
    throw RuntimeError(RuntimeErrorKind::CallDepthExceeded, callExpr.callee().location(),
                       "call depth exceeded");
}

/// Evaluates the callee identifier of a call to its value.
[[nodiscard]] std::shared_ptr<Object> evaluateCallee(const CallExpr& callExpr,
                                                     ExecutionContext& context,
                                                     const std::shared_ptr<Environment>& environment) {
    const Completion calleeCompletion = callExpr.callee().evaluate(context, environment);
    if (calleeCompletion.kind != CompletionKind::Normal) {
        throwInternal("CallExpr::evaluate: callee completion must be Normal");
    }
    if (calleeCompletion.value == nullptr) {
        throwInternal("CallExpr::evaluate: null callee value");
    }
    return calleeCompletion.value;
}

/// Runs the call sequence on already evaluated operands. Takes rvalue references so that the
/// by-value arguments of callFunction are constructed in this frame, not in CallExpr::evaluate's.
[[nodiscard]] Completion completeCall(ExecutionContext& context,
                                      std::shared_ptr<Object>&& calleeValue,
                                      std::vector<std::shared_ptr<Object>>&& argumentValues,
                                      const CallExpr& callSite) {
    return Completion{CompletionKind::Normal,
                      callFunction(context, std::move(calleeValue), std::move(argumentValues),
                                   callSite)};
}

} // namespace

Completion Identifier::evaluate(ExecutionContext& context,
                                const std::shared_ptr<Environment>& environment) const {
    const EvaluationDepthGuard depthGuard(context, location());
    if (environment == nullptr) {
        throw InternalError("Identifier::evaluate: null environment");
    }

    const std::optional<std::shared_ptr<Object>> value = environment->lookup(name());
    if (!value.has_value()) {
        throw RuntimeError(RuntimeErrorKind::UndefinedIdentifier, location(), "undefined identifier");
    }
    if (*value == nullptr) {
        throw InternalError("Identifier::evaluate: null bound value");
    }

    return Completion{CompletionKind::Normal, *value};
}

Completion IntegerLiteral::evaluate(ExecutionContext& context,
                                    const std::shared_ptr<Environment>& environment) const {
    const EvaluationDepthGuard depthGuard(context, location());
    if (environment == nullptr) {
        throw InternalError("IntegerLiteral::evaluate: null environment");
    }

    return Completion{CompletionKind::Normal, std::make_shared<Integer>(value())};
}

Completion StringLiteral::evaluate(ExecutionContext& context,
                                   const std::shared_ptr<Environment>& environment) const {
    const EvaluationDepthGuard depthGuard(context, location());
    if (environment == nullptr) {
        throw InternalError("StringLiteral::evaluate: null environment");
    }

    return Completion{CompletionKind::Normal, std::make_shared<String>(value())};
}

Completion NullLiteral::evaluate(ExecutionContext& context,
                                 const std::shared_ptr<Environment>& environment) const {
    const EvaluationDepthGuard depthGuard(context, location());
    if (environment == nullptr) {
        throw InternalError("NullLiteral::evaluate: null environment");
    }

    return Completion{CompletionKind::Normal, context.nullObject()};
}

Completion CallExpr::evaluate(ExecutionContext& context,
                              const std::shared_ptr<Environment>& environment) const {
    if (context.callDepth() >= ExecutionContext::kMaxCallDepth &&
        context.evaluationDepth() >= ExecutionContext::kMaxEvalDepth) {
        throwCallDepthExceeded(*this);
    }

    const EvaluationDepthGuard depthGuard(context, location());
    if (environment == nullptr) {
        throwInternal("CallExpr::evaluate: null environment");
    }

    // Order (§3.7, D6): the callee first, then the arguments left to right, then the call.
    std::shared_ptr<Object> calleeValue = evaluateCallee(*this, context, environment);

    std::vector<std::shared_ptr<Object>> argumentValues;
    argumentValues.reserve(arguments().size());
    for (const std::unique_ptr<ASTNode>& argument : arguments()) {
        if (!argument) {
            throwInternal("CallExpr::evaluate: null argument");
        }

        const Completion argumentCompletion = argument->evaluate(context, environment);
        if (argumentCompletion.kind != CompletionKind::Normal) {
            throwInternal("CallExpr::evaluate: argument completion must be Normal");
        }
        if (argumentCompletion.value == nullptr) {
            throwInternal("CallExpr::evaluate: null argument value");
        }
        argumentValues.push_back(argumentCompletion.value);
    }

    return completeCall(context, std::move(calleeValue), std::move(argumentValues), *this);
}

Completion ExprStmt::evaluate(ExecutionContext& context,
                              const std::shared_ptr<Environment>& environment) const {
    const EvaluationDepthGuard depthGuard(context, location());
    if (environment == nullptr) {
        throwInternal("ExprStmt::evaluate: null environment");
    }

    [[maybe_unused]] const Completion completion = expression().evaluate(context, environment);
    return Completion{CompletionKind::Normal, context.nullObject()};
}

Completion EchoCall::evaluate(ExecutionContext& context,
                              const std::shared_ptr<Environment>& environment) const {
    const EvaluationDepthGuard depthGuard(context, location());
    if (environment == nullptr) {
        throwInternal("EchoCall::evaluate: null environment");
    }

    const Completion argumentCompletion = argument().evaluate(context, environment);
    if (argumentCompletion.kind != CompletionKind::Normal) {
        throwInternal("EchoCall::evaluate: argument completion must be Normal");
    }
    if (argumentCompletion.value == nullptr) {
        throwInternal("EchoCall::evaluate: null argument value");
    }

    std::shared_ptr<Object> argumentValue = argumentCompletion.value;
    if (argumentValue->objectType() != ObjectType::String) {
        throw RuntimeError(RuntimeErrorKind::ArgumentType, argument().location(),
                           "argument type does not match parameter type");
    }

    const auto& stringValue = static_cast<const String&>(*argumentValue);
    const std::error_code writeResult = context.writeOutput(stringValue.bytes());
    if (writeResult) {
        throw RuntimeError(RuntimeErrorKind::OutputWrite, location(),
                           "could not write to standard output: " + writeResult.message());
    }

    return Completion{CompletionKind::Normal, context.nullObject()};
}

Completion TypeofCall::evaluate(ExecutionContext& context,
                                const std::shared_ptr<Environment>& environment) const {
    const EvaluationDepthGuard depthGuard(context, location());
    if (environment == nullptr) {
        throwInternal("TypeofCall::evaluate: null environment");
    }

    const Completion argumentCompletion = argument().evaluate(context, environment);
    if (argumentCompletion.kind != CompletionKind::Normal) {
        throwInternal("TypeofCall::evaluate: argument completion must be Normal");
    }
    if (argumentCompletion.value == nullptr) {
        throwInternal("TypeofCall::evaluate: null argument value");
    }

    std::shared_ptr<Object> argumentValue = argumentCompletion.value;
    return Completion{CompletionKind::Normal,
                      std::make_shared<String>(std::string(scriptTypeName(*argumentValue)))};
}

Completion ReturnStmt::evaluate(ExecutionContext& context,
                                const std::shared_ptr<Environment>& environment) const {
    const EvaluationDepthGuard depthGuard(context, location());
    if (environment == nullptr) {
        throwInternal("ReturnStmt::evaluate: null environment");
    }

    if (value() == nullptr) {
        return Completion{CompletionKind::Return, context.nullObject(), location()};
    }

    const Completion valueCompletion = value()->evaluate(context, environment);
    if (valueCompletion.kind != CompletionKind::Normal) {
        throwInternal("ReturnStmt::evaluate: return value completion must be Normal");
    }
    if (valueCompletion.value == nullptr) {
        throwInternal("ReturnStmt::evaluate: null return value");
    }
    return Completion{CompletionKind::Return, valueCompletion.value, location()};
}

Completion PassStmt::evaluate(ExecutionContext& context,
                              const std::shared_ptr<Environment>& environment) const {
    const EvaluationDepthGuard depthGuard(context, location());
    if (environment == nullptr) {
        throw InternalError("PassStmt::evaluate: null environment");
    }

    return Completion{CompletionKind::Normal, context.nullObject()};
}

Completion Block::evaluate(ExecutionContext& context,
                           const std::shared_ptr<Environment>& environment) const {
    const EvaluationDepthGuard depthGuard(context, location());
    if (environment == nullptr) {
        throwInternal("Block::evaluate: null environment");
    }

    for (const std::unique_ptr<ASTNode>& statement : statements()) {
        if (!statement) {
            throwInternal("Block::evaluate: null statement");
        }

        const Completion completion = statement->evaluate(context, environment);
        if (completion.value == nullptr) {
            throwInternal("Block::evaluate: null completion value");
        }
        if (completion.kind == CompletionKind::Return) {
            return completion;
        }
        if (completion.kind != CompletionKind::Normal) {
            throwInternal("Block::evaluate: invalid completion kind");
        }
    }

    return Completion{CompletionKind::Normal, context.nullObject()};
}

Completion Param::evaluate(ExecutionContext& context,
                           const std::shared_ptr<Environment>& environment) const {
    const EvaluationDepthGuard depthGuard(context, location());
    if (environment == nullptr) {
        throw InternalError("Param::evaluate: null environment");
    }

    throw InternalError("Param::evaluate must not be called");
}

Completion FunctionDecl::evaluate(ExecutionContext& context,
                                  const std::shared_ptr<Environment>& environment) const {
    const EvaluationDepthGuard depthGuard(context, location());
    if (environment == nullptr) {
        throw InternalError("FunctionDecl::evaluate: null environment");
    }

    throw InternalError("FunctionDecl::evaluate must not be called");
}

} // namespace philios
