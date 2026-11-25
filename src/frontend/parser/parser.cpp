#include "frontend/parser/parser.hpp"

#include "common/errors.hpp"
#include "frontend/ast/ast.hpp"
#include "frontend/lexer/token.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace philios {
namespace {

[[nodiscard]] bool isBaseTypeToken(TokenKind kind) noexcept {
    return kind == TokenKind::TypeInteger || kind == TokenKind::TypeString ||
           kind == TokenKind::TypeFunction || kind == TokenKind::TypeObject ||
           kind == TokenKind::KeywordNull || kind == TokenKind::TypeBoolean ||
           kind == TokenKind::TypeSymbol;
}

[[nodiscard]] bool isNullableBaseTypeToken(TokenKind kind) noexcept {
    return kind == TokenKind::TypeInteger || kind == TokenKind::TypeString ||
           kind == TokenKind::TypeFunction || kind == TokenKind::TypeObject ||
           kind == TokenKind::TypeBoolean || kind == TokenKind::TypeSymbol;
}

[[nodiscard]] bool isNonDeclarableTypeWord(TokenKind kind) noexcept {
    return kind == TokenKind::TypeImmutableObject || kind == TokenKind::TypeNullObject ||
           kind == TokenKind::TypeUnsignedInteger || kind == TokenKind::TypeFloat;
}

[[nodiscard]] bool isAdjacentQuestion(const Token& questionToken, const Token& nextToken) noexcept {
    return nextToken.location.line == questionToken.location.line &&
           nextToken.location.column == questionToken.location.column + 1;
}

[[nodiscard]] TypeName toTypeName(TokenKind kind) {
    switch (kind) {
    case TokenKind::TypeInteger:
        return TypeName::Integer;
    case TokenKind::TypeString:
        return TypeName::String;
    case TokenKind::TypeFunction:
        return TypeName::Function;
    case TokenKind::TypeObject:
        return TypeName::Object;
    case TokenKind::KeywordNull:
        return TypeName::Null;
    case TokenKind::TypeBoolean:
        return TypeName::Boolean;
    case TokenKind::TypeSymbol:
        return TypeName::Symbol;
    default:
        throw InternalError("Parser: invalid type token");
    }
}

} // namespace

class ParserImpl final {
public:
    ParserImpl(std::span<const Token> tokens, std::size_t maxNesting) noexcept
        : tokens_(tokens), maxNesting_(maxNesting) {}

    [[nodiscard]] Program parseProgram() {
        Program program;
        while (!isAtEnd()) {
            if (peek().kind == TokenKind::EndOfInput) {
                break;
            }
            program.push_back(parseModuleItem());
        }
        [[maybe_unused]] const Token& end = expect(TokenKind::EndOfInput, "expected end of input");
        return program;
    }

private:
    [[nodiscard]] const Token& peek(std::size_t offset = 0) const {
        const std::size_t index = position_ + offset;
        if (index >= tokens_.size()) {
            throw InternalError("Parser: token stream ended unexpectedly");
        }
        return tokens_[index];
    }

    [[nodiscard]] bool isAtEnd() const {
        return peek().kind == TokenKind::EndOfInput;
    }

    [[nodiscard]] const Token& advance() {
        const Token& token = peek();
        if (!isAtEnd()) {
            ++position_;
        }
        return token;
    }

    [[nodiscard]] const Token& expect(TokenKind kind, std::string_view message) {
        if (peek().kind != kind) {
            throw SyntaxError(peek().location, std::string(message));
        }
        return advance();
    }

    void enterNesting(SourceLocation location) {
        if (nesting_ >= maxNesting_) {
            throw SyntaxError(location, "nesting exceeds parser limit");
        }
        ++nesting_;
    }

    void leaveNesting() noexcept {
        if (nesting_ > 0) {
            --nesting_;
        }
    }

