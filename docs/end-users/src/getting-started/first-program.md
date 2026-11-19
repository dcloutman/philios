# Getting Started: Your First Program

This page shows how to write a Philios program, run it, and read its result.

## The Project Directory
A Philios project is a directory that contains a file named `main.phil`. The interpreter always runs the `main.phil` file in the current working directory.

## Hello World
Create a directory, and save this text in it as `main.phil`:

```philios
function main (): Integer:
    echo("Hello World")
    return 0
```

Every program must declare a function named `main`. The interpreter calls `main` to start the program.

## Running the Program
Open a terminal in the project directory and run `philios` with no arguments:

The program writes exactly `Hello World` to standard output (no trailing line break), then exits with status `0`.

`philios` accepts no command-line arguments. Any argument causes a usage error.

## Next Steps
- Read the rules for [Source Files](../reference/source-files.md).
- Learn how to declare and call [Functions](../reference/functions.md).
- Learn about the [Types](../reference/types.md) that Philios supports.
