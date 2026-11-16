#include "runtime/object_model/function.hpp"

#include "common/errors.hpp"

#include <memory>
#include <utility>

namespace philios {

Function::Function(std::shared_ptr<const FunctionDecl> declaration,
                   std::shared_ptr<Environment> closureEnv)
    : declaration_(std::move(declaration)), closureEnv_(std::move(closureEnv)) {
    if (!declaration_ || !closureEnv_) {
        throw InternalError("Function: null declaration or closure environment");
    }
}

ObjectType Function::objectType() const noexcept {
    return ObjectType::Function;
}

const std::shared_ptr<const FunctionDecl>& Function::declaration() const noexcept {
    return declaration_;
}

const std::shared_ptr<Environment>& Function::closureEnv() const noexcept {
    return closureEnv_;
}

} // namespace philios
