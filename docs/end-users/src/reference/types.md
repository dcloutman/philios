# Language Reference: Types

Every value in Philios is an object. Philios has no primitive types.

## Declarable Types
These names may be used as parameter types and return types.

| Declared type | Accepts runtime values of class |
|---|---|
| `Integer` | `Integer` |
| `String` | `String` |
| `Function` | `Function` |
| `Object` | `Integer`, `String`, `Function` (every non-`null` value) |
| `null` | `NullObject` (`null`) only |
| `?Integer` | `Integer` or `null` |
| `?String` | `String` or `null` |
| `?Function` | `Function` or `null` |
| `?Object` | `Integer`, `String`, `Function`, or `null` |

Philios checks argument types at call time and return types when a function returns. Philios does not convert values.

## `Integer`
- `Integer` is a signed 64-bit whole number.
- Positive integer literals range from `0` to `9223372036854775807`.
- Negative integer literals range from `-9223372036854775808` to `-1`.
- `-0` is invalid.
- Philios has no arithmetic operators yet.

## `String`
- A `String` holds text.
- A `String` is immutable.

## `Function`
- Every declared function is a `Function` value.
- `Function` means "any function"; it does not encode signature details.

## `Object`
- `Object` accepts any non-`null` runtime value.
- In this version, no plain `Object` instance can be created directly.

## `null` and Nullable Types
- `null` is the only value of type `null`.
- A parameter or return type with `?` accepts `null` plus the base type.
- `?null` is invalid syntax.

## Reserved, Non-declarable Type Names
`ImmutableObject`, `NullObject`, `UnsignedInteger`, and `Float` are reserved words, but they are not valid declared types in this version.
