# Language Reference: Functions

A Philios program is currently a set of function declarations. This page describes how to declare, call, and pass functions.

## Program Structure
- A source file contains only function declarations.
- Statements may appear only inside a function body. A statement at the top level of the file is an error.
- A function cannot be declared inside another function.
- Functions may be declared in any order. A function may call a function that is declared later in the file.

## Declaring a Function
A function declaration has this form:

```text
function name (parameter: Type, other: Type): ReturnType:
    statement
```

- The declaration starts with the `function` keyword, followed by the function's name.
- Parameters appear in parentheses and are separated by commas. Each parameter has a name and a type. A function may have no parameters.
- A trailing comma after the last parameter is an error.
- The return type follows the parameter list and a colon. The final colon ends the header.
- The body is an indented block on the following lines.
- Every function except `main` must declare a return type.

The available types are `Integer`, `String`, `Function`, and `null`. See [Types](types.md).

### Naming Rules
- Each function name must be unique in the file.
- Each parameter name must be unique within its function.
- A parameter may not have the same name as any function in the file.
- A function name is a constant. Nothing can assign a new value to it.

## Statements
A function body must contain at least one statement. Philios currently supports three statements.

| Statement | Example | Meaning |
|---|---|---|
| `return` | `return value` | Ends the function and returns a value. |
| `pass` | `pass` | Does nothing. Use it for an empty body. |
| Call | `log(1)` | Calls a function and discards its result. |

```philios
function nothing (): null:
    pass
```

A body that contains only blank lines or comments is an error. Use `pass` instead.

A bare value, such as `42` or a name on its own line, is not a statement. Only a call can stand alone as a statement.

## Returning Values
- `return value` ends the function and returns `value`.
- A statement that follows `return` in the same block is an error. Blank lines and comments after `return` are allowed.
- A function with a return type other than `null` must return a value. If such a function reaches the end of its body without a `return`, a runtime error occurs.
- A bare `return`, with no value, is allowed only in a function that returns `null`.
- A function that returns `null` may use `return`, `return null`, or reach the end of its body. Each of these returns `null`.

```philios
function check (value: Integer): null:
    return
```

## The `main` Function
Every program must have exactly one function named `main`. The interpreter calls it to start the program.

- `main` takes no parameters.
- `main` returns an `Integer`. You may write the return type `Integer`, or you may omit it. Any other return type is an error.
- A bare `return` in `main` is an error. Use `return` with a value.
- If `main` reaches the end of its body, it returns `0`.
- The `Integer` that `main` returns becomes the program's exit status. See [Your First Program](../getting-started/first-program.md).
- No code may call `main` or refer to it by name. The name `main` may appear only in its own declaration.

```philios
function main ():
    pass
```

This program exits with status `0`.

## Calling a Function
Call a function by writing its name followed by arguments in parentheses.

```philios
function main (): Integer:
    return larger(3, 9)

function larger (first: Integer, second: Integer): Integer:
    return second
```

- The callee must be a name. Calling the result of a call, such as `f()()`, is an error.
- Arguments are separated by commas. A trailing comma after the last argument is an error.
- An argument may be a literal, a name, `null`, or another call.
- The number of arguments must equal the number of parameters.
- Each argument must match its parameter's type exactly. See [Types](types.md).
- The returned value must match the function's declared return type.

### Evaluation Order
Philios evaluates a call in this order:

1. It finds the function that the name refers to.
2. It evaluates each argument fully, from left to right. Calls inside an argument run before the next argument is evaluated.
3. It checks that the callee is a function.
4. It checks the number of arguments.
5. It checks each argument's type, from left to right.
6. It runs the function body.

### Name Lookup
A name inside a function refers to one of its parameters or to a function declared in the file. Any other name is an error. Philios reports undefined names before the program runs, even in functions that are never called.

## Recursion
A function may call itself, and functions may call each other. Each call adds one level of call depth. A program may have at most 1,000 active calls, and `main` counts as the first. See [Errors and Limits](errors-and-limits.md).

## Functions as Values
Every function is a value of the type `Function`. A function name without parentheses refers to the function. It does not call the function.

- A function can be passed as an argument to a parameter of type `Function`.
- A function can be returned from a function whose return type is `Function`.
- A parameter of type `Function` can be called by its name.

```philios
function main (): Integer:
    return twice(chooser())

function chooser (): Function:
    return four

function twice (action: Function): Integer:
    action()
    return action()

function four (): Integer:
    return 4
```

This program exits with status `4`.

The `Function` type does not describe parameters or a return type. Philios checks the number and types of arguments when the function is called.
