#pragma once

/// @file
/// @brief Test-only AST builders shared by several unit test files (plan 0001 §7.1, D44).

#include "common/errors.hpp"
#include "frontend/ast/ast.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace philios::test {

/// @brief Creates an Identifier node.
/// @param name Copied into the node, so it need not outlive the node (plan 0003 §4.14.2).
/// @throws std::bad_alloc.
[[nodiscard]] std::unique_ptr<Identifier> makeIdentifier(std::string_view name,
                                                         SourceLocation location = {});

/// @brief Creates an IntegerLiteral node.
/// @throws std::bad_alloc.
[[nodiscard]] std::unique_ptr<IntegerLiteral> makeIntegerLiteral(std::int64_t value,
                                                                 SourceLocation location = {});

/// @brief Creates `callee(arguments...)` whose callee identifier is at @p calleeLocation.
/// @param callee Copied into the callee Identifier, so it need not outlive the node.
/// @throws std::bad_alloc; InternalError if an argument is null.
[[nodiscard]] std::unique_ptr<CallExpr> makeCall(std::string_view callee,
                                                 std::vector<std::unique_ptr<ASTNode>> arguments,
                                                 SourceLocation calleeLocation = {});

/// @brief Collects move-only nodes into an argument vector (an initializer list cannot hold
///        `std::unique_ptr`).
/// @throws std::bad_alloc.
template <typename... Nodes>
[[nodiscard]] std::vector<std::unique_ptr<ASTNode>> argumentList(Nodes... nodes) {
    std::vector<std::unique_ptr<ASTNode>> arguments;
    arguments.reserve(sizeof...(nodes));
    (arguments.push_back(std::move(nodes)), ...);
    return arguments;
}

} // namespace philios::test
