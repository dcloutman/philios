#include "support/source_pipeline.hpp"

#include "frontend/ast/ast.hpp"
#include "frontend/lexer/lexer.hpp"
#include "frontend/lexer/token.hpp"
#include "frontend/parser/parser.hpp"
#include "runtime/evaluator/execution_context.hpp"

#include <string_view>
#include <vector>

namespace philios::test {

std::vector<Token> lexSource(std::string_view source) {
    Lexer lexer(source);
    return lexer.tokenize();
}

Program parseSource(std::string_view source) {
    const std::vector<Token> tokens = lexSource(source);
    Parser parser(tokens, ExecutionContext::kMaxParseNesting);
    return parser.parseProgram();
}

} // namespace philios::test
