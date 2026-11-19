# Language Reference: Functions

A Philios program is currently a set of function declarations.

## Declaring a Function
A function declaration has this form:

```text
function name (parameter: Type, other: Type): ReturnType:
    statement
```

- Every function except `main` must declare a return type.
- Available declared types are `Integer`, `String`, `Function`, `Object`, `null`, `?Integer`, `?String`, `?Function`, and `?Object`.

## Statements
A function body must contain at least one statement.

| Statement | Example | Meaning |
|---|---|---|
| `return` | `return value` | Ends the function and returns a value. |
| `pass` | `pass` | Does nothing. Use it for an empty body. |
| Call | `helper(1)` | Calls a function and discards its result. |
| Pseudo-call | `echo("x")` | Calls a built-in pseudo-function. |

## Return Rules (D29)
- `null` functions: `return`, `return null`, and falling off the end all return `null`.
- `?T` functions (`?Integer`, `?String`, `?Function`, `?Object`):
  - `return` is allowed and behaves exactly as `return null`.
  - `return null` is allowed.
  - Falling off the end is a runtime error (missing return).
- `Integer`, `String`, `Function`, and `Object` functions:
  - must use `return <expr>`;
  - bare `return` is a declaration error;
  - falling off the end is a runtime error.
- A statement after `return` in the same block is a declaration error.

## The `main` Function
Every program must have exactly one `main` function.

- `main` takes no parameters.
- `main`'s return type is omitted or exactly `Integer`.
- A bare `return` in `main` is a declaration error.
- Falling off the end of `main` returns `0`.
- The `Integer` result maps to the process exit status modulo 256.

## Calling Functions
- Arguments are evaluated left to right.
- Argument count and types are checked at runtime.
- A call target that is not a `Function` is a runtime not-callable error.

## Pseudo-functions
Philios currently provides two pseudo-functions, `echo` and `typeof`. A pseudo-function is a built-in part of the language, not a `Function` value.

- A pseudo-function can only be called. `echo(...)` and `typeof(...)` are the only valid forms.
- It cannot be referenced, passed, returned, or used as a type. For example, `return echo`, `apply(typeof, 1)`, and `typeof(echo)` are syntax errors at the bare word.
- `echo` and `typeof` are reserved words, so no function or parameter can use either name.
- A pseudo-call takes exactly one argument. `echo()`, `echo("a", "b")`, `typeof()`, and `typeof(1, 2)` are syntax errors at the `echo` or `typeof` keyword.
- The argument is checked like any other expression before the program runs. An undefined name or any use of `main` is a declaration error, so `typeof(main)` is a declaration error at `main`.
- A pseudo-call may be a statement on its own line. Its result is then discarded, so `typeof(1)` alone on a line is valid.
- A pseudo-call does not count toward the call depth limit. Its argument list does count toward the nesting limit of 256, like a call's argument list. See [Errors and Limits](errors-and-limits.md).

### `echo(String str): null`
- The argument is evaluated exactly once.
- If the argument is an integer literal, such as `echo(42)` or `echo(-1)`, or the `null` literal, the program is rejected with a syntax error at the argument before it runs. A string literal is accepted.
- Any other argument, such as a name or a call, is checked when it is evaluated. If its value is not a `String`, evaluation raises a runtime argument-type error at the argument, and nothing is written.
- On success, `echo` writes the string's bytes exactly to standard output, with no line break added, flushes, and returns `null`.
- If writing or flushing fails, for example because of a broken pipe, a full device, or a closed standard output, evaluation raises a runtime output-write error at the `echo` keyword. Its message contains `standard output`.

### `typeof(expr): String`
- `typeof` takes exactly one argument of any type, including `null`. It accepts integer and `null` literals.
- It returns one of the script-level names: `Integer`, `String`, `Function`, or `null`.
- It reports the runtime type, not the declared type. For a parameter `x: Object` holding `1`, `typeof(x)` returns `"Integer"`.
- `typeof` has no runtime error of its own. At runtime it can raise only errors from evaluating its argument and the evaluation depth limit error.

This program writes `IntegerStringnull` and exits with status 0:

```philios
function main():
    echo(typeof(1))
    echo(typeof("a"))
    echo(typeof(null))
```

### Pseudo-function Errors

| Condition | Detected | Class | Location | Exit status |
|---|---|---|---|---|
| `echo` or `typeof` not followed by `(`, or used as a function or parameter name | before the program runs (parsing) | syntax | the word | 65 |
| A pseudo-call with no arguments or more than one | before the program runs (parsing) | syntax | the `echo` or `typeof` keyword | 65 |
| `echo` with an integer literal (positive or negative) or the `null` literal as its argument | before the program runs (parsing) | syntax | the argument's first character | 65 |
| Argument lists nested more than 256 deep, counting pseudo-call argument lists | before the program runs (parsing) | syntax | the `(` that opens level 257 | 65 |
| An undefined name, or any use of `main`, in the argument | before the program runs (validation) | declaration | the name | 65 |
| `echo`'s argument evaluates to a value that is not a `String` | while the program runs | runtime (argument type) | the argument's first character | 70 |
| Writing to or flushing standard output fails | while the program runs | runtime (output write; the message contains `standard output`) | the `echo` keyword | 70 |

A syntax or declaration error stops the program before it runs, so no `echo` call writes anything. After a runtime error, output already written by earlier `echo` calls stays on standard output.
