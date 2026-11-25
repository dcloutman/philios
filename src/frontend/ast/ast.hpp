#pragma once

/// @file
/// @brief AST node classes, NodeKind, TypeName, DeclaredType, and the iterative-destruction
///        protocol (plan 0001 §3.4, §3.9, D3, D5, D45; plan 0002 §3.4, §3.8: DeclaredType and
///        the EchoCall and TypeofCall pseudo-call nodes; plan 0003 §4.12, §4.14.2: declarations,
///        assignment, `Boolean` literals, the unary and binary operator nodes, and module items).
///
/// Dependency rule (§7.1): this header includes nothing from `src/runtime/` or `src/analysis/`.
/// ExecutionContext, Environment, and Completion are forward-declared; every `evaluate`
/// definition for the concrete nodes lives in `src/runtime/evaluator/evaluate.cpp`.
///
/// Ownership: nodes own their children through `std::unique_ptr`, except module items (plan 0003
/// §4.14.2, AD2). Each module item (a FunctionDecl, ConstDecl, or SymbolDecl) is owned through a
/// ModuleItem (`std::shared_ptr<const ASTNode>`) held by the module's Program; a FunctionDecl item
/// is also shared with its Function object through the pointer asFunctionDecl returns (D3).
/// Function-scope declarations and statements are owned by their Block like every other
/// statement. Every node that has a name owns its text as a `std::string`, so no node refers to
/// the source buffer: only tokens do, and a node may safely outlive the Module.
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

/// @brief One value per concrete node class. Used by the validator, Module::load, and
///        asFunctionDecl, with `kind()` and a `static_cast` (or `std::static_pointer_cast`) to the
///        matching `final` class (§3.9; plan 0003 §4.14.2).
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
    EchoCall,   ///< `echo(argument)` pseudo-call (plan 0002 §3.5, §3.8).
    TypeofCall, ///< `typeof(argument)` pseudo-call (plan 0002 §3.5, §3.8).
    LetDecl,        ///< `let name: Type [= expression]` (plan 0003 §4.6).
    ConstDecl,      ///< `const name: Type [= expression]` (plan 0003 §4.7).
    SymbolDecl,     ///< `symbol name` (plan 0003 §4.9).
    AssignStmt,     ///< `name = expression` (plan 0003 §4.8).
    BooleanLiteral, ///< `true` or `false` (plan 0003 §4.3).
    UnaryExpr,      ///< A unary operator applied to one operand (plan 0003 §4.12).
    BinaryExpr,     ///< A binary operator applied to two operands (plan 0003 §4.12).
};

/// @brief The type word of a parsed `typeName` (plan 0001 §3.4; plan 0002 §3.3, §3.4). Compared
///        with runtime values only by matchesDeclaredType, through DeclaredType.
enum class TypeName {
    Integer,  ///< `Integer`
    String,   ///< `String`
    Function, ///< `Function` (untyped function values, plan 0001 D16)
    Null,     ///< `null`
    Object,   ///< `Object`: accepts every non-`null` value (plan 0002 D9)
    Boolean,  ///< `Boolean` (plan 0003 §4.3)
    Symbol,   ///< `Symbol` (plan 0003 §4.3)
};

/// @brief The operator of a UnaryExpr (plan 0003 §4.12). New unary operators add enumerators,
///        not node classes.
enum class UnaryOperator {
    Not, ///< `!` (plan 0003 §4.11.1)
};

/// @brief The operator of a BinaryExpr (plan 0003 §4.12). New binary operators add enumerators,
///        not node classes.
enum class BinaryOperator {
    Equal,    ///< `==` (plan 0003 §4.11.2)
    NotEqual, ///< `!=` (plan 0003 §4.11.3)
};

/// @brief A parsed `typeName` (plan 0002 §3.3, §3.4): a base type word and the optional `?`
///        nullable prefix.
///
/// A plain aggregate stored by value in Param, FunctionDecl, LetDecl, and ConstDecl; it owns
/// nothing. The thirteen declarable types (plan 0003 §4.3) are the seven base words with
/// `nullable == false` and `Integer`, `String`, `Function`, `Object`, `Boolean`, and `Symbol` with
/// `nullable == true`. Invariant (enforced by the parser, not by
/// this type): `nullable` is never true together with `TypeName::Null` (`?null` is a syntax
/// error, D10). Unit tests may build other values directly.
struct DeclaredType {
    TypeName base;  ///< The type word.
    bool nullable;  ///< True for the `?Type` form (D9, D25).

