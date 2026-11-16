#pragma once

/// @file
/// @brief `NullObject`, the class of Philios `null` (plan 0001 §3.6, D18).

#include "runtime/object_model/object.hpp"

namespace philios {

/// @brief The class of `null`.
///
/// Exactly one instance exists per ExecutionContext, which creates and owns it (never
/// process-wide). Every `null` produced during an execution is that context's instance (T-NULL-4).
class NullObject final : public Object {
public:
    /// @throws Nothing.
    NullObject() noexcept = default;

    /// @return ObjectType::Null.
    /// @throws Nothing.
    [[nodiscard]] ObjectType objectType() const noexcept override;
};

} // namespace philios
