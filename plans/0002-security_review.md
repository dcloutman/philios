# Security Review: Plan 0002 — Standard Output and Pseudo-Functions

## Identity Verification
- **Confirmed Plan ID:** `0002`
- **Confirmed Plan Path:** `plans/0002.md`
- **Architecture Decisions:** `plans/0002-architecture_decisions.md`
- **Implementation Tasks:** `plans/0002-implementation-tasks.md`
- **Implementation Notes:** `plans/0002-implementation_notes.md`
- **Code Review Record:** `plans/0002-code_review.md`
- **Security Report Path:** `plans/0002-security_review.md`
- **Review Round:** `1`
- **Date:** 2026-11-19

---

## Executive Summary
An independent application systems and memory safety audit was performed on the completed C++20 implementation of Plan `0002` (`Standard Output and Pseudo-Functions`), covering all changes introduced across Tasks 1 through 7. The implementation was audited against the approved Plan 0002 specification, `AGENTS.md`, `docs/language_rules.md`, `.github/instructions/cpp-style-guide.instructions.md`, and the SEI CERT C++ Coding Standard (including applicable CERT C rules).

Before conducting the full audit, task progress in `plans/0002-implementation-tasks.md` was verified: Tasks 1 through 7 each have `Stub and tests: Done`, `Implementation: Done`, and `Review: GO`.

The audit evaluated all newly created and modified C++ source and header files, CMake test harness scripts, and supporting test components:
- **Lexer & Tokens (Task 2):** Addition of the 5 new reserved words (`not`, `goto`, `isa`, `echo`, `typeof`), `-` and `?` tokenization, contextual expression-start negative integer literal parsing, and overflow/leading-zero guards.
- **Type System & Subtyping (Task 3):** `TypeName::Object`, `DeclaredType` aggregate, `matchesDeclaredType`, `scriptTypeName`, `?` syntax and byte-adjacency validation, non-declarable type word rejections, `main` return annotation constraints, and D29 bare-return semantics.
- **Program Output Seam & System Safety (Task 4):** Abstract `OutputWriter` seam, `ExecutionContext` non-empty writer invariant, shipped `writeStandardOutput` implementation, and process-wide `SIGPIPE` ignoring (`ignoreBrokenPipeSignal`).
- **Pseudo-Functions & Object Model (Task 5):** `EchoCall` and `TypeofCall` AST nodes with iterative non-recursive destruction, parser expression/statement grammar integration, semantic analysis validation, evaluator execution with evaluation-depth guards and single-argument evaluation order, and the `String::bytes()` accessor.
- **CLI Test Driver & Harness (Tasks 1 & 6):** `tests/cli/run_case.cmake` stdout capture modes (`capture`, `dev_full`, `closed`, `broken_pipe`), exact hex comparison, and the `copy_folder` mode with canonical path resolution and symlink/alias overlap protection.

No buffer overflows, use-after-free, dangling references/views, integer overflows, uninitialized memory reads, race conditions, or unvalidated untrusted inputs were identified. Memory safety, resource limits, and reentrancy lifetime invariants are rigorously upheld.

The security gate decision for Plan 0002 is **GO**.

---

## Task Progress Verification
Every task in `plans/0002-implementation-tasks.md` was inspected prior to this audit:
- Task 1 (CLI driver support for stdout): `Stub and tests: Done` | `Implementation: Done` | `Review: GO`
- Task 2 (Lexer: reserved words, `-`, negative literals, `?`): `Stub and tests: Done` | `Implementation: Done` | `Review: GO`
- Task 3 (Types): `Stub and tests: Done` | `Implementation: Done` | `Review: GO`
- Task 4 (Output seam): `Stub and tests: Done` | `Implementation: Done` | `Review: GO`
- Task 5 (Pseudo-functions): `Stub and tests: Done` | `Implementation: Done` | `Review: GO`
- Task 6 (Documentation & copy_folder driver mode): `Stub and tests: Done` | `Implementation: Done` | `Review: GO`
- Task 7 (End-to-end verification): `Stub and tests: Done` | `Implementation: Done` | `Review: GO`

All prerequisite tasks have passed their task-level reviews.

---

## Detailed Security Analysis

### 1. Memory Safety & Lifetime Management
- **Iterative AST Destruction Protocol (CERT DCL57, MEM51):** `EchoCall` and `TypeofCall` AST nodes in `src/frontend/ast/ast.hpp` and `.cpp` adhere strictly to the non-recursive destruction pattern. Destructors and `detachChildren` are declared `noexcept`, allocate no memory, and route children through the intrusive `worklistHead` destruction loop. Unit test `T2-OWN-2` confirms that deallocating deep chains exceeding 200,000 nodes completes without native stack overflow or memory corruption.
- **View and Reference Lifetimes (CERT EXP61):** In `src/runtime/evaluator/evaluate.cpp`, `EchoCall::evaluate` evaluates its argument expression into a local `std::shared_ptr<Object> argumentValue` before invoking `String::bytes()`. This guarantees that the underlying `String` instance remains alive and immutable throughout `context.writeOutput(...)`. The `OutputWriter` contract explicitly defines `bytes` as a borrowed view valid only for the duration of the call, and `writeStandardOutput` consumes the view synchronously via `std::fwrite` without retaining pointers.
- **Capturing Fake Output Writer Safety:** In `tests/unit/support/capturing_output.cpp`, `CapturingOutput::writer()` captures `state_` by value (`[state = state_]`), preventing dangling reference hazards if the `CapturingOutput` handle is destroyed before the writer callback is executed.