    /// @brief Member-wise equality.
    /// @throws Nothing.
    friend bool operator==(const DeclaredType&, const DeclaredType&) noexcept = default;
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
    /// @param name The identifier's text; copied into the node, so it need not outlive the node
    ///        (plan 0003 §4.14.2, "Name storage").
    /// @throws std::bad_alloc if copying @p name fails.
    Identifier(SourceLocation location, std::string_view name);

    [[nodiscard]] NodeKind kind() const noexcept override;
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The identifier's text: a view of the node's own copy, valid while the node lives.
    /// @throws Nothing.
    [[nodiscard]] std::string_view name() const noexcept;

private:
    std::string name_;
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

/// @brief `echo(argument)`: the output pseudo-call (plan 0002 §3.5, §3.6, §3.8; D8, D13–D18, D24).
///
/// A built-in language construct, not a Philios call: evaluating it adds no call depth, creates
/// no Environment, and runs no call-depth guard. The parser guarantees exactly one argument, and
/// that the argument is not an integer or `null` literal (D16, D17).
class EchoCall final : public ASTNode {
public:
    /// @param location Location of the `echo` keyword (output-write and evaluation-depth errors
    ///        are reported there).
    /// @param argument The single argument expression; ownership is taken.
    /// @pre @p argument is non-null.
    /// @throws InternalError if @p argument is null.
    EchoCall(SourceLocation location, std::unique_ptr<ASTNode> argument);

    /// @brief Iterative destruction of the argument (plan 0001 §3.9; T2-OWN-2).
    /// @throws Nothing; allocates nothing.
    ~EchoCall() override;
    EchoCall(const EchoCall&) = delete;
    EchoCall& operator=(const EchoCall&) = delete;
    EchoCall(EchoCall&&) = delete;
    EchoCall& operator=(EchoCall&&) = delete;

    [[nodiscard]] NodeKind kind() const noexcept override;

    /// @brief Evaluates `echo` (plan 0002 §3.5).
    ///
    /// Order: the evaluation-depth guard at the keyword; the argument, evaluated exactly once
    /// into an owning local held for the rest of the pseudo-call; then the checks.
    /// @return A Normal completion whose value is the context's NullObject.
    /// @throws RuntimeError of kind ArgumentType at the argument's location if the value is not a
    ///         `String` (nothing is written); RuntimeError of kind OutputWrite at the keyword if
    ///         ExecutionContext::writeOutput returns a non-empty code (the message contains
    ///         `standard output` followed by the code's reason text); RuntimeError of kind
    ///         EvaluationDepthExceeded at the keyword; any error raised by the argument;
    ///         InternalError; std::bad_alloc.
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The argument expression.
    /// @pre detachChildren has not been called.
    /// @throws Nothing.
    [[nodiscard]] const ASTNode& argument() const noexcept;

private:
    std::unique_ptr<ASTNode> argument_;
};

/// @brief `typeof(argument)`: the runtime-type pseudo-call (plan 0002 §3.4, §3.5, §3.8; D3, D8,
///        D12).
///
/// A built-in language construct, not a Philios call (see EchoCall). The parser guarantees
/// exactly one argument (D17).
class TypeofCall final : public ASTNode {
public:
    /// @param location Location of the `typeof` keyword (the evaluation-depth error is reported
    ///        there).
    /// @param argument The single argument expression; ownership is taken.
    /// @pre @p argument is non-null.
    /// @throws InternalError if @p argument is null.
    TypeofCall(SourceLocation location, std::unique_ptr<ASTNode> argument);

    /// @brief Iterative destruction of the argument (plan 0001 §3.9; T2-OWN-2).
    /// @throws Nothing; allocates nothing.
    ~TypeofCall() override;
    TypeofCall(const TypeofCall&) = delete;
    TypeofCall& operator=(const TypeofCall&) = delete;
    TypeofCall(TypeofCall&&) = delete;
    TypeofCall& operator=(TypeofCall&&) = delete;

    [[nodiscard]] NodeKind kind() const noexcept override;

