#include "runtime/evaluator/module.hpp"

#include "analysis/semantic_analyzer/validator.hpp"
#include "common/errors.hpp"
#include "frontend/lexer/lexer.hpp"
#include "frontend/lexer/token.hpp"
#include "frontend/parser/parser.hpp"
#include "runtime/environment/environment.hpp"
#include "runtime/evaluator/execution_context.hpp"
#include "runtime/object_model/function.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace philios {

std::unique_ptr<Module> Module::load(ExecutionContext& context, std::string source) {
    std::unique_ptr<Module> module = std::make_unique<Module>(Passkey{}, std::move(source));

    Lexer lexer(module->source_);
    const std::vector<Token> tokens = lexer.tokenize();

    Parser parser(tokens, context.kMaxParseNesting);
    module->program_ = parser.parseProgram();
    validateProgram(module->program_);

    module->environment_ = std::make_shared<Environment>(nullptr);
    for (const std::shared_ptr<const FunctionDecl>& declaration : module->program_) {
        if (!declaration) {
            throw InternalError("Module::load: null function declaration");
        }
        module->environment_->define(
            declaration->name(),
            std::make_shared<Function>(declaration, module->environment_));
    }

    return module;
}

Module::Module([[maybe_unused]] Passkey passkey, std::string source) noexcept
    : source_(std::move(source)) {}

Module::~Module() {
    release();
}

void Module::release() noexcept {
    if (environment_ != nullptr) {
        environment_->releaseBindings();
        environment_.reset();
    }

    program_.clear();
    source_.clear();
}

const std::shared_ptr<Environment>& Module::environment() const noexcept {
    return environment_;
}

} // namespace philios