### 2. Integer Safety & Numerical Boundary Handling
- **Negative Decimal Integer Conversion (CERT INT30-C, INT31-C, INT32-C):** `parseNegativeDecimalInteger` in `src/frontend/lexer/integer_literal.cpp` safely converts magnitude digits using `std::from_chars` into an unsigned `std::uint64_t magnitude`. It verifies full consumption, enforces `magnitude <= (1ULL << 63)` (`9'223'372'036'854'775'808`), and computes negation using well-defined unsigned modular arithmetic (`std::uint64_t{0} - magnitude`). In C++20, static casting this value to `std::int64_t` is well-defined two's-complement conversion, eliminating signed integer overflow and correctly handling `INT64_MIN` without undefined behavior.
- **Lexer Offset Bounds (CERT INT30-C):** In `src/frontend/lexer/lexer.cpp`, `Lexer::peek(std::size_t offset)` guards against unsigned integer overflow with `if (offset > (std::numeric_limits<std::size_t>::max)() - position_) return std::nullopt;` before indexing `source_`.

### 3. POSIX Confinement & Process Safety
- **Signal Handling and Broken Pipes (CERT MSC54, POS30-C):** `ignoreBrokenPipeSignal()` in `src/cli/standard_output.cpp` invokes `std::signal(SIGPIPE, SIG_IGN)` before loading `main.phil`. Setting signal disposition to ignore installs no custom handler and mutates no global execution state, safely converting broken pipe terminations into synchronous `EPIPE` return codes from `fwrite`/`fflush`. This allows `echo` to raise the clean `RuntimeErrorKind::OutputWrite` diagnostic without abnormal process termination.
- **Standard Output Handling:** `writeStandardOutput` correctly checks return counts from `std::fwrite`, handles `std::fflush(stdout)`, checks and clears `errno` and stream errors via `std::clearerr(stdout)`, and translates OS error codes into standard `std::error_code` structures without leaking unhandled exceptions.
- **POSIX Confinement Preservation:** `src/cli/standard_output.cpp` and `src/runtime/evaluator/script_output.hpp` rely strictly on ISO C/C++ standard headers (`<cstdio>`, `<csignal>`, `<system_error>`), keeping test `T-BUILD-6` fully satisfied without unconfined platform dependencies.

### 4. Input Validation & Defense-in-Depth
- **Lexical and Syntactic Defense:** Leading zeros on negative literals (`-0`, `-07`), trailing identifier characters (`-12a`), out-of-range magnitudes, non-declarable type keywords (`ImmutableObject`, `NullObject`, `UnsignedInteger`, `Float`), non-adjacent `?` tokens, `?null`, bare `echo`/`typeof` references, and literal arguments to `echo` are caught and rejected at compile time with exact error locations and classes before evaluation begins.
- **Resource Exhaustion Controls:** Parser nesting (`kMaxParseNesting = 256`), call recursion depth (`kMaxCallDepth = 1000`), and evaluation step depth (`kMaxEvalDepth = 10000`) are enforced via RAII guards (`EvaluationDepthGuard`, `CallDepthGuard`). Both `EchoCall` and `TypeofCall` evaluate their argument through the evaluation depth guard, preventing native stack exhaustion on recursive or deeply nested expressions.

### 5. CLI Test Driver & Path Traversal Guards
- **Symlink & Alias Protection in `run_case.cmake`:** The `copy_folder` mode introduced in Task 6 enforces rigorous path validation on `FIXTURE_DIR` and `WORK_DIR`. Canonical real paths are computed with `file(REAL_PATH)`, missing ancestors are checked for dangling symlinks or relative path components (`.`, `..`), and prefix checks in both directions (`cmake_path(IS_PREFIX ... NORMALIZE)`) ensure `WORK_DIR` cannot alias, contain, or reside within `FIXTURE_DIR`. Deletion and recreation of `WORK_DIR` occurs strictly after validation passes. Driver test `t2_doc_1_copy_folder_rejects_alias` verifies these protections under real symlink conditions.

---

## Findings by Severity

- **Critical:** None. **Actionable:** No.
- **High:** None. **Actionable:** No.
- **Medium:** None. **Actionable:** No.
- **Low:** None. **Actionable:** No.

No vulnerabilities or revision items were found.

---

## Security Gate Decision

### **Gate Decision: GO**

**Rationale:**
1. All 7 implementation tasks are completed, tested, and marked `GO`.
2. All modified and newly created C++20 code conforms to SEI CERT C++, the repository C++ style guide, and `AGENTS.md`.
3. Memory safety, object lifetimes, and bounds checks are verified under AddressSanitizer and UndefinedBehaviorSanitizer with zero defects.
4. Input validation and resource limiting boundaries (depth counters, integer ranges, syntax and declaration error checks) reliably defend against invalid or adversarial script input.
5. OS interactions (process `SIGPIPE` disposition, standard output writes, file system test fixtures) are confined and fail-safe.
