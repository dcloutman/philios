#include "runtime/environment/environment.hpp"

#include "common/errors.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace philios {

Environment::Environment(std::shared_ptr<Environment> enclosingScope) noexcept
    : enclosingScope_(std::move(enclosingScope)) {}

Environment::~Environment() {
    while (enclosingScope_ != nullptr && enclosingScope_.use_count() == 1U) {
        std::shared_ptr<Environment> next = std::move(enclosingScope_->enclosingScope_);
        enclosingScope_ = std::move(next);
    }
    enclosingScope_.reset();
}

void Environment::define(std::string_view name, std::shared_ptr<Object> value) {
    if (!value) {
        throw InternalError("Environment::define: null value");
    }
    if (bindings_.contains(name)) {
        throw InternalError("Environment::define: duplicate binding");
    }
    bindings_.emplace(std::string(name), std::move(value));
}

std::optional<std::shared_ptr<Object>> Environment::lookup(std::string_view name) const {
    // Iterative walk of the enclosing-scope chain: no native recursion per scope (style guide;
    // security review SEC-0001-05). Each scope is kept alive by the scope before it.
    for (const Environment* scope = this; scope != nullptr; scope = scope->enclosingScope_.get()) {
        const auto found = scope->bindings_.find(name);
        if (found != scope->bindings_.end()) {
            return found->second;
        }
    }
    return std::nullopt;
}

const std::shared_ptr<Environment>& Environment::enclosingScope() const noexcept {
    return enclosingScope_;
}

void Environment::releaseBindings() noexcept {
    std::map<std::string, std::shared_ptr<Object>, std::less<>> empty;
    bindings_.swap(empty);
}

} // namespace philios