    [[nodiscard]] DeclaredType parseDeclaredType(std::string_view expectedMessage) {
        if (peek().kind == TokenKind::Question) {
            const Token& questionToken = advance();
            const Token& nextToken = peek();
            if (!isAdjacentQuestion(questionToken, nextToken)) {
                throw SyntaxError(questionToken.location, std::string(expectedMessage));
            }
            if (!isNullableBaseTypeToken(nextToken.kind)) {
                throw SyntaxError(nextToken.location, std::string(expectedMessage));
            }
            const Token& baseToken = advance();
            return DeclaredType{toTypeName(baseToken.kind), true};
        }

        if (!isBaseTypeToken(peek().kind) && !isNonDeclarableTypeWord(peek().kind)) {
            throw SyntaxError(peek().location, std::string(expectedMessage));
        }
        const Token& typeToken = advance();
        if (!isBaseTypeToken(typeToken.kind)) {
            throw SyntaxError(typeToken.location, std::string(expectedMessage));
        }
        return DeclaredType{toTypeName(typeToken.kind), false};
    }

    [[nodiscard]] std::shared_ptr<const FunctionDecl> parseFunctionDecl() {
        [[maybe_unused]] const Token& functionKeyword =
            expect(TokenKind::KeywordFunction, "expected function");
        const Token& nameToken = expect(TokenKind::Identifier, "expected function name");
        [[maybe_unused]] const Token& leftParen = expect(TokenKind::LeftParen, "expected (");

        std::vector<std::unique_ptr<Param>> params;
        if (peek().kind != TokenKind::RightParen) {
            for (;;) {
                const Token& paramName = expect(TokenKind::Identifier, "expected parameter name");
                [[maybe_unused]] const Token& paramColon =
                    expect(TokenKind::Colon, "expected : after parameter name");
                params.push_back(std::make_unique<Param>(paramName.location, paramName.text,
                                                         parseDeclaredType("expected parameter type")));
                if (peek().kind == TokenKind::Comma) {
                    [[maybe_unused]] const Token& comma = advance();
                    if (peek().kind == TokenKind::RightParen) {
                        throw SyntaxError(peek().location, "trailing comma in parameters");
                    }
                    continue;
                }
                break;
            }
        }

        [[maybe_unused]] const Token& rightParen = expect(TokenKind::RightParen, "expected )");

        std::optional<DeclaredType> returnType;
        [[maybe_unused]] const Token& headerColon =
            expect(TokenKind::Colon, "expected : before function body");
        if (peek().kind != TokenKind::Newline) {
            returnType = parseDeclaredType("expected return type");
            [[maybe_unused]] const Token& bodyColon =
                expect(TokenKind::Colon, "expected : before function body");
        }
        [[maybe_unused]] const Token& headerNewline =
            expect(TokenKind::Newline, "expected newline after function header");

        std::unique_ptr<Block> body = parseBlock();
        return std::make_shared<const FunctionDecl>(nameToken.location, nameToken.text,
                                                    std::move(params), returnType,
                                                    std::move(body));
    }

    [[nodiscard]] std::unique_ptr<Block> parseBlock() {
        const Token& indent = expect(TokenKind::Indent, "expected indented block");
        enterNesting(indent.location);
        std::vector<std::unique_ptr<ASTNode>> statements;
        if (peek().kind == TokenKind::Dedent) {
            leaveNesting();
            throw SyntaxError(peek().location, "empty block");
        }
        while (peek().kind != TokenKind::Dedent) {
            statements.push_back(parseStatement());
        }
        [[maybe_unused]] const Token& dedent = expect(TokenKind::Dedent, "expected dedent");
        leaveNesting();
        return std::make_unique<Block>(statements.front()->location(), std::move(statements));
    }

