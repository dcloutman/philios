#pragma once

/// @file
/// @brief Root of the Philios object model: Object, ImmutableObject, ObjectType (plan 0001 §3.6,
///        D1).
///
/// Dependency rule (§7.1): `src/runtime/object_model/` includes nothing from
/// `src/runtime/evaluator/`.

namespace philios {

/// @brief The runtime class tag of every concrete object. The only mechanism used to decide a
///        Philios type (no `dynamic_cast` or `typeid`, §3.6).
enum class ObjectType {
    Integer,  ///< Integer
    String,   ///< String
    Null,     ///< NullObject
    Function, ///< Function
    Boolean,  ///< Boolean (plan 0003 §4.3)
    Symbol,   ///< Symbol (plan 0003 §4.3, §4.9)
};

/// @brief Ancestor of every Philios value (D1). There are no native primitives.
///
/// Every runtime value is a non-null `std::shared_ptr` to an Object. Not copyable or movable
/// (CERT OOP51); deleted through base pointers, so the destructor is public and virtual.
class Object {
public:
    virtual ~Object() = default;

    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;
    Object(Object&&) = delete;
    Object& operator=(Object&&) = delete;

    /// @brief The concrete runtime class.
    /// @throws Nothing.
    [[nodiscard]] virtual ObjectType objectType() const noexcept = 0;

protected:
    Object() noexcept = default;
};

/// @brief Base of the built-in immutable types (`Integer`, `String`, `Boolean`).
///
/// Contents never change after construction, but lifetime is still governed by owning
/// references: hold an owning reference across reentrant calls. Must not hold owning references
/// to mutable objects (style guide).
class ImmutableObject : public Object {
protected:
    ImmutableObject() noexcept = default;
};

} // namespace philios
