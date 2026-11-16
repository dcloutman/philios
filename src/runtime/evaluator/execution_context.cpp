#include "runtime/evaluator/execution_context.hpp"

#include "common/errors.hpp"
#include "runtime/object_model/null_object.hpp"

#include <cstddef>
#include <memory>
#include <string>

namespace philios {

ExecutionContext::ExecutionContext() : null_(std::make_shared<NullObject>()) {}

const std::shared_ptr<NullObject>& ExecutionContext::nullObject() const noexcept {
    return null_;
}

std::size_t ExecutionContext::callDepth() const noexcept {
    return callDepth_;
}

std::size_t ExecutionContext::evaluationDepth() const noexcept {
    return evaluationDepth_;
}

CallDepthGuard::CallDepthGuard(ExecutionContext& context, SourceLocation calleeLocation)
    : context_(context) {
    if (context_.callDepth_ >= ExecutionContext::kMaxCallDepth) {
        throw RuntimeError(RuntimeErrorKind::CallDepthExceeded, calleeLocation,
                           "call depth exceeded");
    }
    ++context_.callDepth_;
}

CallDepthGuard::~CallDepthGuard() {
    if (context_.callDepth_ > 0) {
        --context_.callDepth_;
    }
}

EvaluationDepthGuard::EvaluationDepthGuard(ExecutionContext& context, SourceLocation location)
    : context_(context) {
    if (context_.evaluationDepth_ >= ExecutionContext::kMaxEvalDepth) {
        throw RuntimeError(RuntimeErrorKind::EvaluationDepthExceeded, location,
                           "evaluation nested too deeply (limit 10000)");
    }
    ++context_.evaluationDepth_;
}

EvaluationDepthGuard::~EvaluationDepthGuard() {
    if (context_.evaluationDepth_ > 0) {
        --context_.evaluationDepth_;
    }
}

} // namespace philios