    /// @brief Evaluates `typeof` (plan 0002 §3.5).
    ///
    /// Order: the evaluation-depth guard at the keyword, then the argument, evaluated exactly
    /// once into an owning local. Accepts a value of any runtime class, including `null`.
    /// @return A Normal completion whose value is a new `String` holding scriptTypeName of the
    ///         argument's runtime value (the runtime type, never the declared type). A new
    ///         `String` is created on every evaluation (T2-TYPEOF-8).
    /// @throws RuntimeError of kind EvaluationDepthExceeded at the keyword; any error raised by
    ///         the argument; InternalError; std::bad_alloc. `typeof` has no error of its own.
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The argument expression.
    /// @pre detachChildren has not been called.
    /// @throws Nothing.
    [[nodiscard]] const ASTNode& argument() const noexcept;

private:
    std::unique_ptr<ASTNode> argument_;
};

/// @brief A call or pseudo-call used as a statement; its result is discarded (plan 0002 §3.3:
///        `exprStmt := ( callExpr | pseudoCall ) NEWLINE`).
class ExprStmt final : public ASTNode {
public:
    /// @param expression The call (CallExpr) or pseudo-call (EchoCall or TypeofCall); ownership
    ///        is taken. Its location is the statement's.
    /// @pre @p expression is non-null and its kind() is NodeKind::CallExpr, NodeKind::EchoCall,
    ///      or NodeKind::TypeofCall.
    /// @throws InternalError if a precondition is violated.
    explicit ExprStmt(std::unique_ptr<ASTNode> expression);

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

    /// @brief The call or pseudo-call; its kind() is CallExpr, EchoCall, or TypeofCall.
    /// @pre detachChildren has not been called.
    /// @throws Nothing.
    [[nodiscard]] const ASTNode& expression() const noexcept;

private:
    std::unique_ptr<ASTNode> expression_;
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
    /// @param name The parameter name; copied into the node, so it need not outlive the node
    ///        (plan 0003 §4.14.2, "Name storage").
    /// @param type The declared type, including its nullability (plan 0002 §3.4).
    /// @throws std::bad_alloc if copying @p name fails.
    Param(SourceLocation location, std::string_view name, DeclaredType type);

    [[nodiscard]] NodeKind kind() const noexcept override;
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The parameter name: a view of the node's own copy, valid while the node lives.
    /// @throws Nothing.
    [[nodiscard]] std::string_view name() const noexcept;
    /// @brief The declared parameter type; checked against each argument by
    ///        matchesDeclaredType at call time.
    /// @throws Nothing.
    [[nodiscard]] DeclaredType type() const noexcept;

private:
    std::string name_;
    DeclaredType type_;
};

/// @brief `function name(params)[: typeName]:` followed by its body.
///
/// A module item (plan 0003 §4.14.2): owned through the module's Program as a ModuleItem, and
/// shared with the Function object through the `std::shared_ptr<const FunctionDecl>` that
/// asFunctionDecl returns (same control block, D3). It is therefore never put on another node's
/// worklist; its own destructor runs the worklist loop over its children.
class FunctionDecl final : public ASTNode {
public:
    /// @param location Location of the function name (declaration errors are reported there).
    /// @param name The function name; copied into the node, so it need not outlive the node
    ///        (plan 0003 §4.14.2, "Name storage").
    /// @param params Parameters in source order; ownership is taken.
    /// @param returnType The return annotation, or no value if omitted (allowed only for `main`).
    /// @param body The body; ownership is taken.
    /// @pre @p body and every element of @p params are non-null.
    /// @throws InternalError if a precondition is violated; std::bad_alloc if copying @p name
    ///         fails.
    FunctionDecl(SourceLocation location, std::string_view name,
                 std::vector<std::unique_ptr<Param>> params,
                 std::optional<DeclaredType> returnType, std::unique_ptr<Block> body);

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

