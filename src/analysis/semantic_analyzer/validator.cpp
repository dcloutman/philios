#include "analysis/semantic_analyzer/validator.hpp"

#include "common/errors.hpp"
#include "frontend/ast/ast.hpp"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>

namespace philios {
namespace {

constexpr std::string_view kInitializerRequired = "initializer required";
constexpr std::string_view kDuplicateDeclaration = "duplicate declaration";
constexpr std::string_view kModuleLevelName = "name must not match a module-level declaration";
constexpr std::string_view kUsedBeforeDeclaration = "used before declaration";
constexpr std::string_view kNotAllowedInModuleInitializer =
    "not allowed in a module-level initializer";
constexpr std::string_view kUndefinedIdentifier = "undefined identifier";
constexpr std::string_view kMainName = "main may be used only as its declaration name";
constexpr std::string_view kCannotAssignName = "cannot assign to this name";
constexpr std::string_view kCannotAssignConstant = "cannot assign to constant";

struct ModuleNames {
    std::set<std::string_view, std::less<>> all;
    std::set<std::string_view, std::less<>> functions;
    std::set<std::string_view, std::less<>> constants;
    std::set<std::string_view, std::less<>> symbols;
};

enum class LocalNameKind {
    Parameter,
    Variable,
    Constant,
    Symbol,
};

struct LocalConstantState {
    bool nullable{false};
    bool assigned{false};
};

struct FunctionContext {
    DeclaredType declaredReturn{TypeName::Null, false};
    bool isMain{false};
    const ModuleNames* moduleNames{nullptr};
    std::set<std::string_view, std::less<>> allScopeNames;
    std::set<std::string_view, std::less<>> visibleNames;
    std::map<std::string_view, LocalNameKind, std::less<>> localKinds;
    std::map<std::string_view, LocalConstantState, std::less<>> localConstants;
};

[[nodiscard]] DeclaredType declaredReturnType(const FunctionDecl& function) {
    if (function.name() == "main") {
        return DeclaredType{TypeName::Integer, false};
    }
    const std::optional<DeclaredType> returnType = function.returnType();
    if (!returnType.has_value()) {
        throw DeclarationError(function.location(), "function return type is required");
    }
    return *returnType;
}

[[nodiscard]] SourceLocation moduleItemNameLocation(const ASTNode& item) {
    switch (item.kind()) {
    case NodeKind::FunctionDecl:
        return static_cast<const FunctionDecl&>(item).location();
    case NodeKind::ConstDecl:
        return static_cast<const ConstDecl&>(item).nameLocation();
    case NodeKind::SymbolDecl:
        return item.location();
    case NodeKind::Param:
    case NodeKind::Block:
    case NodeKind::ReturnStmt:
    case NodeKind::PassStmt:
    case NodeKind::ExprStmt:
    case NodeKind::CallExpr:
    case NodeKind::IntegerLiteral:
    case NodeKind::StringLiteral:
    case NodeKind::NullLiteral:
    case NodeKind::Identifier:
    case NodeKind::EchoCall:
    case NodeKind::TypeofCall:
    case NodeKind::LetDecl:
    case NodeKind::AssignStmt:
    case NodeKind::BooleanLiteral:
    case NodeKind::UnaryExpr:
    case NodeKind::BinaryExpr:
        throw InternalError("Validator: invalid module item node kind");
    }
    throw InternalError("Validator: invalid module item node kind");
}

[[nodiscard]] std::string_view moduleItemName(const ASTNode& item) {
    switch (item.kind()) {
    case NodeKind::FunctionDecl:
        return static_cast<const FunctionDecl&>(item).name();
    case NodeKind::ConstDecl:
        return static_cast<const ConstDecl&>(item).name();
    case NodeKind::SymbolDecl:
        return static_cast<const SymbolDecl&>(item).name();
    case NodeKind::Param:
    case NodeKind::Block:
    case NodeKind::ReturnStmt:
    case NodeKind::PassStmt:
    case NodeKind::ExprStmt:
    case NodeKind::CallExpr:
    case NodeKind::IntegerLiteral:
    case NodeKind::StringLiteral:
    case NodeKind::NullLiteral:
    case NodeKind::Identifier:
    case NodeKind::EchoCall:
    case NodeKind::TypeofCall:
    case NodeKind::LetDecl:
    case NodeKind::AssignStmt:
    case NodeKind::BooleanLiteral:
    case NodeKind::UnaryExpr:
    case NodeKind::BinaryExpr:
        throw InternalError("Validator: invalid module item node kind");
    }
    throw InternalError("Validator: invalid module item node kind");
}

void collectModuleNames(const Program& program, ModuleNames& names) {
    for (const auto& item : program) {
        if (!item) {
            throw InternalError("Validator: null module item");
        }
        const SourceLocation location = moduleItemNameLocation(*item);
        const std::string_view name = moduleItemName(*item);

        if (name == "main" && item->kind() != NodeKind::FunctionDecl) {
            throw DeclarationError(location, std::string(kMainName));
        }
        if (names.all.contains(name)) {
            throw DeclarationError(location, std::string(kDuplicateDeclaration));
        }
        names.all.insert(name);

        switch (item->kind()) {
        case NodeKind::FunctionDecl:
            names.functions.insert(name);
            break;
        case NodeKind::ConstDecl:
            names.constants.insert(name);
            break;
        case NodeKind::SymbolDecl:
            names.symbols.insert(name);
            break;
        case NodeKind::Param:
        case NodeKind::Block:
        case NodeKind::ReturnStmt:
        case NodeKind::PassStmt:
        case NodeKind::ExprStmt:
        case NodeKind::CallExpr:
        case NodeKind::IntegerLiteral:
        case NodeKind::StringLiteral:
        case NodeKind::NullLiteral:
        case NodeKind::Identifier:
        case NodeKind::EchoCall:
        case NodeKind::TypeofCall:
        case NodeKind::LetDecl:
        case NodeKind::AssignStmt:
        case NodeKind::BooleanLiteral:
        case NodeKind::UnaryExpr:
        case NodeKind::BinaryExpr:
            throw InternalError("Validator: invalid module item node kind");
        }
    }
}

void checkFunctionScopeName(std::string_view name, SourceLocation location,
                            std::set<std::string_view, std::less<>>& seen,
                            const ModuleNames& moduleNames) {
    if (name == "main") {
        throw DeclarationError(location, std::string(kMainName));
    }
    if (seen.contains(name)) {
        throw DeclarationError(location, std::string(kDuplicateDeclaration));
    }
    if (moduleNames.all.contains(name)) {
        throw DeclarationError(location, std::string(kModuleLevelName));
    }
    seen.insert(name);
}

void collectFunctionScopeNames(const FunctionDecl& function, const ModuleNames& moduleNames,
                               FunctionContext& context) {
    std::set<std::string_view, std::less<>> seen;
    for (const auto& parameter : function.params()) {
        if (!parameter) {
            throw InternalError("Validator: null parameter");
        }
        checkFunctionScopeName(parameter->name(), parameter->location(), seen, moduleNames);
        context.allScopeNames.insert(parameter->name());
        context.visibleNames.insert(parameter->name());
        context.localKinds.insert({parameter->name(), LocalNameKind::Parameter});
    }

    for (const auto& statement : function.body().statements()) {
        if (!statement) {
            throw InternalError("Validator: null statement");
        }

        switch (statement->kind()) {
        case NodeKind::LetDecl: {
            const auto& declaration = static_cast<const LetDecl&>(*statement);
            checkFunctionScopeName(declaration.name(), declaration.nameLocation(), seen,
                                   moduleNames);
            context.allScopeNames.insert(declaration.name());
            break;
        }
        case NodeKind::ConstDecl: {
            const auto& declaration = static_cast<const ConstDecl&>(*statement);
            checkFunctionScopeName(declaration.name(), declaration.nameLocation(), seen,
                                   moduleNames);
            context.allScopeNames.insert(declaration.name());
            break;
        }
        case NodeKind::SymbolDecl: {
            const auto& declaration = static_cast<const SymbolDecl&>(*statement);
            checkFunctionScopeName(declaration.name(), declaration.location(), seen, moduleNames);
            context.allScopeNames.insert(declaration.name());
            break;
        }
        case NodeKind::ReturnStmt:
        case NodeKind::PassStmt:
        case NodeKind::ExprStmt:
        case NodeKind::AssignStmt:
            break;
        case NodeKind::FunctionDecl:
        case NodeKind::Param:
        case NodeKind::Block:
        case NodeKind::CallExpr:
        case NodeKind::IntegerLiteral:
        case NodeKind::StringLiteral:
        case NodeKind::NullLiteral:
        case NodeKind::Identifier:
        case NodeKind::EchoCall:
        case NodeKind::TypeofCall:
        case NodeKind::BooleanLiteral:
        case NodeKind::UnaryExpr:
        case NodeKind::BinaryExpr:
            throw InternalError("Validator: invalid statement node kind");
        }
    }
}

void validateExpressionInFunction(const ASTNode& expression, FunctionContext& context);

void validateIdentifierInFunction(const Identifier& identifier, const FunctionContext& context) {
    const std::string_view name = identifier.name();
    if (name == "main") {
        throw DeclarationError(identifier.location(), std::string(kMainName));
    }
    if (context.visibleNames.contains(name)) {
        return;
    }
    if (context.allScopeNames.contains(name)) {
        throw DeclarationError(identifier.location(), std::string(kUsedBeforeDeclaration));
    }
    if (context.moduleNames->all.contains(name)) {
        return;
    }
    throw DeclarationError(identifier.location(), std::string(kUndefinedIdentifier));
}

void validateCallInFunction(const CallExpr& call, FunctionContext& context) {
    validateIdentifierInFunction(call.callee(), context);
    for (const auto& argument : call.arguments()) {
        if (!argument) {
            throw InternalError("Validator: null call argument");
        }
        validateExpressionInFunction(*argument, context);
    }
}

void validateExpressionInFunction(const ASTNode& expression, FunctionContext& context) {
    switch (expression.kind()) {
    case NodeKind::IntegerLiteral:
    case NodeKind::StringLiteral:
    case NodeKind::NullLiteral:
    case NodeKind::BooleanLiteral:
        return;
    case NodeKind::Identifier:
        validateIdentifierInFunction(static_cast<const Identifier&>(expression), context);
        return;
    case NodeKind::CallExpr:
        validateCallInFunction(static_cast<const CallExpr&>(expression), context);
        return;
    case NodeKind::EchoCall:
        validateExpressionInFunction(static_cast<const EchoCall&>(expression).argument(), context);
        return;
    case NodeKind::TypeofCall:
        validateExpressionInFunction(static_cast<const TypeofCall&>(expression).argument(),
                                     context);
        return;
    case NodeKind::UnaryExpr:
        validateExpressionInFunction(static_cast<const UnaryExpr&>(expression).operand(), context);
        return;
    case NodeKind::BinaryExpr: {
        const auto& binary = static_cast<const BinaryExpr&>(expression);
        validateExpressionInFunction(binary.left(), context);
        validateExpressionInFunction(binary.right(), context);
        return;
    }
    case NodeKind::FunctionDecl:
    case NodeKind::Param:
    case NodeKind::Block:
    case NodeKind::ReturnStmt:
    case NodeKind::PassStmt:
    case NodeKind::ExprStmt:
    case NodeKind::LetDecl:
    case NodeKind::ConstDecl:
    case NodeKind::SymbolDecl:
    case NodeKind::AssignStmt:
        throw InternalError("Validator: invalid expression node kind");
    }
    throw InternalError("Validator: invalid expression node kind");
}

void validateAssignTarget(const AssignStmt& assign, FunctionContext& context) {
    const std::string_view name = assign.name();
    if (name == "main") {
        throw DeclarationError(assign.location(), std::string(kMainName));
    }

    if (context.visibleNames.contains(name)) {
        const auto localKind = context.localKinds.find(name);
        if (localKind == context.localKinds.end()) {
            throw InternalError("Validator: missing local binding kind");
        }

        switch (localKind->second) {
        case LocalNameKind::Parameter:
        case LocalNameKind::Variable:
            return;
        case LocalNameKind::Symbol:
            throw DeclarationError(assign.location(), std::string(kCannotAssignName));
        case LocalNameKind::Constant: {
            const auto constant = context.localConstants.find(name);
            if (constant == context.localConstants.end()) {
                throw InternalError("Validator: missing local constant state");
            }
            if (!constant->second.nullable || constant->second.assigned) {
                throw DeclarationError(assign.location(), std::string(kCannotAssignConstant));
            }
            constant->second.assigned = true;
            return;
        }
        }
        throw InternalError("Validator: invalid local binding kind");
    }

    if (context.allScopeNames.contains(name)) {
        throw DeclarationError(assign.location(), std::string(kUsedBeforeDeclaration));
    }
    if (context.moduleNames->constants.contains(name)) {
        throw DeclarationError(assign.location(), std::string(kCannotAssignConstant));
    }
    if (context.moduleNames->functions.contains(name) ||
        context.moduleNames->symbols.contains(name)) {
        throw DeclarationError(assign.location(), std::string(kCannotAssignName));
    }
    throw DeclarationError(assign.location(), std::string(kUndefinedIdentifier));
}

void validateStatements(const Block& block, FunctionContext& context) {
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
                if (context.isMain ||
                    (context.declaredReturn != DeclaredType{TypeName::Null, false} &&
                     !context.declaredReturn.nullable)) {
                    throw DeclarationError(returnStmt.location(),
                                           "bare return is not allowed in this function");
                }
            } else {
                validateExpressionInFunction(*value, context);
            }
            sawReturn = true;
            break;
        }
        case NodeKind::PassStmt:
            break;
        case NodeKind::ExprStmt: {
            const auto& exprStmt = static_cast<const ExprStmt&>(*statement);
            validateExpressionInFunction(exprStmt.expression(), context);
            break;
        }
        case NodeKind::LetDecl: {
            const auto& declaration = static_cast<const LetDecl&>(*statement);
            if (!declaration.type().nullable && declaration.initializer() == nullptr) {
                throw DeclarationError(declaration.location(), std::string(kInitializerRequired));
            }
            if (declaration.initializer() != nullptr) {
                validateExpressionInFunction(*declaration.initializer(), context);
            }
            context.visibleNames.insert(declaration.name());
            context.localKinds.insert({declaration.name(), LocalNameKind::Variable});
            break;
        }
        case NodeKind::ConstDecl: {
            const auto& declaration = static_cast<const ConstDecl&>(*statement);
            if (!declaration.type().nullable && declaration.initializer() == nullptr) {
                throw DeclarationError(declaration.location(), std::string(kInitializerRequired));
            }
            if (declaration.initializer() != nullptr) {
                validateExpressionInFunction(*declaration.initializer(), context);
            }
            context.visibleNames.insert(declaration.name());
            context.localKinds.insert({declaration.name(), LocalNameKind::Constant});
            context.localConstants.insert(
                {declaration.name(),
                 LocalConstantState{declaration.type().nullable,
                                    declaration.initializer() != nullptr &&
                                        declaration.initializer()->kind() !=
                                            NodeKind::NullLiteral}});
            break;
        }
        case NodeKind::SymbolDecl: {
            const auto& declaration = static_cast<const SymbolDecl&>(*statement);
            context.visibleNames.insert(declaration.name());
            context.localKinds.insert({declaration.name(), LocalNameKind::Symbol});
            break;
        }
        case NodeKind::AssignStmt: {
            const auto& assign = static_cast<const AssignStmt&>(*statement);
            validateAssignTarget(assign, context);
            validateExpressionInFunction(assign.value(), context);
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
        case NodeKind::EchoCall:
        case NodeKind::TypeofCall:
        case NodeKind::BooleanLiteral:
        case NodeKind::UnaryExpr:
        case NodeKind::BinaryExpr:
            throw InternalError("Validator: invalid statement node kind");
        }
    }
}

