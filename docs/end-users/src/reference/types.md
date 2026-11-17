# Language Reference: Types

Every value in Philios is an object. Philios has no primitive types. This page describes the types that Philios currently supports.

## Available Types
These names may be used as parameter types and return types.

| Type | Values | Written As |
|---|---|---|
| `Integer` | Signed 64-bit whole numbers | `0`, `42`, `9223372036854775807` |
| `String` | Text | `"hello"` |
| `Function` | References to declared functions | A function name, such as `helper` |
| `null` | The single value `null` | `null` |

### `Integer`
- An `Integer` holds a whole number from -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807.
- An `Integer` is immutable.
- Philios has no arithmetic or comparison operators yet. Integer literals cannot be negative.

### `String`
- A `String` holds text.
- A `String` is immutable.
- Philios has no string operations yet, such as concatenation. See [Source Files](source-files.md#string-literals) for the literal syntax.

### `Function`
- Every declared function is a `Function` value. See [Functions as Values](functions.md#functions-as-values).
- `Function` describes any function. It does not describe the function's parameters or return type.

### `null`
- `null` is the only value of the `null` type.
- Use `null` as a return type for a function that returns no useful value.
- A parameter of type `null` accepts only `null`.

## Exact Type Matching
Philios checks types when a function is called and when it returns.

- An argument must have exactly the type of its parameter.
- A returned value must have exactly the function's return type.
- Philios never converts a value from one type to another.
- `null` matches only the `null` type. You cannot pass `null` to an `Integer`, `String`, or `Function` parameter.

```philios
function main (): Integer:
    return size("text")

function size (value: Integer): Integer:
    return value
```

This program fails with a runtime error, because `"text"` is a `String`, not an `Integer`.

## Reserved Type Names
The names `Object`, `ImmutableObject`, `NullObject`, `UnsignedInteger`, and `Float` are reserved. They cannot be used as types yet.
