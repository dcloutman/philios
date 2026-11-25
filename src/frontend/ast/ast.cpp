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

Identifier::Identifier(SourceLocation location, std::string_view name)
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

Param::Param(SourceLocation location, std::string_view name, DeclaredType type)
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

BooleanLiteral::BooleanLiteral(SourceLocation location, bool value) noexcept
    : ASTNode(location), value_(value) {}

NodeKind BooleanLiteral::kind() const noexcept {
    return NodeKind::BooleanLiteral;
}

void BooleanLiteral::detachChildren(
    [[maybe_unused]] std::unique_ptr<ASTNode>& worklistHead) noexcept {}

bool BooleanLiteral::value() const noexcept {
    return value_;
}

UnaryExpr::UnaryExpr(SourceLocation location, UnaryOperator op, std::unique_ptr<ASTNode> operand)
    : ASTNode(location), op_(op), operand_(std::move(operand)) {
    if (!operand_) {
        throw InternalError("UnaryExpr: null operand");
    }
}

UnaryExpr::~UnaryExpr() {
    std::unique_ptr<ASTNode> worklist;
    pushOntoWorklist(worklist, std::move(operand_));
    destroyWorklist(worklist);
}

NodeKind UnaryExpr::kind() const noexcept {
    return NodeKind::UnaryExpr;
}

void UnaryExpr::detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept {
    pushOntoWorklist(worklistHead, std::move(operand_));
}

UnaryOperator UnaryExpr::op() const noexcept {
    return op_;
}

const ASTNode& UnaryExpr::operand() const noexcept {
    return *operand_;
}

BinaryExpr::BinaryExpr(BinaryOperator op, SourceLocation operatorLocation,
                       std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
    : ASTNode(left ? left->location() : SourceLocation{}), op_(op),
      operatorLocation_(operatorLocation), left_(std::move(left)), right_(std::move(right)) {
    if (!left_) {
        throw InternalError("BinaryExpr: null left operand");
    }
    if (!right_) {
        throw InternalError("BinaryExpr: null right operand");
    }
}

BinaryExpr::~BinaryExpr() {
    std::unique_ptr<ASTNode> worklist;
    pushOntoWorklist(worklist, std::move(left_));
    pushOntoWorklist(worklist, std::move(right_));
    destroyWorklist(worklist);
}

NodeKind BinaryExpr::kind() const noexcept {
    return NodeKind::BinaryExpr;
}

void BinaryExpr::detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept {
    pushOntoWorklist(worklistHead, std::move(left_));
    pushOntoWorklist(worklistHead, std::move(right_));
}

BinaryOperator BinaryExpr::op() const noexcept {
    return op_;
}

SourceLocation BinaryExpr::operatorLocation() const noexcept {
    return operatorLocation_;
}

const ASTNode& BinaryExpr::left() const noexcept {
    return *left_;
}

const ASTNode& BinaryExpr::right() const noexcept {
    return *right_;
}

LetDecl::LetDecl(SourceLocation location, std::string_view name, SourceLocation nameLocation,
                 DeclaredType type, std::unique_ptr<ASTNode> initializer)
    : ASTNode(location), name_(name), nameLocation_(nameLocation), type_(type),
      initializer_(std::move(initializer)) {}

LetDecl::~LetDecl() {
    std::unique_ptr<ASTNode> worklist;
    pushOntoWorklist(worklist, std::move(initializer_));
    destroyWorklist(worklist);
}

NodeKind LetDecl::kind() const noexcept {
    return NodeKind::LetDecl;
}

void LetDecl::detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept {
    pushOntoWorklist(worklistHead, std::move(initializer_));
}

std::string_view LetDecl::name() const noexcept {
    return name_;
}

SourceLocation LetDecl::nameLocation() const noexcept {
    return nameLocation_;
}

DeclaredType LetDecl::type() const noexcept {
    return type_;
}

const ASTNode* LetDecl::initializer() const noexcept {
    return initializer_.get();
}

ConstDecl::ConstDecl(SourceLocation location, std::string_view name, SourceLocation nameLocation,
                     DeclaredType type, std::unique_ptr<ASTNode> initializer)
    : ASTNode(location), name_(name), nameLocation_(nameLocation), type_(type),
      initializer_(std::move(initializer)) {}

ConstDecl::~ConstDecl() {
    std::unique_ptr<ASTNode> worklist;
    pushOntoWorklist(worklist, std::move(initializer_));
    destroyWorklist(worklist);
}

NodeKind ConstDecl::kind() const noexcept {
    return NodeKind::ConstDecl;
}

void ConstDecl::detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept {
    pushOntoWorklist(worklistHead, std::move(initializer_));
}

std::string_view ConstDecl::name() const noexcept {
    return name_;
}

SourceLocation ConstDecl::nameLocation() const noexcept {
    return nameLocation_;
}

DeclaredType ConstDecl::type() const noexcept {
    return type_;
}

const ASTNode* ConstDecl::initializer() const noexcept {
    return initializer_.get();
}

SymbolDecl::SymbolDecl(SourceLocation location, std::string_view name)
    : ASTNode(location), name_(name) {}

NodeKind SymbolDecl::kind() const noexcept {
    return NodeKind::SymbolDecl;
}

void SymbolDecl::detachChildren([[maybe_unused]] std::unique_ptr<ASTNode>& worklistHead) noexcept {}

std::string_view SymbolDecl::name() const noexcept {
    return name_;
}

AssignStmt::AssignStmt(SourceLocation location, std::string_view name,
                       std::unique_ptr<ASTNode> value)
    : ASTNode(location), name_(name), value_(std::move(value)) {
    if (!value_) {
        throw InternalError("AssignStmt: null value");
    }
}

AssignStmt::~AssignStmt() {
    std::unique_ptr<ASTNode> worklist;
    pushOntoWorklist(worklist, std::move(value_));
    destroyWorklist(worklist);
}

NodeKind AssignStmt::kind() const noexcept {
    return NodeKind::AssignStmt;
}

void AssignStmt::detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept {
    pushOntoWorklist(worklistHead, std::move(value_));
}

std::string_view AssignStmt::name() const noexcept {
    return name_;
}

const ASTNode& AssignStmt::value() const noexcept {
    return *value_;
}

std::shared_ptr<const FunctionDecl> asFunctionDecl(const ModuleItem& item) {
    if (!item) {
        throw InternalError("asFunctionDecl: null module item");
    }
    if (item->kind() != NodeKind::FunctionDecl) {
        return nullptr;
    }
    return std::static_pointer_cast<const FunctionDecl>(item);
}

} // namespace philios