    [[nodiscard]] std::unique_ptr<ASTNode> parseStatement() {
        if (peek().kind == TokenKind::KeywordReturn) {
            return parseReturnStatement();
        }
        if (peek().kind == TokenKind::KeywordPass) {
            const Token& token = advance();
            [[maybe_unused]] const Token& newlineAfterPass =
                expect(TokenKind::Newline, "expected newline after pass");
            return std::make_unique<PassStmt>(token.location);
        }
        if (peek().kind == TokenKind::KeywordFunction) {
            throw SyntaxError(peek().location, "nested function declarations are not allowed");
        }
        if (peek().kind == TokenKind::KeywordLet || peek().kind == TokenKind::KeywordConst) {
            const TokenKind declarationKind = peek().kind;
            VariableDeclarationParts parts = parseVariableDeclarationParts();
            if (declarationKind == TokenKind::KeywordLet) {
                return std::make_unique<LetDecl>(parts.keywordLocation, parts.name,
                                                 parts.nameLocation, parts.type,
                                                 std::move(parts.initializer));
            }
            if (declarationKind == TokenKind::KeywordConst) {
                return std::make_unique<ConstDecl>(parts.keywordLocation, parts.name,
                                                   parts.nameLocation, parts.type,
                                                   std::move(parts.initializer));
            }
            throw InternalError("Parser: invalid declaration kind");
        }
        if (peek().kind == TokenKind::KeywordSymbol) {
            const Token& nameToken = parseSymbolDeclarationName();
            return std::make_unique<SymbolDecl>(nameToken.location, nameToken.text);
        }

        // An identifier can start a call, an assignment, or a syntax error at the next token.
        if (peek().kind == TokenKind::Identifier) {
            if (peek(1).kind == TokenKind::LeftParen) {
                std::unique_ptr<CallExpr> call = parseCallExpression();
                [[maybe_unused]] const Token& newlineAfterExpr =
                    expect(TokenKind::Newline, "expected newline after expression statement");
                return std::make_unique<ExprStmt>(std::move(call));
            }
            if (peek(1).kind == TokenKind::Assign) {
                return parseAssignStatement();
            }
            // `a === b` and `a !== b`: the second `=` row of §5.2 wins (developer decision).
            if ((peek(1).kind == TokenKind::EqualEqual || peek(1).kind == TokenKind::BangEqual) &&
                peek(2).kind == TokenKind::Assign) {
                throw SyntaxError(peek(2).location, "invalid expression");
            }
            throw SyntaxError(peek(1).location, "invalid statement");
        }
        // Plan 0002 §3.3: `exprStmt := ( callExpr | pseudoCall ) NEWLINE`.
        if (peek().kind == TokenKind::KeywordEcho || peek().kind == TokenKind::KeywordTypeof) {
            std::unique_ptr<ASTNode> pseudoCall = parsePseudoCall();
            [[maybe_unused]] const Token& newlineAfterPseudoCall =
                expect(TokenKind::Newline, "expected newline after expression statement");
            return std::make_unique<ExprStmt>(std::move(pseudoCall));
        }
        throw SyntaxError(peek().location, "invalid statement");
    }

    [[nodiscard]] std::unique_ptr<ASTNode> parseReturnStatement() {
        const Token& keyword = expect(TokenKind::KeywordReturn, "expected return");
        std::unique_ptr<ASTNode> value;
        if (peek().kind != TokenKind::Newline) {
            value = parseExpression();
        }
        [[maybe_unused]] const Token& newlineAfterReturn =
            expect(TokenKind::Newline, "expected newline after return");
        return std::make_unique<ReturnStmt>(keyword.location, std::move(value));
    }

    [[nodiscard]] std::unique_ptr<ASTNode> parseExpression() {
        return parseEquality();
    }

    /// Parses `pseudoCall := ( 'echo' | 'typeof' ) '(' [ args ] ')'` (plan 0002 §3.3; D8, D16,
    /// D17, D20) into an EchoCall or TypeofCall located at the keyword.
    ///
    /// Rules to implement:
    /// - A keyword not followed by `(` is the plan 0001 D27 reserved-word syntax error at the
    ///   keyword (`return echo`, a statement `echo`, `typeof(echo)`).
    /// - The argument list is one parser nesting level, exactly like a call's: its `(` goes
    ///   through enterNesting, so level 257 fails at that `(`. A trailing comma is the plan 0001
    ///   syntax error at the `)`.
    /// - After the argument list is parsed, a count other than one is a syntax error at the
    ///   keyword (D17).
    /// - For `echo` only, an IntegerLiteral (positive or negative) or NullLiteral argument is a
    ///   syntax error at the argument's first byte (D16, D17). Other arguments are accepted.
    ///
    /// Pre: peek() is KeywordEcho or KeywordTypeof.
    /// Throws SyntaxError as above; InternalError; std::bad_alloc.
    [[nodiscard]] std::unique_ptr<ASTNode> parsePseudoCall() {
        const Token& keyword = advance();
        if (peek().kind != TokenKind::LeftParen) {
            throw SyntaxError(keyword.location, "expected pseudo-call argument list");
        }
        const Token& leftParen = advance();

        enterNesting(leftParen.location);
        std::vector<std::unique_ptr<ASTNode>> arguments;
        if (peek().kind != TokenKind::RightParen) {
            for (;;) {
                arguments.push_back(parseExpression());
                if (peek().kind == TokenKind::Comma) {
                    [[maybe_unused]] const Token& comma = advance();
                    if (peek().kind == TokenKind::RightParen) {
                        leaveNesting();
                        throw SyntaxError(peek().location, "trailing comma in argument list");
                    }
                    continue;
                }
                break;
            }
        }

        [[maybe_unused]] const Token& rightParen =
            expect(TokenKind::RightParen, "expected ) after arguments");
        leaveNesting();

        if (arguments.size() != 1) {
            throw SyntaxError(keyword.location, "pseudo-call requires exactly one argument");
        }

        std::unique_ptr<ASTNode> argument = std::move(arguments.front());
        if (keyword.kind == TokenKind::KeywordEcho) {
            const NodeKind argumentKind = argument->kind();
            if (argumentKind == NodeKind::IntegerLiteral || argumentKind == NodeKind::NullLiteral ||
                argumentKind == NodeKind::BooleanLiteral) {
                throw SyntaxError(argument->location(), "echo does not accept this literal");
            }
            return std::make_unique<EchoCall>(keyword.location, std::move(argument));
        }

        if (keyword.kind != TokenKind::KeywordTypeof) {
            throw InternalError("Parser: invalid pseudo-call keyword");
        }
        return std::make_unique<TypeofCall>(keyword.location, std::move(argument));
    }

