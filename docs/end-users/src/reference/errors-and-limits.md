# Language Reference: Errors and Limits

This page describes how the interpreter reports errors and which limits it enforces.

## How Philios Checks a Program
The interpreter processes `main.phil` in these phases:

1. **Loading** reads the file.
2. **Lexing** splits text into tokens.
3. **Parsing** checks grammar.
4. **Validation** checks declarations in every function.
5. **Evaluation** calls `main`.

The program runs only if the first four phases succeed.

## Error Messages
Diagnostics are written to standard error. Program output from `echo(...)` is written to standard output.

Source-code errors use:

```text
main.phil:<line>:<column>: <class> error: <message>
```

Other errors use:

```text
philios: usage error: philios takes no arguments
philios: startup error: main.phil: <reason>
philios: internal error: <message>
```

## Error Classes and Exit Statuses

| Class | Phase | Exit Status |
|---|---|---|
| `usage` | Startup | 64 |
| `startup` | Loading | 66 |
| `lexical` | Lexing | 65 |
| `syntax` | Parsing | 65 |
| `declaration` | Validation | 65 |
| `runtime` | Evaluation | 70 |
| `internal` | Any | 70 |

## Plan 0002 conditions by class

### Lexical (`65`)
- Negative literal with a leading zero (`-0`, `-00`, `-007`, `-0abc`).
- Negative literal magnitude above `9223372036854775808`.
- Negative literal followed by an identifier character (`-1abc`).

### Syntax (`65`)
- A `-` that does not begin a valid negative literal (`- 1`, `--1`, `-x`, `a-1`, `f(1)-1`, `pass -1`).
- `?` outside a type.
- `?` separated from the next token (`? Integer`, `?/*c*/Integer`).
- `?null` or `?` before an invalid nullable base.
- `ImmutableObject`, `NullObject`, `UnsignedInteger`, or `Float` used as a type.
- `echo` or `typeof` used as a name, or not followed by `(`.
- Pseudo-call argument count not equal to one.
- `echo` with an integer literal (positive/negative) or `null` literal argument.
- Nesting above 256, including pseudo-call argument lists.

### Declaration (`65`)
- `main` annotated with any type other than `Integer` (including `?Integer`, `Object`, `?Object`).
- Bare `return` in an `Object`, `Integer`, `String`, or `Function` function.
  (`?T` functions allow bare `return` and treat it as `return null`.)

### Runtime (`70`)
- `echo` argument evaluates to a non-`String` value (argument-type runtime error).
- Writing or flushing standard output fails, including broken pipe, full device, or closed descriptor (output-write runtime error).
- Argument or return value not accepted by declared type.
- `Object` or `?T` function falls off the end without a `return`.

A lexical, syntax, or declaration error prevents evaluation, so stdout stays empty. After a runtime error, bytes already written by earlier `echo` calls remain on stdout.

## Limits
These limits are fixed.

| Limit | Value | Error |
|---|---|---|
| Source file size | 2,097,152 bytes (2 MiB) | `startup` |
| Nesting of blocks and call argument lists | 256 levels | `syntax` |
| Active function calls, including `main` | 1,000 | `runtime` |
| Nested evaluation steps | 10,000 | `runtime` |

Pseudo-call argument lists (`echo(...)`, `typeof(...)`) count toward the same 256 nesting limit.

## Running Time and Security
Philios does not limit runtime. Stop long-running programs with Ctrl-C.

Philios treats scripts like `python` or `php` CLI scripts and does not sandbox them.