    /// @brief The function name: a view of the node's own copy, valid while the node lives.
    /// @throws Nothing.
    [[nodiscard]] std::string_view name() const noexcept;
    /// @throws Nothing.
    [[nodiscard]] const std::vector<std::unique_ptr<Param>>& params() const noexcept;
    /// @brief The return annotation, including its nullability (plan 0002 §3.4), or no value if
    ///        omitted.
    /// @throws Nothing.
    [[nodiscard]] std::optional<DeclaredType> returnType() const noexcept;
    /// @pre detachChildren has not been called.
    /// @throws Nothing.
    [[nodiscard]] const Block& body() const noexcept;

private:
    std::string name_;
    std::vector<std::unique_ptr<Param>> params_;
    std::optional<DeclaredType> returnType_;
    std::unique_ptr<Block> body_;
};

/// @brief `true` or `false` (plan 0003 §4.3, §4.4). Evaluates to a `Boolean` with this value;
///        whether a new object or an ExecutionContext-owned canonical instance is returned is
///        decided by the evaluator (plan 0003 §4.3).
class BooleanLiteral final : public ASTNode {
public:
    /// @param location Location of the `true` or `false` keyword.
    /// @param value The literal's value.
    /// @throws Nothing.
    BooleanLiteral(SourceLocation location, bool value) noexcept;

    [[nodiscard]] NodeKind kind() const noexcept override;
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    /// @brief Does nothing: a BooleanLiteral has no children.
    /// @throws Nothing; allocates nothing.
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The literal's value.
    /// @throws Nothing.
    [[nodiscard]] bool value() const noexcept;

private:
    bool value_;
};

/// @brief A unary operator applied to one operand (plan 0003 §4.11.1, §4.12): `!primary`.
///
/// The single unary-operator node: a new unary operator adds a UnaryOperator enumerator, not a
/// node class (§4.12). The parser guarantees the operand is a `primary` (never another `!`, DA5).
class UnaryExpr final : public ASTNode {
public:
    /// @param location Location of the operator token (the expression's first byte).
    /// @param op The operator.
    /// @param operand The operand expression; ownership is taken.
    /// @pre @p operand is non-null.
    /// @throws InternalError if @p operand is null.
    UnaryExpr(SourceLocation location, UnaryOperator op, std::unique_ptr<ASTNode> operand);

    /// @brief Iterative destruction of the operand (plan 0003 §4.14.2, §6.1).
    /// @throws Nothing; allocates nothing.
    ~UnaryExpr() override;
    UnaryExpr(const UnaryExpr&) = delete;
    UnaryExpr& operator=(const UnaryExpr&) = delete;
    UnaryExpr(UnaryExpr&&) = delete;
    UnaryExpr& operator=(UnaryExpr&&) = delete;

    [[nodiscard]] NodeKind kind() const noexcept override;

    /// @brief Evaluates the operator (plan 0003 §4.11.1, §4.14.1).
    ///
    /// For `Not`: the evaluation-depth guard, then the operand into an owning local, then the
    /// unconditional `ObjectType::Boolean` check.
    /// @return A Normal completion holding a `Boolean` that is the negation of the operand.
    /// @throws RuntimeError of kind OperandType at the operand's first byte if the operand is not
    ///         a `Boolean`; RuntimeError of kind EvaluationDepthExceeded; any error raised by the
    ///         operand; InternalError; std::bad_alloc.
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The operator.
    /// @throws Nothing.
    [[nodiscard]] UnaryOperator op() const noexcept;

    /// @brief The operand expression.
    /// @pre detachChildren has not been called.
    /// @throws Nothing.
    [[nodiscard]] const ASTNode& operand() const noexcept;

private:
    UnaryOperator op_;
    std::unique_ptr<ASTNode> operand_;
};

/// @brief A binary operator applied to two operands (plan 0003 §4.11.2, §4.11.3, §4.12):
///        `left == right` or `left != right`.
///
/// The single binary-operator node: a new binary operator adds a BinaryOperator enumerator, not a
/// node class (§4.12). Equality chains are left-associative (D16), so `a == b == c` is a
/// BinaryExpr whose left operand is the BinaryExpr `a == b`.
class BinaryExpr final : public ASTNode {
public:
    /// @param op The operator.
    /// @param operatorLocation Location of the operator token (`==` or `!=`).
    /// @param left The left operand; ownership is taken. Its location is the expression's.
    /// @param right The right operand; ownership is taken.
    /// @pre @p left and @p right are non-null.
    /// @throws InternalError if a precondition is violated.
    BinaryExpr(BinaryOperator op, SourceLocation operatorLocation, std::unique_ptr<ASTNode> left,
               std::unique_ptr<ASTNode> right);