    [[nodiscard]] std::unique_ptr<CallExpr> parseCallExpression() {
        const Token& name = expect(TokenKind::Identifier, "expected callee identifier");
        auto callee = std::make_unique<Identifier>(name.location, name.text);
        const Token& leftParen = expect(TokenKind::LeftParen, "expected ( after callee");

        enterNesting(leftParen.location);
        std::vector<std::unique_ptr<ASTNode>> arguments;
        if (peek().kind != TokenKind::RightParen) {
            for (;;) {
                arguments.push_back(parseExpression());
                if (peek().kind == TokenKind::Comma) {
                    [[maybe_unused]] const Token& comma = advance();
                    if (peek().kind == TokenKind::RightParen) {
                        leaveNesting();
                        throw SyntaxError(peek().location, "trailing comma in argument list");
                    }
                    continue;
                }
                break;
            }
        }

        [[maybe_unused]] const Token& rightParen =
            expect(TokenKind::RightParen, "expected ) after arguments");
        leaveNesting();
        return std::make_unique<CallExpr>(std::move(callee), std::move(arguments));
    }

    // ---------------------------------------------------------------------------------------------
    // Plan 0003 task 3 (§4.4, §4.12, §4.14.2, §5.2, §6.1). The functions below implement the
    // plan 0003 grammar and §5.2 syntax errors. Structure (architecture decisions §2.3, §6):
    // one parse function per precedence level, each delegating to the next tighter level, so a
    // later plan can insert levels 1–4, 6–8, and 10–12 without restructuring:
    //     parseExpression (`expression := equality`) → parseEquality → parseUnary → parsePrimary.
    // parseExpression keeps its name and call sites (return value, call and pseudo-call arguments)
    // and also parses declaration initializers and assignment right-hand sides; its current body
    // (the plan 0002 `primary` alternatives) becomes parsePrimary.
    // ---------------------------------------------------------------------------------------------

    /// The parts shared by `letDecl` and `constDecl` (§4.4), at module level and in function
    /// scope. The caller builds a LetDecl or ConstDecl from them, with `std::make_unique` in a
    /// block or `std::make_shared<const ConstDecl>` for a module item (§4.14.2).
    struct VariableDeclarationParts {
        /// Location of the `let` or `const` keyword (`initializer required` is reported there).
        SourceLocation keywordLocation{};
        /// The declared name's token text; a view into the source buffer through the token, copied
        /// by the node constructor (§4.14.2, "Name storage").
        std::string_view name{};
        /// Location of the declared name.
        SourceLocation nameLocation{};
        /// The declared type (§4.3: including `Boolean`, `Symbol`, `?Boolean`, `?Symbol`).
        DeclaredType type{TypeName::Null, false};
        /// The initializer after `=`, or null if there is no `=`.
        std::unique_ptr<ASTNode> initializer{};
    };

