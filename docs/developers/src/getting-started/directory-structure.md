# Source Directory Structure

The source tree groups code by responsibility rather than by a front-to-back compiler pipeline.
The directories describe where components belong; they do not prescribe an execution order.

```text
src/
  analysis/
    semantic_analyzer/
  backend/
    bytecode_compiler/
  common/
  frontend/
    ast/
    lexer/
    parser/
  runtime/
    environment/
    evaluator/
    virtual_machine/
```

## Responsibilities

- `frontend/` contains source-language structures and processing: the AST, lexer, and parser.
- `analysis/semantic_analyzer/` contains semantic validation and analysis of parsed programs.
- `runtime/` contains execution concerns, including environments, AST evaluation, and the virtual
  machine. Runtime values and module execution also belong here.
- `backend/bytecode_compiler/` contains code that compiles programs to bytecode for execution by
  the virtual machine.
- `common/` is for components genuinely shared across these areas. Keep subsystem-specific code
  with its owning area rather than using `common/` as a general utility bucket.

The evaluator and virtual machine are runtime components; the bytecode compiler is a separate
code-generation component. Do not add a `memory_manager/` for a tracing garbage collector:
Philios uses C++ ownership lifetimes for memory management.

Some directories may be empty while their components are unimplemented. Keep the structure aligned
with these responsibilities as source files are added.