    /// @brief Iterative destruction of both operands (plan 0003 §4.14.2, §6.1).
    /// @throws Nothing; allocates nothing.
    ~BinaryExpr() override;
    BinaryExpr(const BinaryExpr&) = delete;
    BinaryExpr& operator=(const BinaryExpr&) = delete;
    BinaryExpr(BinaryExpr&&) = delete;
    BinaryExpr& operator=(BinaryExpr&&) = delete;

    [[nodiscard]] NodeKind kind() const noexcept override;

    /// @brief Evaluates the operator (plan 0003 §4.11.2–§4.11.4).
    ///
    /// The evaluation-depth guard, then the left operand completely, then the right operand
    /// completely (D5), each into an owning local, then valuesEqual (D11). Equality never fails.
    /// @return A Normal completion holding a `Boolean`: valuesEqual for `Equal`, its negation for
    ///         `NotEqual`.
    /// @throws RuntimeError of kind EvaluationDepthExceeded; any error raised by an operand
    ///         (if the left operand fails, the right is not evaluated); InternalError;
    ///         std::bad_alloc.
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The operator.
    /// @throws Nothing.
    [[nodiscard]] BinaryOperator op() const noexcept;

    /// @brief Location of the operator token.
    /// @throws Nothing.
    [[nodiscard]] SourceLocation operatorLocation() const noexcept;

    /// @brief The left operand.
    /// @pre detachChildren has not been called.
    /// @throws Nothing.
    [[nodiscard]] const ASTNode& left() const noexcept;

    /// @brief The right operand.
    /// @pre detachChildren has not been called.
    /// @throws Nothing.
    [[nodiscard]] const ASTNode& right() const noexcept;

private:
    BinaryOperator op_;
    SourceLocation operatorLocation_;
    std::unique_ptr<ASTNode> left_;
    std::unique_ptr<ASTNode> right_;
};

/// @brief `let name: typeName [= expression]` (plan 0003 §4.4, §4.6). A function-scope statement
///        only (`let` at module level is a syntax error, D13).
class LetDecl final : public ASTNode {
public:
    /// @param location Location of the `let` keyword (`initializer required` is reported there).
    /// @param name The declared name; copied into the node, so it need not outlive the node
    ///        (plan 0003 §4.14.2, "Name storage").
    /// @param nameLocation Location of the declared name (name errors are reported there).
    /// @param type The declared type, including its nullability.
    /// @param initializer The initializer expression, or null if omitted; ownership is taken.
    /// @throws std::bad_alloc if copying @p name fails.
    LetDecl(SourceLocation location, std::string_view name, SourceLocation nameLocation,
            DeclaredType type, std::unique_ptr<ASTNode> initializer);

    /// @brief Iterative destruction of the initializer (plan 0003 §4.14.2).
    /// @throws Nothing; allocates nothing.
    ~LetDecl() override;
    LetDecl(const LetDecl&) = delete;
    LetDecl& operator=(const LetDecl&) = delete;
    LetDecl(LetDecl&&) = delete;
    LetDecl& operator=(LetDecl&&) = delete;

    [[nodiscard]] NodeKind kind() const noexcept override;

    /// @brief Binds the variable in @p environment (plan 0003 §4.6, §4.14.1).
    ///
    /// The evaluation-depth guard; then, if there is an initializer, it is evaluated into an owning
    /// local and checked unconditionally with matchesDeclaredType against type(); without one the
    /// variable is bound to the context's NullObject (the validator guarantees the type is
    /// nullable). Binds with BindingKind::Variable and type().
    /// @return A Normal completion whose value is the context's NullObject.
    /// @throws RuntimeError of kind AssignmentType at the initializer's first byte if the value
    ///         is not accepted; RuntimeError of kind EvaluationDepthExceeded; any error raised by
    ///         the initializer; InternalError (including the name already being bound in
    ///         @p environment); std::bad_alloc.
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The declared name: a view of the node's own copy, valid while the node lives.
    /// @throws Nothing.
    [[nodiscard]] std::string_view name() const noexcept;

    /// @brief Location of the declared name.
    /// @throws Nothing.
    [[nodiscard]] SourceLocation nameLocation() const noexcept;

    /// @brief The declared type.
    /// @throws Nothing.
    [[nodiscard]] DeclaredType type() const noexcept;

