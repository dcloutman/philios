# Developer Agent System Instructions: Language Implementation

Currently Writing Version: 0.0.1

We are writing a hyper-fast, lightweight interpreter, in C++20, for a new programming language called Philios. Philios blends the expressive capabilities of PHP, Python, Java, and JavaScript/TypeScript while maintaining low memory consumption. The interpreter compiles to a minimalist, standalone native binary that runs from the command line.

## Agent Pemissions
- Unless otherwise specified, agents may make changes to this directory and subdirectory, as specified by the developer.
- Agents may not execute mutating `git` commands, such as `git commit` or `git push`, `git fetch`, or `git pull`.
- Agents may run non-mutating commands against the local codebase such as `git diff` and `git log`.
- Agents are not granted access to the remote repository. Agents do not need access to the remote repository to plan or implement.
- Agents may not use `wget`, `curl` or any other network access tool. Agents may ask users to run such commands, but may not require a user to run a network command that result in the immediate execution of downloaded code.
- Agents may not claim authorship via an attribution line in commit messages or in the resulting code.

## Document Precedence
* This file defines architecture and language-level implementation decisions. The C++ style guide, `.github/instructions/cpp-style-guide.instructions.md`, governs how C++ code is written. Follow both.
* If they conflict, this file wins for architecture and language behavior, and the style guide wins for C++ coding practice. Report any conflict you find instead of silently choosing one.
* Language semantics come from `docs/language_rules.md` and approved plans in `plans/`. Neither the style guide nor an implementation may invent language behavior.
* Priorities, in order: security (memory safety, no undefined behavior, no crashes on any input), conformance to industry-standard C++ practice, then performance.

## Core Architectural Constraints
Strictly adhere to Philios requirements. Do not introduce abstractions or workarounds that violate these paradigms:

1. Global Variables are Forbidden in Philios: All state must be locally scoped or explicitly bound to an isolated execution space. Do not generate a global registry or fallback namespace. The only exception is the single signal interrupt flag the C++ style guide permits (required by CERT MSC54); it carries no script-visible state.
2. Philios Uses an Isolated Module System: Implement file-level isolation (similar to Node.js CommonJS/ESM or Python modules). Each parsed file has its own isolated `Module` structure and private top-level `Environment`. Sharing code across files is strictly limited to explicit `exports` and `imports`.
3. Philios has Implicit Memory Management (Auto-GC): Use modern C++ memory ownership semantics (`std::shared_ptr`, `std::unique_ptr`). Rely on reference counting lifecycles to automatically clean up AST nodes, classes, dynamic instances, and scopes. Do NOT write a custom tracing or mark-and-sweep garbage collector.
4. Minimal Binary Footprint: Rely strictly on the C++ standard library and header-only parser combinators. Avoid heavy dependencies like ANTLR, Flex, Bison, or large runtime systems.

