#pragma once

/// @file
/// @brief AST node classes, NodeKind, TypeName, and the iterative-destruction protocol
///        (plan 0001 §3.4, §3.9, D3, D5, D45).
///
/// Dependency rule (§7.1): this header includes nothing from `src/runtime/` or `src/analysis/`.
/// ExecutionContext, Environment, and Completion are forward-declared; every `evaluate`
/// definition for the concrete nodes lives in `src/runtime/evaluator/evaluate.cpp`.
///
/// Ownership: nodes own their children through `std::unique_ptr`, except that each FunctionDecl is
/// owned through `std::shared_ptr<const FunctionDecl>` shared by the module's Program and the
/// Function object (D3). Nodes may hold `std::string_view`s into the module's source buffer; no
/// node may outlive the Module (§3.9).
///
/// Destruction (§3.9, CERT DCL57): every destructor and detachChildren is `noexcept`, does not
/// recurse once per node, and allocates nothing. A node's children are moved onto an intrusive
/// worklist threaded through each node's private `link_` and destroyed iteratively. Unlink
/// invariant: a node's `link_` is always empty when that node is destroyed.

#include "common/errors.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace philios {

class ExecutionContext;
class Environment;
struct Completion;

/// @brief One value per concrete node class. Used only by the validator, with `kind()` and a
///        `static_cast` to the matching `final` class (§3.9).
enum class NodeKind {
    FunctionDecl,
    Param,
    Block,
    ReturnStmt,
    PassStmt,
    ExprStmt,
    CallExpr,
    IntegerLiteral,
    StringLiteral,
    NullLiteral,
    Identifier,
};

/// @brief The parsed `typeName` production (§3.4). Compared with runtime values only by
///        matchesDeclaredType (§3.6).
enum class TypeName {
    Integer,  ///< `Integer`
    String,   ///< `String`
    Function, ///< `Function` (untyped function values, D16)
    Null,     ///< `null`
};

/// @brief Polymorphic base of every AST node (also the expression base used by CallExpr).
///
/// Not copyable or movable (CERT OOP51). Destroyed through `std::unique_ptr<ASTNode>`, so the
/// destructor is public and virtual. Test-only subclasses (T-FN-6, T-OWN-5) are allowed but are
/// never passed to the validator.
class ASTNode {
public:
    /// @pre `link_` is empty (unlink invariant).
    /// @throws Nothing.
    virtual ~ASTNode();

    ASTNode(const ASTNode&) = delete;
    ASTNode& operator=(const ASTNode&) = delete;
    ASTNode(ASTNode&&) = delete;
    ASTNode& operator=(ASTNode&&) = delete;

    /// @brief The node's concrete class tag.
    /// @throws Nothing.
    [[nodiscard]] virtual NodeKind kind() const noexcept = 0;

    /// @brief Evaluates this node (D5). Defined for concrete nodes in
    ///        `src/runtime/evaluator/evaluate.cpp`.
    ///
    /// Reentrant: may run Philios code. The first action is the evaluation-depth guard (D24).
    /// @param context The execution context; borrowed.
    /// @param environment The environment to resolve names in; borrowed and non-null. The
    ///        implementation holds its own owning copies across reentrant calls.
    /// @pre @p environment is non-null.
    /// @return A Completion whose value is non-null. Expressions complete Normal; `return`
    ///         completes Return; a block stops at the first Return.
    /// @throws RuntimeError for language runtime errors (including the depth limits);
    ///         InternalError for invariant failures; std::bad_alloc. Exceptions carry only errors,
    ///         never control flow (D5).
    [[nodiscard]] virtual Completion evaluate(ExecutionContext& context,
                                              const std::shared_ptr<Environment>& environment)
        const = 0;

    /// @brief Moves every child this node owns through a `std::unique_ptr` onto the front of the
    ///        destruction worklist.
    ///
    /// Called only by the destruction helper on a fully alive node that has already been unlinked
    /// from the list. A node without children does nothing.
    /// @param worklistHead Head of the intrusive worklist; each pushed child's empty `link_`
    ///        receives the old head and the child becomes the new head.
    /// @post This node owns no children.
    /// @throws Nothing; allocates nothing.
    virtual void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept = 0;

    /// @brief Location of the node's first byte (for errors, §4).
    /// @throws Nothing.
    [[nodiscard]] SourceLocation location() const noexcept;

protected:
    /// @brief Constructs the base with the node's location.
    /// @throws Nothing.
    explicit ASTNode(SourceLocation location) noexcept;

    /// @brief Pushes @p node onto the front of the worklist. A null @p node is ignored.
    /// @pre @p node's `link_` is empty.
    /// @throws Nothing; allocates nothing.
    static void pushOntoWorklist(std::unique_ptr<ASTNode>& worklistHead,
                                 std::unique_ptr<ASTNode> node) noexcept;

