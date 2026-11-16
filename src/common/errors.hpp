#pragma once

/// @file
/// @brief Error hierarchy, source locations, and diagnostic formatting (plan 0001 §4, D45).
///
/// Every Philios language error derives from PhiliosError and carries a SourceLocation. Host I/O
/// failures (StartupError) and internal invariant failures (InternalError) are distinct types
/// outside that hierarchy. Resource exhaustion means `std::bad_alloc` only; the call-depth and
/// evaluation-depth limits are RuntimeError kinds (D7, D24). All types derive from
/// `std::runtime_error`, so copying an error object cannot throw (CERT ERR60).

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace philios {

/// @brief A 1-based position in `main.phil`. Columns count bytes, not characters (D8).
///
/// Plain aggregate; copying never throws.
struct SourceLocation {
    std::size_t line{1};   ///< 1-based line number; each line break adds 1 (§3.3).
    std::size_t column{1}; ///< 1-based byte column; reset to 1 after each line break.

    /// @brief Member-wise equality.
    [[nodiscard]] friend constexpr bool operator==(const SourceLocation&,
                                                   const SourceLocation&) noexcept = default;
};

/// @brief The `<class>` field of a located diagnostic (§4).
enum class ErrorClass {
    Lexical,     ///< Detected while lexing; exit 65.
    Syntax,      ///< Detected while parsing; exit 65.
    Declaration, ///< Detected by whole-file validation; exit 65.
    Runtime,     ///< Detected during evaluation; exit 70.
};

/// @brief One distinct kind per runtime condition in the §4 table, so tests can tell them apart.
enum class RuntimeErrorKind {
    UndefinedIdentifier,      ///< Name lookup failed at evaluation (D31b).
    NotCallable,              ///< Callee is not a `Function` (D14, D16).
    ArgumentCount,            ///< Wrong number of arguments (D14).
    ArgumentType,             ///< An argument's runtime type does not match its parameter (D14).
    ReturnType,               ///< A returned value does not match the declared return type.
    MissingReturn,            ///< A non-`null`, non-`main` function ended without `return` (D13).
    CallDepthExceeded,        ///< Call depth would exceed `kMaxCallDepth` (D7, D24 precedence).
    EvaluationDepthExceeded,  ///< Evaluation depth would exceed `kMaxEvalDepth` (D24).
};

/// @brief Returns the lowercase class name used in diagnostics: `lexical`, `syntax`,
///        `declaration`, or `runtime`.
/// @param errorClass Any enumerator of ErrorClass.
/// @return A view of a string literal with static storage duration.
/// @throws Nothing.
[[nodiscard]] std::string_view errorClassName(ErrorClass errorClass) noexcept;

/// @brief Base of every Philios language error (style guide, Errors & Diagnostics).
///
/// Carries the error class and the location of the first byte of the offending token or
/// construct (§4). The message is stored in the `std::runtime_error` base, so copying never
/// throws (CERT ERR60). Error objects copy any text they report and never refer into the source
/// buffer (§3.9). Not constructible directly; use a derived type.
class PhiliosError : public std::runtime_error {
public:
    /// @brief The diagnostic class of this error.
    /// @throws Nothing.
    [[nodiscard]] ErrorClass errorClass() const noexcept;

    /// @brief The location of the offending token or construct.
    /// @throws Nothing.
    [[nodiscard]] SourceLocation location() const noexcept;

protected:
    /// @brief Constructs the base.
    /// @param errorClass The diagnostic class.
    /// @param location Location of the first byte of the offending token or construct.
    /// @param message Unescaped message prose; escaping happens when formatting (§4).
    /// @throws std::bad_alloc If the message cannot be copied.
    PhiliosError(ErrorClass errorClass, SourceLocation location, const std::string& message);

private:
    ErrorClass errorClass_;
    SourceLocation location_;
};

/// @brief A lexical error (class `lexical`, exit 65).
class LexicalError final : public PhiliosError {
public:
    /// @brief Constructs a lexical error at @p location.
    /// @throws std::bad_alloc If the message cannot be copied.
    LexicalError(SourceLocation location, const std::string& message);
};

/// @brief A syntax error (class `syntax`, exit 65).
class SyntaxError final : public PhiliosError {
public:
    /// @brief Constructs a syntax error at @p location.
    /// @throws std::bad_alloc If the message cannot be copied.
    SyntaxError(SourceLocation location, const std::string& message);
};

/// @brief A declaration error found by whole-file validation (class `declaration`, exit 65).
class DeclarationError final : public PhiliosError {
public:
    /// @brief Constructs a declaration error at @p location.
    /// @throws std::bad_alloc If the message cannot be copied.
    DeclarationError(SourceLocation location, const std::string& message);
};

/// @brief A runtime error (class `runtime`, exit 70) with a distinct RuntimeErrorKind.
class RuntimeError final : public PhiliosError {
public:
    /// @brief Constructs a runtime error of @p kind at @p location.
    /// @post kind() == @p kind.
    /// @throws std::bad_alloc If the message cannot be copied.
    RuntimeError(RuntimeErrorKind kind, SourceLocation location, const std::string& message);

    /// @brief The runtime condition that raised this error.
    /// @throws Nothing.
    [[nodiscard]] RuntimeErrorKind kind() const noexcept;

private:
    RuntimeErrorKind kind_;
};

/// @brief A host I/O failure while loading `main.phil` (exit 66, §3.2). Not a PhiliosError.
class StartupError final : public std::runtime_error {
public:
    /// @brief Constructs a startup error.
    /// @param reason Unescaped reason text, reported after `main.phil: `.
    /// @throws std::bad_alloc If the reason cannot be copied.
    explicit StartupError(const std::string& reason);
};

/// @brief An internal invariant failure, i.e. an interpreter bug (exit 70). Never a script error.
///
/// Thrown in every build instead of `assert` (style guide, CERT ERR50).
class InternalError final : public std::runtime_error {
public:
    /// @brief Constructs an internal error.
    /// @param message Unescaped message text.
    /// @throws std::bad_alloc If the message cannot be copied.
    explicit InternalError(const std::string& message);
};

/// @brief Escapes text for echoing in a diagnostic (§4, T-SEC-7).
///
/// @param text Arbitrary bytes, possibly script-derived.
/// @return A copy in which every control character (including CR) and every terminal escape
///         sequence introducer is replaced by a printable escape. Printable ASCII is unchanged.
/// @post The result contains no byte below 0x20 and no 0x7F.
/// @throws std::bad_alloc On allocation failure.
[[nodiscard]] std::string escapeForDiagnostic(std::string_view text);

/// @brief Formats a located error as `main.phil:<line>:<column>: <class> error: <message>`.
/// @param error Any language error.
/// @return The escaped diagnostic line, without a trailing newline.
/// @throws std::bad_alloc On allocation failure.
[[nodiscard]] std::string formatDiagnostic(const PhiliosError& error);

/// @brief Formats a startup error as `philios: startup error: main.phil: <reason>`.
/// @return The escaped diagnostic line, without a trailing newline.
/// @throws std::bad_alloc On allocation failure.
[[nodiscard]] std::string formatDiagnostic(const StartupError& error);

/// @brief Formats an internal error as `philios: internal error: <message>`.
/// @return The escaped diagnostic line, without a trailing newline.
/// @throws std::bad_alloc On allocation failure.
[[nodiscard]] std::string formatDiagnostic(const InternalError& error);

} // namespace philios