    /// Parses one `moduleItem := functionDecl | constDecl | symbolDecl` (§4.4, §4.14.2; D13).
    ///
    /// Rules to implement:
    /// - `function` → parseFunctionDecl. `const` → parseVariableDeclarationParts, then
    ///   `std::make_shared<const ConstDecl>`. `symbol` → parseSymbolDeclarationName, then
    ///   `std::make_shared<const SymbolDecl>` located at the name.
    /// - `let` at module level is a syntax error at `let` (C7, §5.2). Any other token is a syntax
    ///   error at that token (`return 1` keeps its plan 0001 location).
    /// - A module-level initializer is not inside a block, so it is parsed at nesting level 0
    ///   (§6.1): a chain of 256 operators is accepted, and the 257th operator is the error.
    ///
    /// parseProgram appends each result to the Program in source order; the result is never null
    /// and its kind() is FunctionDecl, ConstDecl, or SymbolDecl (the §4.14.2 invariant).
    /// Pre: peek() is not EndOfInput.
    /// Throws SyntaxError as above and as the called parse functions; InternalError;
    /// std::bad_alloc.
    [[nodiscard]] ModuleItem parseModuleItem() {
        if (peek().kind == TokenKind::KeywordFunction) {
            return parseFunctionDecl();
        }
        if (peek().kind == TokenKind::KeywordConst) {
            VariableDeclarationParts parts = parseVariableDeclarationParts();
            return std::make_shared<const ConstDecl>(parts.keywordLocation, parts.name,
                                                     parts.nameLocation, parts.type,
                                                     std::move(parts.initializer));
        }
        if (peek().kind == TokenKind::KeywordSymbol) {
            const Token& nameToken = parseSymbolDeclarationName();
            return std::make_shared<const SymbolDecl>(nameToken.location, nameToken.text);
        }
        throw SyntaxError(peek().location, "invalid module item");
    }

    /// Parses `( 'let' | 'const' ) IDENT ':' typeName [ '=' expression ] NEWLINE` (§4.4, §4.6,
    /// §4.7), including the NEWLINE.
    ///
    /// Rules to implement (§5.2; each error is at the token where the missing part is expected):
    /// - The name must be an Identifier; a reserved word (including the plan 0003 words) is an
    ///   error at the word (T3-LEX-3). Then `:`, then a type through parseDeclaredType (which
    ///   gains `Boolean`, `Symbol`, `?Boolean`, `?Symbol`; `? Boolean` fails at `?`, `?null` at
    ///   `null`, `Float` at the word, as plan 0002 for parameters).
    /// - After the type, either NEWLINE (no initializer) or `=` followed by an expression; an `=`
    ///   with no expression is an error at the token after it. A further `=` after the
    ///   initializer is an error at that `=` (D7, `let x: Integer = y = 1`).
    /// - A missing initializer is not a syntax error (`initializer required` is a declaration
    ///   error, §5.3).
    ///
    /// Pre: peek() is KeywordLet or KeywordConst.
    /// Throws SyntaxError as above; InternalError; std::bad_alloc.
    [[nodiscard]] VariableDeclarationParts parseVariableDeclarationParts() {
        if (peek().kind != TokenKind::KeywordLet && peek().kind != TokenKind::KeywordConst) {
            throw InternalError("Parser: expected let or const");
        }
        const Token& keywordToken = advance();
        const Token& nameToken = expect(TokenKind::Identifier, "expected declaration name");
        [[maybe_unused]] const Token& colon = expect(TokenKind::Colon, "expected :");
        const DeclaredType type = parseDeclaredType("expected type");

        std::unique_ptr<ASTNode> initializer;
        if (peek().kind == TokenKind::Assign) {
            [[maybe_unused]] const Token& assign = advance();
            initializer = parseExpression();
            if (peek().kind == TokenKind::Assign) {
                throw SyntaxError(peek().location, "expected newline");
            }
        }

        [[maybe_unused]] const Token& newline = expect(TokenKind::Newline, "expected newline");
        return VariableDeclarationParts{keywordToken.location, nameToken.text, nameToken.location,
                                        type, std::move(initializer)};
    }