    /// @brief Destroys every node on the worklist without per-node recursion.
    ///
    /// Repeats: `node = std::move(head); head = std::move(node->link_);` then
    /// `node->detachChildren(head)`, then destroys the now childless `node`.
    /// @post @p worklistHead is empty.
    /// @throws Nothing; allocates nothing.
    static void destroyWorklist(std::unique_ptr<ASTNode>& worklistHead) noexcept;

private:
    std::unique_ptr<ASTNode> link_; ///< Used only during destruction.
    SourceLocation location_;
};

/// @brief An identifier used as a value or as a callee.
class Identifier final : public ASTNode {
public:
    /// @param location Location of the identifier's first byte.
    /// @param name View into the source buffer (or other storage outliving the node).
    /// @throws Nothing.
    Identifier(SourceLocation location, std::string_view name) noexcept;

    [[nodiscard]] NodeKind kind() const noexcept override;
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The identifier's text; valid while the source buffer lives.
    /// @throws Nothing.
    [[nodiscard]] std::string_view name() const noexcept;

private:
    std::string_view name_;
};

/// @brief A decimal integer literal. Each evaluation creates a new `Integer` object (§3.9).
class IntegerLiteral final : public ASTNode {
public:
    /// @throws Nothing.
    IntegerLiteral(SourceLocation location, std::int64_t value) noexcept;

    [[nodiscard]] NodeKind kind() const noexcept override;
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The parsed value.
    /// @throws Nothing.
    [[nodiscard]] std::int64_t value() const noexcept;

private:
    std::int64_t value_;
};

/// @brief A string literal holding its escape-processed bytes. Each evaluation creates a new
///        `String` object (§3.9).
class StringLiteral final : public ASTNode {
public:
    /// @param value Escape-processed bytes; ownership is taken.
    /// @throws Nothing.
    StringLiteral(SourceLocation location, std::string value) noexcept;

    [[nodiscard]] NodeKind kind() const noexcept override;
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The literal's bytes; valid while this node lives.
    /// @throws Nothing.
    [[nodiscard]] const std::string& value() const noexcept;

private:
    std::string value_;
};

/// @brief The `null` literal; evaluates to the execution context's NullObject (D18).
class NullLiteral final : public ASTNode {
public:
    /// @throws Nothing.
    explicit NullLiteral(SourceLocation location) noexcept;

    [[nodiscard]] NodeKind kind() const noexcept override;
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;
};

/// @brief `callee(args...)`. The callee is always an identifier (§3.4); arguments are any
///        ASTNode, so unit tests may pass test-only nodes (T-FN-6, T-DECL-8).
class CallExpr final : public ASTNode {
public:
    /// @param callee The callee identifier; ownership is taken. Its location is the call's.
    /// @param arguments Argument expressions in source order; ownership is taken.
    /// @pre @p callee and every element of @p arguments are non-null.
    /// @throws InternalError if a precondition is violated.
    CallExpr(std::unique_ptr<Identifier> callee, std::vector<std::unique_ptr<ASTNode>> arguments);

    /// @brief Iterative destruction of the callee and arguments (§3.9).
    /// @throws Nothing; allocates nothing.
    ~CallExpr() override;
    CallExpr(const CallExpr&) = delete;
    CallExpr& operator=(const CallExpr&) = delete;
    CallExpr(CallExpr&&) = delete;
    CallExpr& operator=(CallExpr&&) = delete;

    [[nodiscard]] NodeKind kind() const noexcept override;
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The callee identifier.
    /// @pre detachChildren has not been called.
    /// @throws Nothing.
    [[nodiscard]] const Identifier& callee() const noexcept;

    /// @brief The arguments in source order; empty after detachChildren.
    /// @throws Nothing.
    [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>>& arguments() const noexcept;

private:
    std::unique_ptr<Identifier> callee_;
    std::vector<std::unique_ptr<ASTNode>> arguments_;
};

/// @brief A call used as a statement; its result is discarded.
class ExprStmt final : public ASTNode {
public:
    /// @param call The call; ownership is taken. Its location is the statement's.
    /// @pre @p call is non-null.
    /// @throws InternalError if @p call is null.
    explicit ExprStmt(std::unique_ptr<CallExpr> call);

    /// @throws Nothing; allocates nothing.
    ~ExprStmt() override;
    ExprStmt(const ExprStmt&) = delete;
    ExprStmt& operator=(const ExprStmt&) = delete;
    ExprStmt(ExprStmt&&) = delete;
    ExprStmt& operator=(ExprStmt&&) = delete;

    [[nodiscard]] NodeKind kind() const noexcept override;
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The call.
    /// @pre detachChildren has not been called.
    /// @throws Nothing.
    [[nodiscard]] const CallExpr& call() const noexcept;

private:
    std::unique_ptr<CallExpr> call_;
};

/// @brief `return [expr]`.
class ReturnStmt final : public ASTNode {
public:
    /// @param location Location of the `return` keyword.
    /// @param value The returned expression, or null for a bare `return`; ownership is taken.
    /// @throws Nothing.
    ReturnStmt(SourceLocation location, std::unique_ptr<ASTNode> value) noexcept;

