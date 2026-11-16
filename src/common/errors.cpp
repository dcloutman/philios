#include "common/errors.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace philios {
namespace {

[[nodiscard]] constexpr char hexDigit(unsigned int nibble) noexcept {
    return static_cast<char>(nibble < 10U ? ('0' + nibble) : ('A' + (nibble - 10U)));
}

[[nodiscard]] std::array<char, 4> hexEscape(unsigned char byte) noexcept {
    return std::array<char, 4>{'\\', 'x', hexDigit(byte >> 4U), hexDigit(byte & 0x0FU)};
}

} // namespace

std::string_view errorClassName(ErrorClass errorClass) noexcept {
    switch (errorClass) {
    case ErrorClass::Lexical:
        return "lexical";
    case ErrorClass::Syntax:
        return "syntax";
    case ErrorClass::Declaration:
        return "declaration";
    case ErrorClass::Runtime:
        return "runtime";
    }
    return "runtime";
}

PhiliosError::PhiliosError(ErrorClass errorClass, SourceLocation location,
                           const std::string& message)
    : std::runtime_error(message), errorClass_(errorClass), location_(location) {}

ErrorClass PhiliosError::errorClass() const noexcept {
    return errorClass_;
}

SourceLocation PhiliosError::location() const noexcept {
    return location_;
}

LexicalError::LexicalError(SourceLocation location, const std::string& message)
    : PhiliosError(ErrorClass::Lexical, location, message) {}

SyntaxError::SyntaxError(SourceLocation location, const std::string& message)
    : PhiliosError(ErrorClass::Syntax, location, message) {}

DeclarationError::DeclarationError(SourceLocation location, const std::string& message)
    : PhiliosError(ErrorClass::Declaration, location, message) {}

RuntimeError::RuntimeError(RuntimeErrorKind kind, SourceLocation location,
                           const std::string& message)
    : PhiliosError(ErrorClass::Runtime, location, message), kind_(kind) {}

RuntimeErrorKind RuntimeError::kind() const noexcept {
    return kind_;
}

StartupError::StartupError(const std::string& reason) : std::runtime_error(reason) {}

InternalError::InternalError(const std::string& message) : std::runtime_error(message) {}

std::string escapeForDiagnostic(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char byte : text) {
        const auto value = static_cast<unsigned char>(byte);
        if (value < 0x20U || value == 0x7FU) {
            const auto replacement = hexEscape(value);
            escaped.append(replacement.begin(), replacement.end());
            continue;
        }
        escaped.push_back(byte);
    }
    return escaped;
}

std::string formatDiagnostic(const PhiliosError& error) {
    return "main.phil:" + std::to_string(error.location().line) + ":" +
           std::to_string(error.location().column) + ": " +
           std::string(errorClassName(error.errorClass())) + " error: " +
           escapeForDiagnostic(error.what());
}

std::string formatDiagnostic(const StartupError& error) {
    return "philios: startup error: main.phil: " + escapeForDiagnostic(error.what());
}

std::string formatDiagnostic(const InternalError& error) {
    return "philios: internal error: " + escapeForDiagnostic(error.what());
}

} // namespace philios