void validateFunction(const FunctionDecl& function, const ModuleNames& moduleNames) {
    const bool isMain = function.name() == "main";

    if (isMain) {
        if (!function.params().empty()) {
            throw DeclarationError(function.location(), "main must have no parameters");
        }
        const std::optional<DeclaredType> returnType = function.returnType();
        if (returnType.has_value() && *returnType != DeclaredType{TypeName::Integer, false}) {
            throw DeclarationError(function.location(), "main return type must be Integer");
        }
    } else if (!function.returnType().has_value()) {
        throw DeclarationError(function.location(), "function return type is required");
    }

    FunctionContext context;
    context.declaredReturn = declaredReturnType(function);
    context.isMain = isMain;
    context.moduleNames = &moduleNames;

    collectFunctionScopeNames(function, moduleNames, context);
    validateStatements(function.body(), context);
}

void validateIdentifierInModuleInitializer(const Identifier& identifier,
                                           const ModuleNames& moduleNames,
                                           const std::set<std::string_view, std::less<>>&
                                               visibleModuleValues) {
    const std::string_view name = identifier.name();
    if (name == "main") {
        throw DeclarationError(identifier.location(), std::string(kMainName));
    }
    if (visibleModuleValues.contains(name)) {
        return;
    }
    if (moduleNames.constants.contains(name) || moduleNames.symbols.contains(name)) {
        throw DeclarationError(identifier.location(), std::string(kUsedBeforeDeclaration));
    }
    if (moduleNames.functions.contains(name)) {
        throw DeclarationError(identifier.location(), std::string(kNotAllowedInModuleInitializer));
    }
    throw DeclarationError(identifier.location(), std::string(kUndefinedIdentifier));
}