    /// @throws Nothing; allocates nothing.
    ~ReturnStmt() override;
    ReturnStmt(const ReturnStmt&) = delete;
    ReturnStmt& operator=(const ReturnStmt&) = delete;
    ReturnStmt(ReturnStmt&&) = delete;
    ReturnStmt& operator=(ReturnStmt&&) = delete;

    [[nodiscard]] NodeKind kind() const noexcept override;
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The returned expression (borrowed, nullable); null for a bare `return`.
    /// @throws Nothing.
    [[nodiscard]] const ASTNode* value() const noexcept;

private:
    std::unique_ptr<ASTNode> value_;
};

/// @brief `pass`, the no-op statement (D11).
class PassStmt final : public ASTNode {
public:
    /// @throws Nothing.
    explicit PassStmt(SourceLocation location) noexcept;

    [[nodiscard]] NodeKind kind() const noexcept override;
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;
};

/// @brief An indented block of at least one statement (D11). In this plan every block is a
///        function body.
class Block final : public ASTNode {
public:
    /// @param location Location of the first statement.
    /// @param statements Statements in source order; ownership is taken.
    /// @pre Every element of @p statements is non-null. (The parser also guarantees at least one
    ///      statement; unit tests may build other shapes.)
    /// @throws InternalError if an element is null.
    Block(SourceLocation location, std::vector<std::unique_ptr<ASTNode>> statements);

    /// @throws Nothing; allocates nothing.
    ~Block() override;
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    Block(Block&&) = delete;
    Block& operator=(Block&&) = delete;

    [[nodiscard]] NodeKind kind() const noexcept override;
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The statements in source order; empty after detachChildren.
    /// @throws Nothing.
    [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>>& statements() const noexcept;

private:
    std::vector<std::unique_ptr<ASTNode>> statements_;
};

/// @brief `name: typeName` in a parameter list.
class Param final : public ASTNode {
public:
    /// @param location Location of the parameter name.
    /// @param name View into the source buffer.
    /// @param type The declared type.
    /// @throws Nothing.
    Param(SourceLocation location, std::string_view name, TypeName type) noexcept;

    [[nodiscard]] NodeKind kind() const noexcept override;
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @throws Nothing.
    [[nodiscard]] std::string_view name() const noexcept;
    /// @throws Nothing.
    [[nodiscard]] TypeName type() const noexcept;

private:
    std::string_view name_;
    TypeName type_;
};

/// @brief `function name(params)[: typeName]:` followed by its body.
///
/// Owned through `std::shared_ptr<const FunctionDecl>` (shared with the Function object, D3), so
/// it is never put on another node's worklist; its own destructor runs the worklist loop over its
/// children.
class FunctionDecl final : public ASTNode {
public:
    /// @param location Location of the function name (declaration errors are reported there).
    /// @param name View into the source buffer.
    /// @param params Parameters in source order; ownership is taken.
    /// @param returnType The return annotation, or no value if omitted (allowed only for `main`).
    /// @param body The body; ownership is taken.
    /// @pre @p body and every element of @p params are non-null.
    /// @throws InternalError if a precondition is violated.
    FunctionDecl(SourceLocation location, std::string_view name,
                 std::vector<std::unique_ptr<Param>> params, std::optional<TypeName> returnType,
                 std::unique_ptr<Block> body);

    /// @throws Nothing; allocates nothing.
    ~FunctionDecl() override;
    FunctionDecl(const FunctionDecl&) = delete;
    FunctionDecl& operator=(const FunctionDecl&) = delete;
    FunctionDecl(FunctionDecl&&) = delete;
    FunctionDecl& operator=(FunctionDecl&&) = delete;

    [[nodiscard]] NodeKind kind() const noexcept override;
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @throws Nothing.
    [[nodiscard]] std::string_view name() const noexcept;
    /// @throws Nothing.
    [[nodiscard]] const std::vector<std::unique_ptr<Param>>& params() const noexcept;
    /// @brief The annotation, or no value if omitted.
    /// @throws Nothing.
    [[nodiscard]] std::optional<TypeName> returnType() const noexcept;
    /// @pre detachChildren has not been called.
    /// @throws Nothing.
    [[nodiscard]] const Block& body() const noexcept;

private:
    std::string_view name_;
    std::vector<std::unique_ptr<Param>> params_;
    std::optional<TypeName> returnType_;
    std::unique_ptr<Block> body_;
};

/// @brief The parsed `program` production: module-level function declarations in source order.
using Program = std::vector<std::shared_ptr<const FunctionDecl>>;

} // namespace philios
