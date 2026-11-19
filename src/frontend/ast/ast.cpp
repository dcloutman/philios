#include "frontend/ast/ast.hpp"

#include "common/errors.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace philios {

ASTNode::ASTNode(SourceLocation location) noexcept : location_(location) {}

ASTNode::~ASTNode() = default;

SourceLocation ASTNode::location() const noexcept {
    return location_;
}

void ASTNode::pushOntoWorklist(std::unique_ptr<ASTNode>& worklistHead,
                               std::unique_ptr<ASTNode> node) noexcept {
    if (!node) {
        return;
    }
    node->link_ = std::move(worklistHead);
    worklistHead = std::move(node);
}

void ASTNode::destroyWorklist(std::unique_ptr<ASTNode>& worklistHead) noexcept {
    while (worklistHead) {
        std::unique_ptr<ASTNode> node = std::move(worklistHead);
        worklistHead = std::move(node->link_);
        node->detachChildren(worklistHead);
        node.reset();
    }
}

Identifier::Identifier(SourceLocation location, std::string_view name) noexcept
    : ASTNode(location), name_(name) {}

NodeKind Identifier::kind() const noexcept {
    return NodeKind::Identifier;
}

void Identifier::detachChildren([[maybe_unused]] std::unique_ptr<ASTNode>& worklistHead) noexcept {}

std::string_view Identifier::name() const noexcept {
    return name_;
}

IntegerLiteral::IntegerLiteral(SourceLocation location, std::int64_t value) noexcept
    : ASTNode(location), value_(value) {}

NodeKind IntegerLiteral::kind() const noexcept {
    return NodeKind::IntegerLiteral;
}

void IntegerLiteral::detachChildren(
    [[maybe_unused]] std::unique_ptr<ASTNode>& worklistHead) noexcept {}

std::int64_t IntegerLiteral::value() const noexcept {
    return value_;
}

StringLiteral::StringLiteral(SourceLocation location, std::string value) noexcept
    : ASTNode(location), value_(std::move(value)) {}

NodeKind StringLiteral::kind() const noexcept {
    return NodeKind::StringLiteral;
}

void StringLiteral::detachChildren(
    [[maybe_unused]] std::unique_ptr<ASTNode>& worklistHead) noexcept {}

const std::string& StringLiteral::value() const noexcept {
    return value_;
}

NullLiteral::NullLiteral(SourceLocation location) noexcept : ASTNode(location) {}

NodeKind NullLiteral::kind() const noexcept {
    return NodeKind::NullLiteral;
}

void NullLiteral::detachChildren([[maybe_unused]] std::unique_ptr<ASTNode>& worklistHead) noexcept {}

CallExpr::CallExpr(std::unique_ptr<Identifier> callee,
                   std::vector<std::unique_ptr<ASTNode>> arguments)
    : ASTNode(callee ? callee->location() : SourceLocation{}), callee_(std::move(callee)),
      arguments_(std::move(arguments)) {
    if (!callee_) {
        throw InternalError("CallExpr: null callee");
    }
    for (const auto& argument : arguments_) {
        if (!argument) {
            throw InternalError("CallExpr: null argument");
        }
    }
}

CallExpr::~CallExpr() {
    std::unique_ptr<ASTNode> worklist;
    pushOntoWorklist(worklist, std::move(callee_));
    for (auto& argument : arguments_) {
        pushOntoWorklist(worklist, std::move(argument));
    }
    arguments_.clear();
    destroyWorklist(worklist);
}

NodeKind CallExpr::kind() const noexcept {
    return NodeKind::CallExpr;
}

void CallExpr::detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept {
    pushOntoWorklist(worklistHead, std::move(callee_));
    for (auto& argument : arguments_) {
        pushOntoWorklist(worklistHead, std::move(argument));
    }
    arguments_.clear();
}

const Identifier& CallExpr::callee() const noexcept {
    return *callee_;
}

const std::vector<std::unique_ptr<ASTNode>>& CallExpr::arguments() const noexcept {
    return arguments_;
}

EchoCall::EchoCall(SourceLocation location, std::unique_ptr<ASTNode> argument)
    : ASTNode(location), argument_(std::move(argument)) {
    if (!argument_) {
        throw InternalError("EchoCall: null argument");
    }
}

EchoCall::~EchoCall() {
    std::unique_ptr<ASTNode> worklist;
    pushOntoWorklist(worklist, std::move(argument_));
    destroyWorklist(worklist);
}

NodeKind EchoCall::kind() const noexcept {
    return NodeKind::EchoCall;
}

void EchoCall::detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept {
    pushOntoWorklist(worklistHead, std::move(argument_));
}

const ASTNode& EchoCall::argument() const noexcept {
    return *argument_;
}

TypeofCall::TypeofCall(SourceLocation location, std::unique_ptr<ASTNode> argument)
    : ASTNode(location), argument_(std::move(argument)) {
    if (!argument_) {
        throw InternalError("TypeofCall: null argument");
    }
}

TypeofCall::~TypeofCall() {
    std::unique_ptr<ASTNode> worklist;
    pushOntoWorklist(worklist, std::move(argument_));
    destroyWorklist(worklist);
}