    /// @brief The initializer (borrowed, nullable); null if omitted or after detachChildren.
    /// @throws Nothing.
    [[nodiscard]] const ASTNode* initializer() const noexcept;

private:
    std::string name_;
    SourceLocation nameLocation_;
    DeclaredType type_;
    std::unique_ptr<ASTNode> initializer_;
};

/// @brief `const name: typeName [= expression]` (plan 0003 §4.4, §4.7). Used at module level (as
///        a module item) and in function scope.
///
/// At module level it is owned through a ModuleItem and never put on a worklist; in function scope
/// it is owned by its Block (plan 0003 §4.14.2).
class ConstDecl final : public ASTNode {
public:
    /// @param location Location of the `const` keyword (`initializer required` is reported
    ///        there).
    /// @param name The declared name; copied into the node, so it need not outlive the node
    ///        (plan 0003 §4.14.2, "Name storage").
    /// @param nameLocation Location of the declared name (name errors are reported there).
    /// @param type The declared type, including its nullability.
    /// @param initializer The initializer expression, or null if omitted; ownership is taken.
    /// @throws std::bad_alloc if copying @p name fails.
    ConstDecl(SourceLocation location, std::string_view name, SourceLocation nameLocation,
              DeclaredType type, std::unique_ptr<ASTNode> initializer);

    /// @brief Iterative destruction of the initializer (plan 0003 §4.14.2).
    /// @throws Nothing; allocates nothing.
    ~ConstDecl() override;
    ConstDecl(const ConstDecl&) = delete;
    ConstDecl& operator=(const ConstDecl&) = delete;
    ConstDecl(ConstDecl&&) = delete;
    ConstDecl& operator=(ConstDecl&&) = delete;

    [[nodiscard]] NodeKind kind() const noexcept override;

    /// @brief Binds the constant in @p environment (plan 0003 §4.7, §4.14.1).
    ///
    /// The evaluation-depth guard. With a nullable type and no initializer, or a NullLiteral
    /// initializer, the initializer is not evaluated and the constant is bound to the context's
    /// NullObject (unassigned). Otherwise the initializer is evaluated into an owning local; a
    /// nullable type with a `null` value fails, then matchesDeclaredType is checked
    /// unconditionally. Binds with BindingKind::Constant and type().
    /// @return A Normal completion whose value is the context's NullObject.
    /// @throws RuntimeError of kind ConstantNullWrite or AssignmentType at the initializer's first
    ///         byte; RuntimeError of kind EvaluationDepthExceeded; any error raised by the
    ///         initializer; InternalError (including the name already being bound in
    ///         @p environment); std::bad_alloc.
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The declared name: a view of the node's own copy, valid while the node lives.
    /// @throws Nothing.
    [[nodiscard]] std::string_view name() const noexcept;

    /// @brief Location of the declared name.
    /// @throws Nothing.
    [[nodiscard]] SourceLocation nameLocation() const noexcept;

    /// @brief The declared type.
    /// @throws Nothing.
    [[nodiscard]] DeclaredType type() const noexcept;

    /// @brief The initializer (borrowed, nullable); null if omitted or after detachChildren.
    /// @throws Nothing.
    [[nodiscard]] const ASTNode* initializer() const noexcept;

private:
    std::string name_;
    SourceLocation nameLocation_;
    DeclaredType type_;
    std::unique_ptr<ASTNode> initializer_;
};

/// @brief `symbol name` (plan 0003 §4.4, §4.9). Used at module level (as a module item) and in
///        function scope. Has no children.
class SymbolDecl final : public ASTNode {
public:
    /// @param location Location of the declared name (every SymbolDecl error is reported there,
    ///        plan 0003 §5.3; as for FunctionDecl, the keyword location is not kept).
    /// @param name The declared name; copied into the node, so it need not outlive the node
    ///        (plan 0003 §4.14.2, "Name storage").
    /// @throws std::bad_alloc if copying @p name fails.
    SymbolDecl(SourceLocation location, std::string_view name);

    [[nodiscard]] NodeKind kind() const noexcept override;