    /// Parses `'symbol' IDENT NEWLINE` (§4.4, §4.9), including the NEWLINE, and returns the name
    /// token. The caller builds a SymbolDecl located at the name (as a block statement or, at
    /// module level, with `std::make_shared<const SymbolDecl>`).
    ///
    /// Rules to implement (§5.2): `symbol` without a name is an error at the token where the name
    /// is expected; a reserved word as the name is an error at the word; anything other than
    /// NEWLINE after the name (`symbol x = 1` at `=`, T3-SYM-5; `symbol x: Symbol` at `:`) is an
    /// error at that token.
    ///
    /// Pre: peek() is KeywordSymbol.
    /// Throws SyntaxError as above; InternalError.
    [[nodiscard]] const Token& parseSymbolDeclarationName() {
        [[maybe_unused]] const Token& symbolKeyword =
            expect(TokenKind::KeywordSymbol, "expected symbol");
        const Token& nameToken = expect(TokenKind::Identifier, "expected symbol name");
        [[maybe_unused]] const Token& newline = expect(TokenKind::Newline, "expected newline");
        return nameToken;
    }

    /// Parses `assignStmt := IDENT '=' expression NEWLINE` (§4.4, §4.8; D7) into an AssignStmt
    /// located at the target name, including the NEWLINE.
    ///
    /// Rules to implement: parseStatement dispatches here when an identifier at statement start
    /// is followed by `=` (a following `(` is a call; any other token is a syntax error at that
    /// token, §5.2). The right-hand side is one expression; a missing expression (`a = = b`) is
    /// an error at the token where it is expected, and a second `=` after it (`a = b = 1`) is an
    /// error at that `=`.
    ///
    /// Pre: peek() is Identifier and peek(1) is Assign.
    /// Throws SyntaxError as above; InternalError; std::bad_alloc.
    [[nodiscard]] std::unique_ptr<ASTNode> parseAssignStatement() {
        const Token& nameToken = expect(TokenKind::Identifier, "expected assignment target");
        [[maybe_unused]] const Token& assign = expect(TokenKind::Assign, "expected =");
        std::unique_ptr<ASTNode> value = parseExpression();
        if (peek().kind == TokenKind::Assign) {
            throw SyntaxError(peek().location, "expected newline");
        }
        [[maybe_unused]] const Token& newline = expect(TokenKind::Newline, "expected newline");
        return std::make_unique<AssignStmt>(nameToken.location, nameToken.text, std::move(value));
    }

    /// Parses `equality := unary { ( '==' | '!=' ) unary }` (§4.4, §4.11.2, §4.11.3, §4.12;
    /// D16) into a left-associative chain of BinaryExpr nodes (`Equal` or `NotEqual`), each
    /// located at its left operand with operatorLocation at its operator token.
    ///
    /// Rules to implement (§6.1):
    /// - Iterative: one loop and a counter, never one C++ frame per operator, so a
    ///   100,000-operator chain fails with the nesting syntax error, not a stack overflow.
    /// - Each operator adds one nesting level through enterNesting at the operator's location,
    ///   before its right operand is parsed; the levels are released when the chain ends (also on
    ///   the error path). Level 257 is a syntax error at the operator that would open it.
    /// - An operator with no following operand (end of line, `)`, `,`, `=`) is an error at the
    ///   token where the operand is expected; `=` is never part of an expression (D7).
    ///
    /// Throws SyntaxError as above and as parseUnary; InternalError; std::bad_alloc.
    [[nodiscard]] std::unique_ptr<ASTNode> parseEquality() {
        std::unique_ptr<ASTNode> left = parseUnary();
        if (peek().kind != TokenKind::EqualEqual && peek().kind != TokenKind::BangEqual) {
            return left;
        }

        std::size_t openedLevels = 0;
        try {
            while (peek().kind == TokenKind::EqualEqual || peek().kind == TokenKind::BangEqual) {
                const Token& operatorToken = advance();
                enterNesting(operatorToken.location);
                ++openedLevels;
                std::unique_ptr<ASTNode> right = parseUnary();
                const BinaryOperator op = operatorToken.kind == TokenKind::EqualEqual
                                              ? BinaryOperator::Equal
                                              : BinaryOperator::NotEqual;
                left = std::make_unique<BinaryExpr>(op, operatorToken.location, std::move(left),
                                                    std::move(right));
            }
        } catch (...) {
            for (std::size_t index = 0; index < openedLevels; ++index) {
                leaveNesting();
            }
            throw;
        }
        for (std::size_t index = 0; index < openedLevels; ++index) {
            leaveNesting();
        }
        return left;
    }

