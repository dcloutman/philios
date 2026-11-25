#pragma once

/// @file
/// @brief The built-in `Integer` type: 64-bit signed (plan 0001 §3.6, §3.8, D2, D20).

#include "runtime/object_model/object.hpp"

#include <cstdint>

namespace philios {

/// @brief An immutable 64-bit signed integer. Its storage is private to this class.
///
/// No arithmetic exists in this plan; literal parsing is in the frontend (parseDecimalInteger).
class Integer final : public ImmutableObject {
public:
    /// @brief Constructs an Integer holding @p value.
    /// @throws Nothing.
    explicit Integer(std::int64_t value) noexcept;

    /// @return ObjectType::Integer.
    /// @throws Nothing.
    [[nodiscard]] ObjectType objectType() const noexcept override;

    /// @brief The process exit status for this value when returned by `main` (§3.8, D20).
    ///
    /// Computes `r = v % 256; if (r < 0) r += 256;` which cannot overflow.
    /// @return A value in [0, 255]; for example -1 -> 255, INT64_MIN -> 0, 300 -> 44.
    /// @throws Nothing.
    [[nodiscard]] int exitStatus() const noexcept;

    /// @brief Whether this Integer holds the same exact `std::int64_t` value as @p other.
    /// @throws Nothing.
    [[nodiscard]] bool equals(const Integer& other) const noexcept;

private:
    std::int64_t value_;
};

} // namespace philios