NodeKind TypeofCall::kind() const noexcept {
    return NodeKind::TypeofCall;
}

void TypeofCall::detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept {
    pushOntoWorklist(worklistHead, std::move(argument_));
}

const ASTNode& TypeofCall::argument() const noexcept {
    return *argument_;
}

ExprStmt::ExprStmt(std::unique_ptr<ASTNode> expression)
    : ASTNode(expression ? expression->location() : SourceLocation{}),
      expression_(std::move(expression)) {
    if (!expression_) {
        throw InternalError("ExprStmt: null expression");
    }
    const NodeKind expressionKind = expression_->kind();
    if (expressionKind != NodeKind::CallExpr && expressionKind != NodeKind::EchoCall &&
        expressionKind != NodeKind::TypeofCall) {
        throw InternalError("ExprStmt: expression must be a call or pseudo-call");
    }
}

ExprStmt::~ExprStmt() {
    std::unique_ptr<ASTNode> worklist;
    pushOntoWorklist(worklist, std::move(expression_));
    destroyWorklist(worklist);
}

NodeKind ExprStmt::kind() const noexcept {
    return NodeKind::ExprStmt;
}

void ExprStmt::detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept {
    pushOntoWorklist(worklistHead, std::move(expression_));
}

const ASTNode& ExprStmt::expression() const noexcept {
    return *expression_;
}

ReturnStmt::ReturnStmt(SourceLocation location, std::unique_ptr<ASTNode> value) noexcept
    : ASTNode(location), value_(std::move(value)) {}

ReturnStmt::~ReturnStmt() {
    std::unique_ptr<ASTNode> worklist;
    pushOntoWorklist(worklist, std::move(value_));
    destroyWorklist(worklist);
}

NodeKind ReturnStmt::kind() const noexcept {
    return NodeKind::ReturnStmt;
}

void ReturnStmt::detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept {
    pushOntoWorklist(worklistHead, std::move(value_));
}

const ASTNode* ReturnStmt::value() const noexcept {
    return value_.get();
}

PassStmt::PassStmt(SourceLocation location) noexcept : ASTNode(location) {}

NodeKind PassStmt::kind() const noexcept {
    return NodeKind::PassStmt;
}

void PassStmt::detachChildren([[maybe_unused]] std::unique_ptr<ASTNode>& worklistHead) noexcept {}

Block::Block(SourceLocation location, std::vector<std::unique_ptr<ASTNode>> statements)
    : ASTNode(location), statements_(std::move(statements)) {
    for (const auto& statement : statements_) {
        if (!statement) {
            throw InternalError("Block: null statement");
        }
    }
}

Block::~Block() {
    std::unique_ptr<ASTNode> worklist;
    for (auto& statement : statements_) {
        pushOntoWorklist(worklist, std::move(statement));
    }
    statements_.clear();
    destroyWorklist(worklist);
}

NodeKind Block::kind() const noexcept {
    return NodeKind::Block;
}

void Block::detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept {
    for (auto& statement : statements_) {
        pushOntoWorklist(worklistHead, std::move(statement));
    }
    statements_.clear();
}

const std::vector<std::unique_ptr<ASTNode>>& Block::statements() const noexcept {
    return statements_;
}

Param::Param(SourceLocation location, std::string_view name, DeclaredType type) noexcept
    : ASTNode(location), name_(name), type_(type) {}

NodeKind Param::kind() const noexcept {
    return NodeKind::Param;
}

void Param::detachChildren([[maybe_unused]] std::unique_ptr<ASTNode>& worklistHead) noexcept {}

std::string_view Param::name() const noexcept {
    return name_;
}

DeclaredType Param::type() const noexcept {
    return type_;
}

FunctionDecl::FunctionDecl(SourceLocation location, std::string_view name,
                           std::vector<std::unique_ptr<Param>> params,
                           std::optional<DeclaredType> returnType, std::unique_ptr<Block> body)
    : ASTNode(location), name_(name), params_(std::move(params)), returnType_(returnType),
      body_(std::move(body)) {
    if (!body_) {
        throw InternalError("FunctionDecl: null body");
    }
    for (const auto& param : params_) {
        if (!param) {
            throw InternalError("FunctionDecl: null parameter");
        }
    }
}

FunctionDecl::~FunctionDecl() {
    std::unique_ptr<ASTNode> worklist;
    pushOntoWorklist(worklist, std::move(body_));
    for (auto& param : params_) {
        pushOntoWorklist(worklist, std::move(param));
    }
    params_.clear();
    destroyWorklist(worklist);
}

NodeKind FunctionDecl::kind() const noexcept {
    return NodeKind::FunctionDecl;
}

void FunctionDecl::detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept {
    pushOntoWorklist(worklistHead, std::move(body_));
    for (auto& param : params_) {
        pushOntoWorklist(worklistHead, std::move(param));
    }
    params_.clear();
}

std::string_view FunctionDecl::name() const noexcept {
    return name_;
}

const std::vector<std::unique_ptr<Param>>& FunctionDecl::params() const noexcept {
    return params_;
}

std::optional<DeclaredType> FunctionDecl::returnType() const noexcept {
    return returnType_;
}

const Block& FunctionDecl::body() const noexcept {
    return *body_;
}

} // namespace philios
