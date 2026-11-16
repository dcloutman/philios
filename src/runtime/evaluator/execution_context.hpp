#pragma once

/// @file
/// @brief The execution context: limits, depth counters with RAII guards, and the `null`
///        instance (plan 0001 §3.4, §3.7, §3.9, D7, D18, D24).
///
/// Created by the CLI entry point and passed explicitly by reference to Module::load and to every
/// reentrant function. Never global. Contains no test hooks.

#include "common/errors.hpp"
#include "runtime/object_model/null_object.hpp"

#include <cstddef>
#include <memory>

namespace philios {

/// @brief Per-execution state.
///
/// Not copyable or movable (guards hold references to it).
class ExecutionContext final {
public:
    /// @brief Parser nesting limit (blocks plus call argument lists, D7). Passed to the parser as
    ///        a value (§3.4).
    static constexpr std::size_t kMaxParseNesting = 256;
    /// @brief Maximum number of active Philios calls; `main` is depth 1 (D7).
    static constexpr std::size_t kMaxCallDepth = 1000;
    /// @brief Maximum number of nested ASTNode::evaluate entries (D24). Fixed, not configurable.
    static constexpr std::size_t kMaxEvalDepth = 10000;

    /// @brief Creates the context and its NullObject instance.
    /// @post callDepth() == 0, evaluationDepth() == 0, nullObject() is non-null.
    /// @throws std::bad_alloc on exhaustion.
    ExecutionContext();

    ExecutionContext(const ExecutionContext&) = delete;
    ExecutionContext& operator=(const ExecutionContext&) = delete;
    ExecutionContext(ExecutionContext&&) = delete;
    ExecutionContext& operator=(ExecutionContext&&) = delete;
    ~ExecutionContext() = default;

    /// @brief This context's single NullObject (D18). The same instance on every call; different
    ///        contexts have different instances.
    /// @throws Nothing.
    [[nodiscard]] const std::shared_ptr<NullObject>& nullObject() const noexcept;

    /// @brief Number of active Philios calls.
    /// @throws Nothing.
    [[nodiscard]] std::size_t callDepth() const noexcept;

    /// @brief Number of active ASTNode::evaluate entries.
    /// @throws Nothing.
    [[nodiscard]] std::size_t evaluationDepth() const noexcept;

private:
    friend class CallDepthGuard;
    friend class EvaluationDepthGuard;

    std::shared_ptr<NullObject> null_;
    std::size_t callDepth_{0};
    std::size_t evaluationDepth_{0};
};

/// @brief RAII guard for one active Philios call (D7).
///
/// Constructed before the callee's body starts. Not copyable or movable.
class CallDepthGuard final {
public:
    /// @param context Borrowed; must outlive the guard.
    /// @param calleeLocation Location of the callee identifier, used for the error.
    /// @post On success, `context.callDepth()` is one greater than before.
    /// @throws RuntimeError of kind RuntimeErrorKind::CallDepthExceeded at @p calleeLocation if
    ///         the call would make the depth exceed ExecutionContext::kMaxCallDepth; the depth is
    ///         then unchanged.
    CallDepthGuard(ExecutionContext& context, SourceLocation calleeLocation);

    /// @brief Restores the call depth on every exit path, including exceptions.
    /// @throws Nothing.
    ~CallDepthGuard();

    CallDepthGuard(const CallDepthGuard&) = delete;
    CallDepthGuard& operator=(const CallDepthGuard&) = delete;
    CallDepthGuard(CallDepthGuard&&) = delete;
    CallDepthGuard& operator=(CallDepthGuard&&) = delete;

private:
    ExecutionContext& context_;
};

/// @brief RAII guard for one ASTNode::evaluate entry (D24). Its check is the first action of
///        every evaluate.
///
/// Takes a SourceLocation rather than a node, so it can be tested without AST nodes (T-SEC-5).
/// Not copyable or movable.
class EvaluationDepthGuard final {
public:
    /// @param context Borrowed; must outlive the guard.
    /// @param location Location of the node being entered, used for the error.
    /// @post On success, `context.evaluationDepth()` is one greater than before.
    /// @throws RuntimeError of kind RuntimeErrorKind::EvaluationDepthExceeded at @p location, with
    ///         a message containing `evaluation nested too deeply (limit 10000)`, if the entry
    ///         would make the count exceed ExecutionContext::kMaxEvalDepth; the count is then
    ///         unchanged. (The call-depth precedence rule of §3.7 is applied by the CallExpr
    ///         evaluate, not here.)
    EvaluationDepthGuard(ExecutionContext& context, SourceLocation location);

    /// @brief Restores the evaluation depth on every exit path, including exceptions.
    /// @throws Nothing.
    ~EvaluationDepthGuard();

    EvaluationDepthGuard(const EvaluationDepthGuard&) = delete;
    EvaluationDepthGuard& operator=(const EvaluationDepthGuard&) = delete;
    EvaluationDepthGuard(EvaluationDepthGuard&&) = delete;
    EvaluationDepthGuard& operator=(EvaluationDepthGuard&&) = delete;

private:
    ExecutionContext& context_;
};

} // namespace philios