    /// Parses `unary := '!' primary | primary` (§4.4, §4.11.1, §4.12; DA5) into a UnaryExpr
    /// (`Not`) located at the `!`, or returns the primary.
    ///
    /// Rules to implement:
    /// - `!` adds one nesting level for its operand through enterNesting at the `!` (§6.1), and
    ///   releases it after the operand.
    /// - `!` directly followed by another `!` (`!!x`, `! !x`) is a syntax error at the second `!`
    ///   with the message `repeated ! is not allowed` (DA5, §5.2). This is checked before any
    ///   further token is consumed, so a run of 100,000 `!` fails at the second one.
    /// - `!` with no following operand is an error at the token where the operand is expected.
    ///   A `!` inside a call argument starts a new expression (`!f(!x)` is valid).
    ///
    /// Throws SyntaxError as above and as parsePrimary; InternalError; std::bad_alloc.
    [[nodiscard]] std::unique_ptr<ASTNode> parseUnary() {
        if (peek().kind != TokenKind::Bang) {
            return parsePrimary();
        }

        const Token& bangToken = advance();
        if (peek().kind == TokenKind::Bang) {
            throw SyntaxError(peek().location, "repeated ! is not allowed");
        }

        enterNesting(bangToken.location);
        try {
            std::unique_ptr<ASTNode> operand = parsePrimary();
            leaveNesting();
            return std::make_unique<UnaryExpr>(bangToken.location, UnaryOperator::Not,
                                               std::move(operand));
        } catch (...) {
            leaveNesting();
            throw;
        }
    }

    /// Parses `primary := INTEGER | STRING | 'null' | 'true' | 'false' | IDENT | callExpr |
    /// pseudoCall` (§4.4): the plan 0002 alternatives that parseExpression parses today, plus
    /// `true` and `false` as BooleanLiteral nodes located at the keyword.
    ///
    /// Rules to implement: any other token is a syntax error at that token. parsePseudoCall's
    /// `echo` literal rule extends to BooleanLiteral: `echo(true)` and `echo(false)` are syntax
    /// errors at the argument's first byte; `typeof(true)` is accepted (§4.11.5, C10).
    ///
    /// Throws SyntaxError as above and as the called parse functions; InternalError;
    /// std::bad_alloc.
    [[nodiscard]] std::unique_ptr<ASTNode> parsePrimary() {
        switch (peek().kind) {
        case TokenKind::IntegerLiteral: {
            const Token& token = advance();
            return std::make_unique<IntegerLiteral>(token.location, token.integerValue);
        }
        case TokenKind::StringLiteral: {
            const Token& token = advance();
            return std::make_unique<StringLiteral>(token.location, token.stringValue);
        }
        case TokenKind::KeywordNull: {
            const Token& token = advance();
            return std::make_unique<NullLiteral>(token.location);
        }
        case TokenKind::KeywordTrue: {
            const Token& token = advance();
            return std::make_unique<BooleanLiteral>(token.location, true);
        }
        case TokenKind::KeywordFalse: {
            const Token& token = advance();
            return std::make_unique<BooleanLiteral>(token.location, false);
        }
        case TokenKind::Identifier: {
            if (peek(1).kind == TokenKind::LeftParen) {
                return parseCallExpression();
            }
            const Token& token = advance();
            return std::make_unique<Identifier>(token.location, token.text);
        }
        case TokenKind::KeywordEcho:
        case TokenKind::KeywordTypeof:
            return parsePseudoCall();
        default:
            throw SyntaxError(peek().location, "invalid expression");
        }
    }

    std::span<const Token> tokens_;
    std::size_t position_{0};
    std::size_t maxNesting_;
    std::size_t nesting_{0};
};

Parser::Parser(std::span<const Token> tokens, std::size_t maxNesting) noexcept
    : tokens_(tokens), maxNesting_(maxNesting) {}

Program Parser::parseProgram() {
    if (consumed_) {
        throw InternalError("Parser::parseProgram called more than once");
    }
    consumed_ = true;
    ParserImpl impl(tokens_, maxNesting_);
    return impl.parseProgram();
}

} // namespace philios
