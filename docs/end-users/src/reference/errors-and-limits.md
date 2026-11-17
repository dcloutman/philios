# Language Reference: Errors and Limits

This page describes how the interpreter reports errors and which limits it enforces.

## How Philios Checks a Program
The interpreter processes `main.phil` in these phases:

1. **Loading** reads the file.
2. **Lexing** splits the text into tokens and checks characters, literals, comments, and indentation.
3. **Parsing** checks the grammar.
4. **Validation** checks the declarations in every function, including functions that are never called.
5. **Evaluation** calls `main` and runs the program.

The program runs only if the first four phases succeed. The interpreter stops at the first error.

## Error Messages
The interpreter writes error messages to standard error. It never writes to standard output.

An error in the source code has this format:

```text
main.phil:<line>:<column>: <class> error: <message>
```

For example:

```text
main.phil:2:14: runtime error: argument type does not match parameter type
```

The line and column point to the first byte of the code that caused the error. Columns count bytes. Control characters in echoed text are escaped.

Other errors use these formats:

```text
philios: usage error: philios takes no arguments
philios: startup error: main.phil: <reason>
philios: internal error: <message>
```

## Error Classes and Exit Statuses

| Class | Phase | Exit Status | Examples |
|---|---|---|---|
| `usage` | Startup | 64 | `philios` was given a command-line argument. |
| `startup` | Loading | 66 | `main.phil` is missing, unreadable, a symbolic link, not a regular file, or larger than 2 MiB. |
| `lexical` | Lexing | 65 | Invalid UTF-8, a disallowed character, bad indentation, an unterminated string or comment, an unknown escape, a misplaced `;`, or an invalid integer literal. |
| `syntax` | Parsing | 65 | An empty block, a statement at the top level, a nested function, a trailing comma, a reserved word used as a name, or nesting that is too deep. |
| `declaration` | Validation | 65 | A missing `main`, a duplicate name, a missing return type, an undefined name, a use of `main`, or a statement after `return`. |
| `runtime` | Evaluation | 70 | A wrong argument count, a type mismatch, a call to a non-function value, a missing `return`, or an exceeded depth limit. |
| `internal` | Any | 70 | An interpreter defect or an out-of-memory condition. |

A successful program exits with the status that `main` returns. See [Your First Program](../getting-started/first-program.md#program-results).

## Runtime Errors
These errors occur while the program runs:

- A function is called with the wrong number of arguments.
- An argument's type does not match its parameter's type.
- A returned value's type does not match the function's return type. This includes `main` returning a value that is not an `Integer`.
- A function with a return type other than `null` reaches the end of its body without a `return`.
- A value that is not a function is called.
- A depth limit is exceeded.

## Limits
These limits are fixed. They cannot be configured.

| Limit | Value | Error |
|---|---|---|
| Source file size | 2,097,152 bytes (2 MiB) | `startup` |
| Nesting of blocks and call argument lists | 256 levels | `syntax` |
| Active function calls, including `main` | 1,000 | `runtime`: call depth exceeded |
| Nested evaluation steps | 10,000 | `runtime`: evaluation nested too deeply |

A function body is one nesting level, and each argument list inside it adds another level. For example, `return f(g(1))` in a function body reaches nesting level 3.

The evaluation limit protects the interpreter when deep nesting and deep recursion combine. Typical programs reach the call limit first.

The interpreter relies on the default Linux stack size of 8 MiB. A smaller stack size, set with `ulimit -s`, may cause very deep programs to crash.

## Running Time
Philios does not limit how long a program runs. Press Ctrl-C to stop a program.

## Security
The interpreter treats a script like a `python` or `php` command-line script. It does not sandbox the script. Do not run unknown `.phil` files with elevated privileges.
