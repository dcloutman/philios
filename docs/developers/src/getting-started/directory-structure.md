# Source Directory Structure

The source tree groups code by responsibility rather than by a front-to-back compiler pipeline.
The directories describe where components belong; they do not prescribe an execution order.

```text
src/
  analysis/
    semantic_analyzer/
  backend/
    bytecode_compiler/
  cli/
  common/
  frontend/
    ast/
    lexer/
    parser/
  runtime/
    environment/
    evaluator/
    object_model/
    virtual_machine/
  main.cpp
```

## Responsibilities

- `main.cpp` is the only source file directly under `src/`. It holds the program entry point.
- `cli/` contains the command-line driver: argument checking, loading the project's source file,
  running the program, and mapping results to exit statuses.
- `frontend/` contains source-language structures and processing: the AST, lexer, and parser.
- `analysis/semantic_analyzer/` contains semantic validation and analysis of parsed programs.
- `runtime/` contains execution concerns, including environments, AST evaluation, and the virtual
  machine. Runtime values and module execution also belong here.
- `runtime/object_model/` contains the runtime value types: `Object`, `ImmutableObject`, and the
  built-in types that derive from them, such as `Integer`, `String`, and `Function`. It contains no
  evaluation logic and does not depend on `runtime/evaluator/`.
- `backend/bytecode_compiler/` contains code that compiles programs to bytecode for execution by
  the virtual machine.
- `common/` is for components genuinely shared across these areas. Keep subsystem-specific code
  with its owning area rather than using `common/` as a general utility bucket.

The evaluator and virtual machine are runtime components; the bytecode compiler is a separate
code-generation component. Do not add a `memory_manager/` for a tracing garbage collector:
Philios uses C++ ownership lifetimes for memory management.

Some directories may be empty while their components are unimplemented. Keep the structure aligned
with these responsibilities as source files are added.

## Headers and tests

- Each `.hpp` file sits next to its `.cpp` file in its `src/` subdirectory. There is no `include/`
  directory and no public-header tier: every project header is internal.
- Unit tests under `tests/unit/` mirror the `src/` subdirectories (for example
  `tests/unit/frontend/lexer/`). A fake used by only one test file stays in that file's anonymous
  namespace, with no header. Shared test helpers go in `tests/unit/support/` as `.hpp`/`.cpp`
  pairs.
- `tests/unit/` is a second include root for the unit-test target only. Production targets never
  get a `tests/` include path or source, and nothing test-only lives under `src/`.
- CLI golden tests live in `tests/cli/` and build probes in `tests/build/`.
