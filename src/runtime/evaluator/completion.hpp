#pragma once

/// @file
/// @brief The result of evaluating an AST node (plan 0001 §3.9, D5).

#include "common/errors.hpp"
#include "runtime/object_model/object.hpp"

#include <memory>

namespace philios {

/// @brief How evaluation of a node ended.
enum class CompletionKind {
    Normal, ///< Continue with the next statement; expressions always complete Normal.
    Return, ///< A `return` executed; the enclosing block stops and the call returns.
};

/// @brief The value and kind produced by ASTNode::evaluate. No exceptions are used for control
///        flow (D5).
///
/// Plain aggregate. Invariant for every Completion returned by evaluate: `value` is non-null
/// (Philios `null` is the context's NullObject, never `nullptr`). Shares ownership of the value.
/// For a Return completion, `location` is the location of the executed `return` statement, where
/// the call sequence reports a return-type mismatch (§4); it is unused for Normal completions.
struct Completion {
    CompletionKind kind{CompletionKind::Normal}; ///< Normal or Return.
    std::shared_ptr<Object> value{};             ///< The produced value; non-null when returned.
    SourceLocation location{};                   ///< The `return` statement, for Return only.
};

} // namespace philios
