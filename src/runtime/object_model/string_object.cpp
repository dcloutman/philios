#include "runtime/object_model/string_object.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace philios {

String::String(std::string bytes) noexcept : bytes_(std::move(bytes)) {}

ObjectType String::objectType() const noexcept {
    return ObjectType::String;
}

std::string_view String::bytes() const noexcept {
    return bytes_;
}

bool String::contentEquals(std::string_view bytes) const noexcept {
    return bytes_ == bytes;
}

} // namespace philios
