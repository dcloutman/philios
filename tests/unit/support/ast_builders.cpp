#include "support/ast_builders.hpp"

#include "common/errors.hpp"
#include "frontend/ast/ast.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace philios::test {

std::unique_ptr<Identifier> makeIdentifier(std::string_view name, SourceLocation location) {
    return std::make_unique<Identifier>(location, name);
}

std::unique_ptr<IntegerLiteral> makeIntegerLiteral(std::int64_t value, SourceLocation location) {
    return std::make_unique<IntegerLiteral>(location, value);
}

std::unique_ptr<CallExpr> makeCall(std::string_view callee,
                                   std::vector<std::unique_ptr<ASTNode>> arguments,
                                   SourceLocation calleeLocation) {
    return std::make_unique<CallExpr>(makeIdentifier(callee, calleeLocation), std::move(arguments));
}

} // namespace philios::test
