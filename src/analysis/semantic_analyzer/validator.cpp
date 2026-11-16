#include "analysis/semantic_analyzer/validator.hpp"

#include "common/errors.hpp"
#include "frontend/ast/ast.hpp"

#include <memory>
#include <optional>
#include <set>
#include <string_view>

namespace philios {
namespace {

struct FunctionContext {
    std::string_view name{};
    TypeName declaredReturn{TypeName::Null};
    bool isMain{false};
    std::set<std::string_view, std::less<>> parameters;
    const std::set<std::string_view, std::less<>>* functionNames{nullptr};
};

[[nodiscard]] TypeName declaredReturnType(const FunctionDecl& function) {
    if (function.name() == "main") {
        return TypeName::Integer;
    }
    const std::optional<TypeName> returnType = function.returnType();
    if (!returnType.has_value()) {
        throw DeclarationError(function.location(), "function return type is required");
    }
    return *returnType;
}

void validateExpression(const ASTNode& expression, const FunctionContext& context);

void validateIdentifier(const Identifier& identifier, const FunctionContext& context) {
    const std::string_view name = identifier.name();
    if (name == "main") {
        throw DeclarationError(identifier.location(), "main may be used only as its declaration name");
    }
    if (context.parameters.contains(name)) {
        return;
    }
    if (context.functionNames->contains(name)) {
        return;
    }
    throw DeclarationError(identifier.location(), "undefined identifier");
}

void validateCall(const CallExpr& call, const FunctionContext& context) {
    validateIdentifier(call.callee(), context);
    for (const auto& argument : call.arguments()) {
        if (!argument) {
            throw InternalError("Validator: null call argument");
        }
        validateExpression(*argument, context);
    }
}

void validateExpression(const ASTNode& expression, const FunctionContext& context) {
    switch (expression.kind()) {
    case NodeKind::IntegerLiteral:
    case NodeKind::StringLiteral:
    case NodeKind::NullLiteral:
        return;
    case NodeKind::Identifier:
        validateIdentifier(static_cast<const Identifier&>(expression), context);
        return;
    case NodeKind::CallExpr:
        validateCall(static_cast<const CallExpr&>(expression), context);
        return;
    case NodeKind::FunctionDecl:
    case NodeKind::Param:
    case NodeKind::Block:
    case NodeKind::ReturnStmt:
    case NodeKind::PassStmt:
    case NodeKind::ExprStmt:
        break;
    }
    throw InternalError("Validator: invalid expression node kind");
}

void validateStatements(const Block& block, const FunctionContext& context) {
    bool sawReturn = false;
    for (const auto& statement : block.statements()) {
        if (!statement) {
            throw InternalError("Validator: null statement");
        }
        if (sawReturn) {
            throw DeclarationError(statement->location(), "statement after return");
        }

        switch (statement->kind()) {
        case NodeKind::ReturnStmt: {
            const auto& returnStmt = static_cast<const ReturnStmt&>(*statement);
            const ASTNode* value = returnStmt.value();
            if (value == nullptr) {
                if (context.isMain || context.declaredReturn != TypeName::Null) {
                    throw DeclarationError(returnStmt.location(),
                                           "bare return is not allowed in this function");
                }
            } else {
                validateExpression(*value, context);
            }
            sawReturn = true;
            break;
        }
        case NodeKind::PassStmt:
            break;
        case NodeKind::ExprStmt: {
            const auto& exprStmt = static_cast<const ExprStmt&>(*statement);
            validateCall(exprStmt.call(), context);
            break;
        }
        case NodeKind::FunctionDecl:
        case NodeKind::Param:
        case NodeKind::Block:
        case NodeKind::CallExpr:
        case NodeKind::IntegerLiteral:
        case NodeKind::StringLiteral:
        case NodeKind::NullLiteral:
        case NodeKind::Identifier:
            throw InternalError("Validator: invalid statement node kind");
        }
    }
}

} // namespace

void validateProgram(const Program& program) {
    std::set<std::string_view, std::less<>> functionNames;
    std::optional<SourceLocation> mainLocation;

    for (const auto& function : program) {
        if (!function) {
            throw InternalError("Validator: null function declaration");
        }
        const std::string_view name = function->name();
        if (functionNames.contains(name)) {
            throw DeclarationError(function->location(), "duplicate function declaration");
        }
        functionNames.insert(name);

        if (name == "main") {
            if (mainLocation.has_value()) {
                throw DeclarationError(function->location(), "duplicate function declaration");
            }
            mainLocation = function->location();
        }
    }

    if (!mainLocation.has_value()) {
        throw DeclarationError(SourceLocation{1, 1}, "missing main function");
    }

    for (const auto& function : program) {
        const bool isMain = function->name() == "main";

        if (isMain) {
            if (!function->params().empty()) {
                throw DeclarationError(function->location(), "main must have no parameters");
            }
            if (function->returnType().has_value() && *function->returnType() != TypeName::Integer) {
                throw DeclarationError(function->location(), "main return type must be Integer");
            }
        } else if (!function->returnType().has_value()) {
            throw DeclarationError(function->location(), "function return type is required");
        }

        std::set<std::string_view, std::less<>> parameterNames;
        for (const auto& parameter : function->params()) {
            if (!parameter) {
                throw InternalError("Validator: null parameter");
            }
            const std::string_view name = parameter->name();
            if (parameterNames.contains(name)) {
                throw DeclarationError(parameter->location(), "duplicate parameter");
            }
            if (functionNames.contains(name)) {
                throw DeclarationError(parameter->location(),
                                       "parameter name must not match a module function");
            }
            parameterNames.insert(name);
        }

        FunctionContext context;
        context.name = function->name();
        context.declaredReturn = declaredReturnType(*function);
        context.isMain = isMain;
        context.parameters = std::move(parameterNames);
        context.functionNames = &functionNames;

        validateStatements(function->body(), context);
    }
}

} // namespace philios
