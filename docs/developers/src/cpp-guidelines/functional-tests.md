# Command-Line Tests

Command-line tests run the compiled `philios` program in separate working directories. Each test checks the program's exit status, standard error, and standard output. The test driver prepares each case from a fixture or generates its input in the build directory.

Add test cases to [`tests/cli/cli_cases.cmake`](../../../../tests/cli/cli_cases.cmake). Store program fixtures in `tests/cli/cases/`.

## Checking Exit Status and Errors

Use `philios_cli_test` when a test checks the exit status and diagnostic output. Set `FIXTURE` to a file in `tests/cli/cases/`, then specify the expected status and diagnostic kind. For a located error, also specify its class and source location.

```cmake
philios_cli_test(t_lex_1_stray_continuation_in_code
    FIXTURE lex_1_stray_continuation_in_code.phil
    STATUS 65
    KIND located
    CLASS lexical
    LINE 2
    COLUMN 10)
```

`KIND empty` requires no diagnostic on standard error. `KIND located` checks the error class and location. `KIND startup` checks a startup diagnostic. `KIND usage` checks the command-line usage message. The test driver also requires standard output to be empty unless the test uses `philios_cli_stdout_test`.

## Checking Standard Output

Use `philios_cli_stdout_test` to check exact output bytes. Set `STDOUT_HEX` to the expected bytes written as lowercase hexadecimal. For example, this test checks that the program writes `Hello World` with no trailing newline.

```cmake
philios_cli_stdout_test(t2_echo_1_hello_world
    FIXTURE t2_echo_1_hello_world.phil
    STATUS 1
    KIND empty
    STDOUT_HEX 48656c6c6f20576f726c64)
```

Set `STDOUT_MODE` to `dev_full`, `closed`, or `broken_pipe` when testing how the program handles a failed standard-output write. These modes do not compare output bytes.

## Running a Test

After configuring the build directory, build the program and run the selected test with:

```bash
cmake --build build
ctest --test-dir build -R '^t2_echo_1_hello_world$' --output-on-failure
```
