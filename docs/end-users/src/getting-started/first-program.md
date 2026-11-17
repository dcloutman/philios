# Getting Started: Your First Program

This page shows how to write a Philios program, run it, and read its result.

## The Project Directory
A Philios project is a directory that contains a file named `main.phil`. The interpreter always runs the `main.phil` file in the current working directory.

## A Minimal Program
Create a directory, and save this text in it as `main.phil`:

```philios
function main (): Integer:
    return 42
```

Every program must declare a function named `main`. The interpreter calls `main` to start the program.

## Running the Program
Open a terminal in the project directory and run `philios` with no arguments:

```bash
philios
echo $?
```

The second command prints `42`. That number is the program's exit status.

`philios` accepts no command-line arguments. Any argument causes a usage error.

## Program Results
Philios has no output functions yet. A program reports its result only through its exit status.

- The exit status is the `Integer` that `main` returns, modulo 256. For example, `300` becomes `44`.
- If `main` finishes without a `return` statement, the exit status is `0`.
- A successful program writes nothing to the terminal.
- If an error occurs, the interpreter writes one message to standard error and exits with an error status. See [Errors and Limits](../reference/errors-and-limits.md).

Exit statuses `64`, `65`, `66`, and `70` are also used for interpreter errors. If a program returns one of these values, check standard error to tell the two cases apart. A successful program never writes to standard error.

## A Program with Several Functions
A program may declare any number of functions. Functions may appear in any order in the file.

```philios
// main.phil
function main (): Integer:
    return apply(seven)

function apply (callback: Function): Integer:
    return callback()

function seven (): Integer:
    return 7
```

This program exits with status `7`. The `apply` function receives `seven` as a value and calls it. See [Functions](../reference/functions.md) for details.

## The `philios.json` File
Some examples include a `philios.json` file. The interpreter currently ignores this file. The entry point is always `main.phil`.

## Next Steps
- Read the rules for [Source Files](../reference/source-files.md).
- Learn how to declare and call [Functions](../reference/functions.md).
- Learn about the [Types](../reference/types.md) that Philios supports.
