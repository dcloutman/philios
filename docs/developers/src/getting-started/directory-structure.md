# Source Directory Structure

Under `src/`, the project organizes implementation by interpreter component. The frontend handles source text and parsing. The analysis component checks parsed programs. The runtime evaluates them. The backend handles bytecode compilation. The CLI connects the interpreter to command-line use. The `common/` directory holds code shared across components. The `tests/` directory separates CLI tests from unit tests. It keeps fixtures and test support with the tests that use them.

```text
src/
  analysis/
    semantic_analyzer/
  backend/
    bytecode_compiler/
  cli/
    standard_output.hpp
    standard_output.cpp
  common/
  frontend/
    ast/
    lexer/
    parser/
  runtime/
    environment/
    evaluator/
      script_output.hpp
    object_model/
    virtual_machine/
  main.cpp

tests/
  cli/
    cli_cases.cmake
    run_case.cmake
  unit/
    sample-programs/
      hello-world/
    support/
      capturing_output.hpp
      capturing_output.cpp
```

## Responsibilities
- `src/cli/` contains CLI entrypoints and shipped I/O integration, including standard output writing and `SIGPIPE` handling.
- `src/runtime/evaluator/script_output.hpp` defines the output seam (`OutputWriter`) used by `echo`.
- `tests/cli/` contains CLI golden tests and the test driver.
- `tests/unit/support/capturing_output.*` provides the capturing output writer used by evaluator tests.
- `tests/unit/sample-programs/hello-world/` stores the read-only fixture copied by Task 6's `copy_folder` mode.
