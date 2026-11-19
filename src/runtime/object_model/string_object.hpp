#pragma once

/// @file
/// @brief The built-in `String` type (plan 0001 §3.6).

#include "runtime/object_model/object.hpp"

#include <string>
#include <string_view>

namespace philios {

/// @brief An immutable byte string. Its storage is private to this class.
///
/// No string operations exist in this plan (no concatenation or interpolation).
class String final : public ImmutableObject {
public:
    /// @brief Constructs a String owning @p bytes.
    /// @throws Nothing.
    explicit String(std::string bytes) noexcept;

    /// @return ObjectType::String.
    /// @throws Nothing.
    [[nodiscard]] ObjectType objectType() const noexcept override;

    /// @brief The owned bytes, borrowed for the duration of the call.
    /// @throws Nothing.
    [[nodiscard]] std::string_view bytes() const noexcept;

    /// @brief Whether this String's bytes equal @p bytes exactly (T-FN-1).
    /// @param bytes Borrowed for the duration of the call.
    /// @throws Nothing.
    [[nodiscard]] bool contentEquals(std::string_view bytes) const noexcept;

private:
    std::string bytes_;
};

} // namespace philios