    /// @brief Creates a new `Symbol` and binds it in @p environment with BindingKind::Symbol
    ///        (plan 0003 §4.9; DA4: a new symbol each time the declaration executes).
    /// @return A Normal completion whose value is the context's NullObject.
    /// @throws RuntimeError of kind EvaluationDepthExceeded; InternalError (including the name
    ///         already being bound in @p environment); std::bad_alloc.
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    /// @brief Does nothing: a SymbolDecl has no children.
    /// @throws Nothing; allocates nothing.
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The declared name: a view of the node's own copy, valid while the node lives.
    /// @throws Nothing.
    [[nodiscard]] std::string_view name() const noexcept;

private:
    std::string name_;
};

/// @brief `name = expression` (plan 0003 §4.4, §4.8). A statement only (D7).
class AssignStmt final : public ASTNode {
public:
    /// @param location Location of the target name (the statement's first byte; target errors are
    ///        reported there).
    /// @param name The target name; copied into the node, so it need not outlive the node
    ///        (plan 0003 §4.14.2, "Name storage").
    /// @param value The right-hand side; ownership is taken.
    /// @pre @p value is non-null.
    /// @throws InternalError if @p value is null; std::bad_alloc if copying @p name fails.
    AssignStmt(SourceLocation location, std::string_view name, std::unique_ptr<ASTNode> value);

    /// @brief Iterative destruction of the right-hand side (plan 0003 §4.14.2).
    /// @throws Nothing; allocates nothing.
    ~AssignStmt() override;
    AssignStmt(const AssignStmt&) = delete;
    AssignStmt& operator=(const AssignStmt&) = delete;
    AssignStmt(AssignStmt&&) = delete;
    AssignStmt& operator=(AssignStmt&&) = delete;

    [[nodiscard]] NodeKind kind() const noexcept override;

    /// @brief Executes the assignment (plan 0003 §4.8 steps 1–4, §4.14.3).
    ///
    /// The evaluation-depth guard; step 1 (target binding and, for a constant, its unassigned
    /// state through Environment::bindingInfo and lookup); step 2 (the right-hand side into an
    /// owning local); step 3 (null and matchesDeclaredType checks); step 4 (Environment::assign).
    /// If any step fails, the target keeps its previous value.
    /// @return A Normal completion whose value is the context's NullObject.
    /// @throws RuntimeError of kind ConstantReassignment at the target, or ConstantNullWrite or
    ///         AssignmentType at the right-hand side's first byte; RuntimeError of kind
    ///         EvaluationDepthExceeded; any error raised by the right-hand side; InternalError
    ///         (including a Function or Symbol binding as the target); std::bad_alloc.
    [[nodiscard]] Completion evaluate(ExecutionContext& context,
                                      const std::shared_ptr<Environment>& environment)
        const override;
    void detachChildren(std::unique_ptr<ASTNode>& worklistHead) noexcept override;

    /// @brief The target name: a view of the node's own copy, valid while the node lives.
    /// @throws Nothing.
    [[nodiscard]] std::string_view name() const noexcept;

    /// @brief The right-hand side.
    /// @pre detachChildren has not been called.
    /// @throws Nothing.
    [[nodiscard]] const ASTNode& value() const noexcept;

private:
    std::string name_;
    std::unique_ptr<ASTNode> value_;
};

/// @brief One `moduleItem` (§4.4): a FunctionDecl, ConstDecl, or SymbolDecl.
using ModuleItem = std::shared_ptr<const ASTNode>;

/// @brief The parsed `program` production: every module item in source order.
///
/// Invariant (plan 0003 §4.14.2): every item is non-null, and its `kind()` is
/// NodeKind::FunctionDecl, NodeKind::ConstDecl, or NodeKind::SymbolDecl. The parser guarantees
/// it; the validator and Module::load check it and throw InternalError on a violation.
using Program = std::vector<ModuleItem>;

/// @brief The FunctionDecl that @p item points to, sharing ownership with @p item, or null if
///        @p item is not a FunctionDecl. Uses `kind()` and `std::static_pointer_cast`.
/// @param item A module item; borrowed.
/// @return A pointer that shares @p item's control block (plan 0001 D3), or null if
///         `item->kind()` is not NodeKind::FunctionDecl.
/// @throws InternalError if @p item is null.
[[nodiscard]] std::shared_ptr<const FunctionDecl> asFunctionDecl(const ModuleItem& item);

} // namespace philios