## Data Type Blueprint
* Everything is an object: Philios has no native primitives. Every type descends from an ancestral `Object` type.
* Built-in immutable types: `String`, `Integer`, `UnsignedInteger`, and `Float` are `ImmutableObject` classes. Their internal C++ storage is a private implementation detail. Only the owning class may depend on it, so it can be replaced without breaking Philios code.
* Integer overflow wraps: `Integer` and `UnsignedInteger` arithmetic wraps modulo 2^N (two's complement for `Integer`) at the type's fixed bit width. The bit widths are not yet specified.
* Other numeric semantics are not yet specified: bit widths, division and remainder by zero, `min / -1`, shift behavior, mixed signed/unsigned/float operations, and conversions. Do not choose them during implementation. Implement them only after they are specified in `docs/language_rules.md` or an approved plan.
* Callables: Anonymous functions and classes are deferred until v0.1.0. Named standalone functions and closures capture their environment chain via `closureEnv`.
* Dynamic Objects: Instance fields are stored in a map paired with an owning pointer to the class blueprint (`CustomClass`). Field iteration order is an open decision (see below).

## Implementation Workflows
We use AI-powered development. Developers write an initial plan for a feature in `/plans` and run the `/planning-loop` skill in chat. The planning-loop orchestrator delegates grilling to `plan-grill-me` and relays its questions to the developer one at a time. After each answer, it invokes the grill agent again with the exact answer and prior handoff state; the grill agent writes or appends the decision record only after the developer confirms its final summary. The orchestrator then coordinates plan composition and reviews. The developer reviews and revises the approved plan, if necessary, before explicitly starting the `/implementation-loop`. That loop implements the code. The developer then reviews the code, compiles it, and manually debugs it as necessary.

## Development Environment
We assume that developers are working in a POSIX environment and that `bash` is available to run commands. macOS and Linux users should use the Bash environment native to their development system. Bare-metal Windows development is not supported. Windows users must use Bash inside the development environment's WSL POSIX environment. Do not use PowerShell, `cmd.exe`, Cygwin, or Git for Windows Bash.

Agents must use a tool that executes Bash directly inside the POSIX development environment. Agents must not invoke `wsl.exe` or use a Windows shell or execution tool to launch Bash. If no direct Bash tool is available, agents must stop before running commands and report the missing capability. For WSL development, the developer must reopen the workspace in Visual Studio Code's WSL remote environment with a direct Bash tool available. Agents must not create tasks or delegate commands to bypass this requirement.

## Code Generation Quality Guardrails
* Write explicit, type-safe, idiomatic C++20. Do not use generic macros. Raw pointers must never own memory; non-owning use is limited to what the style guide permits.
* Wrap evaluation errors in clear runtime exception boundaries.
* Prioritize clean execution paths over overly complex template meta-programming abstractions.
* Follow the C++ style guide at `.github/instructions/cpp-style-guide.instructions.md`.

## Open Decisions
These are project-wide contracts. Do not resolve them inside an implementation. If a task depends on one, stop and ask the developer, or follow the approved plan that resolves it. Interim defaults apply until a decision is recorded.

| Decision | Needed by | Interim default |
|---|---|---|
| Remaining numeric semantics (bit widths and the other items in Data Type Blueprint; overflow wrapping is decided) | Before implementing numeric operations | None; do not implement |
| Reclaiming reference cycles (closures that escape, self- or mutually-referencing instances) | v0.1.0 | Leaked cycles are acceptable only until process exit; never weaken an owning edge to avoid one |
| Evaluator control flow for `return`/`break`/`continue` (result type vs. dedicated exceptions) | Before implementing statements | None; ask |
| Evaluation order of operands, call arguments, and assignment targets | Before implementing expressions | None; ask. The C++ evaluator must sequence evaluation explicitly regardless of the choice (style guide, CERT EXP50) |
| Field and map iteration order visible to scripts | Before exposing iteration | Do not expose iteration order |
| Source encoding and column units | Before the lexer | UTF-8 input; columns in bytes |
| Import path resolution and import cycles | Before modules | Reject import cycles with a language error |
| Concurrency model | Before any threading | Single-threaded; no mutable state shared across threads |
| Trust model, script capabilities, and resource limits | Before untrusted execution is supported | Scripts are trusted like `python`/`php` CLI scripts, but no input may crash the interpreter or cause undefined behavior |
| Minimum compiler and standard library versions | Before the first build system | Use only C++20 features supported by current GCC, Clang, and MSVC releases |
| Build system and test framework (the standard-library-only rule covers the shipped binary; may test-only dependencies exist?) | Before the first test | None; ask |
| Recursion depth limit values (parser nesting, call depth) | Before the parser | Limits are named constants held by the execution context; the developer sets the values |

## Verification
* Builds must be warning-free under `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` (GCC/Clang) or `/W4` (MSVC).
* Tests must pass under AddressSanitizer and UndefinedBehaviorSanitizer where the toolchain supports them.
* Code must pass `clang-tidy` with the `cert-*` and `bugprone-*` checks enabled, once the build system is set up. Suppressions need a comment citing the specific rule and the reason.
* Every change must add regression tests for the boundary cases it touches, including malformed and deeply nested input.
* Fuzzing of the lexer and parser is planned. Set it up through a plan, not as a side effect of another task.