void validateModuleInitializerExpression(const ASTNode& expression, const ModuleNames& moduleNames,
                                        const std::set<std::string_view, std::less<>>&
                                            visibleModuleValues) {
    switch (expression.kind()) {
    case NodeKind::IntegerLiteral:
    case NodeKind::StringLiteral:
    case NodeKind::NullLiteral:
    case NodeKind::BooleanLiteral:
        return;
    case NodeKind::Identifier:
        validateIdentifierInModuleInitializer(static_cast<const Identifier&>(expression),
                                              moduleNames, visibleModuleValues);
        return;
    case NodeKind::UnaryExpr:
        validateModuleInitializerExpression(static_cast<const UnaryExpr&>(expression).operand(),
                                            moduleNames, visibleModuleValues);
        return;
    case NodeKind::BinaryExpr: {
        const auto& binary = static_cast<const BinaryExpr&>(expression);
        validateModuleInitializerExpression(binary.left(), moduleNames, visibleModuleValues);
        validateModuleInitializerExpression(binary.right(), moduleNames, visibleModuleValues);
        return;
    }
    case NodeKind::CallExpr:
    case NodeKind::EchoCall:
    case NodeKind::TypeofCall:
        throw DeclarationError(expression.location(), std::string(kNotAllowedInModuleInitializer));
    case NodeKind::FunctionDecl:
    case NodeKind::Param:
    case NodeKind::Block:
    case NodeKind::ReturnStmt:
    case NodeKind::PassStmt:
    case NodeKind::ExprStmt:
    case NodeKind::LetDecl:
    case NodeKind::ConstDecl:
    case NodeKind::SymbolDecl:
    case NodeKind::AssignStmt:
        throw InternalError("Validator: invalid module initializer expression node kind");
    }
    throw InternalError("Validator: invalid module initializer expression node kind");
}

