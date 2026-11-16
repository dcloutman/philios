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

[[nodiscard]] bool isTypeToken(TokenKind kind) noexcept {
    return kind == TokenKind::TypeInteger || kind == TokenKind::TypeString ||
           kind == TokenKind::TypeFunction || kind == TokenKind::KeywordNull;
}

[[nodiscard]] TypeName toTypeName(TokenKind kind) {
    switch (kind) {
    case TokenKind::TypeInteger:
        return TypeName::Integer;
    case TokenKind::TypeString:
        return TypeName::String;
    case TokenKind::TypeFunction:
        return TypeName::Function;
    case TokenKind::KeywordNull:
        return TypeName::Null;
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
            if (peek().kind != TokenKind::KeywordFunction) {
                throw SyntaxError(peek().location, "expected function declaration");
            }
            program.push_back(parseFunctionDecl());
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
                if (!isTypeToken(peek().kind)) {
                    throw SyntaxError(peek().location, "expected parameter type");
                }
                const Token& typeToken = advance();
                params.push_back(std::make_unique<Param>(paramName.location, paramName.text,
                                                         toTypeName(typeToken.kind)));
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

        std::optional<TypeName> returnType;
        if (peek().kind == TokenKind::Colon && isTypeToken(peek(1).kind)) {
            [[maybe_unused]] const Token& returnTypeColon = advance();
            returnType = toTypeName(advance().kind);
        }

        [[maybe_unused]] const Token& bodyColon =
            expect(TokenKind::Colon, "expected : before function body");
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

        // An identifier can start only `exprStmt := callExpr NEWLINE` (§3.4), so a token other
        // than `(` after it is the offending token: `fo/*c*/o()` fails at `o` (D40, T-LEX-11).
        if (peek().kind == TokenKind::Identifier) {
            std::unique_ptr<CallExpr> call = parseCallExpression();
            [[maybe_unused]] const Token& newlineAfterExpr =
                expect(TokenKind::Newline, "expected newline after expression statement");
            return std::make_unique<ExprStmt>(std::move(call));
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
        case TokenKind::Identifier: {
            if (peek(1).kind == TokenKind::LeftParen) {
                return parseCallExpression();
            }
            const Token& token = advance();
            return std::make_unique<Identifier>(token.location, token.text);
        }
        default:
            throw SyntaxError(peek().location, "invalid expression");
        }
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