void validateModuleConst(const ConstDecl& declaration, const ModuleNames& moduleNames,
                         const std::set<std::string_view, std::less<>>& visibleModuleValues) {
    if (!declaration.type().nullable && declaration.initializer() == nullptr) {
        throw DeclarationError(declaration.location(), std::string(kInitializerRequired));
    }
    if (declaration.initializer() != nullptr) {
        validateModuleInitializerExpression(*declaration.initializer(), moduleNames,
                                            visibleModuleValues);
    }
}

} // namespace

void validateProgram(const Program& program) {
    ModuleNames moduleNames;
    collectModuleNames(program, moduleNames);

    if (!moduleNames.functions.contains("main")) {
        throw DeclarationError(SourceLocation{1, 1}, "missing main function");
    }

    std::set<std::string_view, std::less<>> visibleModuleValues;
    for (const auto& item : program) {
        if (!item) {
            throw InternalError("Validator: null module item");
        }

        switch (item->kind()) {
        case NodeKind::FunctionDecl:
            validateFunction(static_cast<const FunctionDecl&>(*item), moduleNames);
            break;
        case NodeKind::ConstDecl: {
            const auto& declaration = static_cast<const ConstDecl&>(*item);
            validateModuleConst(declaration, moduleNames, visibleModuleValues);
            visibleModuleValues.insert(declaration.name());
            break;
        }
        case NodeKind::SymbolDecl: {
            const auto& declaration = static_cast<const SymbolDecl&>(*item);
            visibleModuleValues.insert(declaration.name());
            break;
        }
        case NodeKind::Param:
        case NodeKind::Block:
        case NodeKind::ReturnStmt:
        case NodeKind::PassStmt:
        case NodeKind::ExprStmt:
        case NodeKind::CallExpr:
        case NodeKind::IntegerLiteral:
        case NodeKind::StringLiteral:
        case NodeKind::NullLiteral:
        case NodeKind::Identifier:
        case NodeKind::EchoCall:
        case NodeKind::TypeofCall:
        case NodeKind::LetDecl:
        case NodeKind::AssignStmt:
        case NodeKind::BooleanLiteral:
        case NodeKind::UnaryExpr:
        case NodeKind::BinaryExpr:
            throw InternalError("Validator: invalid module item node kind");
        }
    }
}

} // namespace philios
