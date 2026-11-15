# Security Review: Plan 0001 — Minimal Philios Interpreter (MVP)

## Identity Verification
- **Plan ID:** `0001`
- **Plan Path:** `plans/0001.md`
- **Architecture Decisions:** `plans/0001-architecture_decisions.md`
- **Implementation Notes:** `plans/0001-implementation_notes.md`
- **Review Record:** `plans/0001_review.md`
- **Security Report Path:** `plans/0001-security_review.md`
- **Date:** 2026-11-14

---

## Executive Summary
An independent application security audit was performed on the current C++20 codebase for Plan `0001` (`Minimal Philios Interpreter (MVP)`). The review evaluated all modified and newly created files in `src/` and `tests/` against the approved plan specification, `AGENTS.md`, `docs/language_rules.md`, `.github/instructions/cpp-style-guide.instructions.md`, and the SEI CERT C++ Coding Standard.

The current implementation pass represents an intermediate development milestone. Foundational components (object model root, integers, strings, null object, execution context depth guards, environment definition/lookup, parser skeleton, and AST iterative destruction) exhibit strong memory safety design. However, critical security boundaries and runtime validation components remain stubbed out (including diagnostic string sanitization, safe source file loading with terminal/symlink/size controls, CLI top-level exception guards, lexer validation, semantic analysis validation, and evaluator runtime depth checks).

Because the core security controls are not yet active in executable code and 49 unit tests currently fail due to unfinished stubs, the current security gate decision is **NO-GO**.

---

## Security Audit Findings

### Finding SEC-0001-01: Incomplete Diagnostic Control Character & Terminal Escape Sanitization
- **File:** `src/common/errors.cpp`
- **Lines:** 45–47
- **Severity:** High
- **CWE:** CWE-150 (Improper Neutralization of Escape, Control, or Special Characters) / CERT ENV33-C
- **Description:**
  `escapeForDiagnostic(std::string_view text)` is currently implemented as a dummy passthrough:
  ```cpp
  std::string escapeForDiagnostic(std::string_view text) {
      return std::string(text);
  }
  ```
  The header contract (`src/common/errors.hpp`), Plan 0001 §4, and test `T-SEC-7` require that every control character (including CR, LF, NUL, and DEL) and every terminal escape sequence introducer (such as `\x1b`) be replaced by printable escape sequences such that the returned string contains no byte below `0x20` and no `0x7F`.
- **Security Impact:**
  Terminal injection and log spoofing. Malicious or malformed source files containing ANSI escape sequences (e.g. `\x1b[2J`, OSC sequences, terminal title setters) or carriage returns (`\r`) can manipulate terminal emulator state, clear operator screens, spoof diagnostic output, or exploit terminal emulator vulnerabilities when errors are printed to `stderr`.
- **Remediation:**
  Implement full byte-by-byte escape processing in `escapeForDiagnostic`. Any byte `< 0x20` or `== 0x7F` must be converted to printable representations (such as `\xHH`, `\r`, `\n`, `\t`, etc.), ensuring the postcondition `containsNoControlBytes` passes.

---

### Finding SEC-0001-02: Safe Source Loading Seam and OS Device Protections are Stubbed
- **File:** `src/cli/source_file.cpp`
- **Lines:** 16–24
- **Severity:** High
- **CWE:** CWE-403 (Improper Handling of Device / Special Files), CWE-59 (Improper Link Resolution), CWE-400 (Uncontrolled Resource Consumption)
- **Description:**
  `readBoundedSource` and `loadMainSource` currently throw `InternalError("not yet implemented")`.
  Plan 0001 §3.2, D21, D32, D47, and D48 establish strict POSIX file opening constraints:
  - `open("main.phil", O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC | O_NOCTTY)`
  - Verification via `fstat` that descriptor is strictly a regular file (`S_ISREG`), explicitly rejecting character devices, block devices, FIFOs, sockets, and directories.
  - Bounded reading with a non-configurable 2 MiB hard cap (`kMaxSourceBytes = 2'097'152`), discarding partial buffers on any failure.
- **Security Impact:**
  Until implemented, the binary has no defense against symlink traversal, denial of service via blocking on FIFO/pipes, controlling terminal takeover from terminal character devices (addressed in D47/T-CLI-10), or memory exhaustion from arbitrarily large source files.
- **Remediation:**
  Implement the non-copyable RAII descriptor wrapper and chunk reader loop in `src/cli/source_file.cpp` satisfying D32 and D47, ensuring all errors map to `StartupError` (exit status 66).

---

### Finding SEC-0001-03: Top-Level Exception Boundary and OOM Handling are Stubbed
- **File:** `src/cli/cli.cpp`
- **Lines:** 10–13
- **Severity:** High
- **CWE:** CWE-703 (Improper Check or Handling of Exceptional Conditions) / CERT ERR51-C, ERR54-C, ERR60-C, ERR61-C
- **Description:**
  `runTopLevel` currently throws `InternalError("not yet implemented: runTopLevel")`.
  In `src/main.cpp`:
  ```cpp
  int main(int argc, [[maybe_unused]] char* argv[]) {
      try {
          return philios::runTopLevel([argc] { return philios::runCli(argc); });
      } catch (...) {
          return philios::kExitSoftware;
      }
  }
  ```
  The plan specification (§4, D30) mandates that `runTopLevel` catch exceptions in a precise order by `const&`:
  1. `const PhiliosError&` (maps to exit 65 or 70 depending on error class)
  2. `const StartupError&` (maps to exit 66)
  3. `const InternalError&` (maps to exit 70)
  4. `const std::bad_alloc&` (maps to exit 70, prints fixed pre-allocated text without dynamic allocation)
  5. `const std::exception&` (maps to exit 70)
  6. `catch (...)` (maps to exit 70)
- **Security Impact:**
  Uncaught exceptions currently bypass diagnostic formatting and leak no useful diagnostic to stderr before hitting `main`'s fallback handler. Under out-of-memory conditions, attempting to allocate memory while reporting `std::bad_alloc` causes immediate program termination without clean resource cleanup.
- **Remediation:**
  Complete `runTopLevel` in `src/cli/cli.cpp` implementing the exact ordered catch chain with no-allocation OOM reporting.

---

### Finding SEC-0001-04: Potential Integer Overflow in UTF-8 Offset Calculations
- **File:** `src/frontend/lexer/utf8.cpp`
- **Lines:** 29, 38–39, 52–54
- **Severity:** Medium (Hardening Recommendation)
- **CWE:** CWE-190 (Integer Overflow or Wraparound) / CERT INT30-C
- **Description:**
  In `decodeUtf8`:
  ```cpp
  const auto at = [&text](std::size_t index) -> std::optional<unsigned char> {
      if (index >= text.size()) {
          return std::nullopt;
      }
      return static_cast<unsigned char>(text[index]);
  };
  ...
  const auto b1 = at(offset + 1U);
  ...
  const auto b2 = at(offset + 2U);
  ```
  While `offset >= text.size()` is checked at function entry, calculating `offset + 1U`, `offset + 2U`, and `offset + 3U` without checking remaining buffer length allows theoretical unsigned integer wrapping if `offset` is near `SIZE_MAX`.
- **Security Impact:**
  If `offset + n` wraps, `index >= text.size()` could evaluate to false on an unexpected wrapped index. In practice `kMaxSourceBytes` bounds file inputs, but library helper functions should be mathematically impervious to wrapping.
- **Remediation:**
  Compute `const std::size_t remaining = text.size() - offset;` after checking `offset >= text.size()`. Verify `remaining >= 2`, `remaining >= 3`, and `remaining >= 4` respectively before accessing subsequent bytes, avoiding addition with `offset`.

---

### Finding SEC-0001-05: Recursive Environment Lookup Across Scope Chains
- **File:** `src/runtime/environment/environment.cpp`
- **Lines:** 37–46
- **Severity:** Low / Medium (Hardening Recommendation)
- **CWE:** CWE-674 (Uncontrolled Recursion) / CERT DCL57
- **Description:**
  `Environment::lookup` uses recursive delegation:
  ```cpp
  std::optional<std::shared_ptr<Object>> Environment::lookup(std::string_view name) const {
      const auto found = bindings_.find(name);
      if (found != bindings_.end()) {
          return found->second;
      }
      if (enclosingScope_ != nullptr) {
          return enclosingScope_->lookup(name);
      }
      return std::nullopt;
  }
  ```
- **Security Impact:**
  In plan 0001, scope depth is strictly at most 2 (call environment -> module environment). However, CERT DCL57 and style guide lifetime rules caution against recursion on linked structures. If future features introduce deeper lexical environments or synthetic test chains, recursive lookup risks native stack exhaustion.
- **Remediation:**
  Rewrite `Environment::lookup` iteratively:
  ```cpp
  for (const Environment* env = this; env != nullptr; env = env->enclosingScope_.get()) {
      const auto found = env->bindings_.find(name);
      if (found != env->bindings_.end()) {
          return found->second;
      }
  }
  return std::nullopt;
  ```

---

### Finding SEC-0001-06: Incomplete Implementation Stubs Expose Unvalidated Attack Surface
- **Files:**
  - `src/frontend/lexer/lexer.cpp` (tokenize stub)
  - `src/analysis/semantic_analyzer/validator.cpp` (validateProgram stub)
  - `src/runtime/evaluator/module.cpp` (Module::load stub)
  - `src/runtime/evaluator/call.cpp` (callFunction stub)
  - `src/runtime/evaluator/evaluate.cpp` (AST evaluate stubs)
  - `src/runtime/evaluator/interpreter.cpp` (runMain stub)
- **Severity:** High
- **CWE:** CWE-654 (Missing Required Security Checks)
- **Description:**
  All key execution pipelines are currently stubs throwing `InternalError`. As documented in `plans/0001-implementation_notes.md`, this results in 49 failing unit tests out of 70. Crucial semantic invariants specified in Plan 0001 §3.5 (e.g., verifying exactly one `main`, rejecting duplicate parameter names, checking return type consistency) and evaluator runtime checks (call-depth limit 1000 and evaluation-depth limit 10000) are not yet integrated into the live execution path.
- **Security Impact:**
  The interpreter cannot safely process any script input. The implementation is not yet in an auditable state for production security sign-off.
- **Remediation:**
  Complete the planned implementation tasks (Tasks 4 through 8) to connect the lexer, parser, validator, module loader, and evaluator.

---

## Positive Security & Memory Safety Observations
During the audit, the following mechanisms were verified to comply with C++20 memory safety and CERT standards:
1. **Iterative AST Destruction Protocol:** `src/frontend/ast/ast.cpp` implements `pushOntoWorklist`, `detachChildren`, and `destroyWorklist` without native recursion. Destructors are marked `noexcept` and allocate nothing. Tests in `tests/unit/frontend/ast/ast_test.cpp` confirm safe iterative teardown of AST structures with 200,000 nodes without stack overflow under AddressSanitizer.
2. **Iterative Environment Scope Unlinking:** `Environment::~Environment()` iteratively unlinks single-owner enclosing scopes in a loop, successfully verified with 200,000 chained environments.
3. **RAII Depth Limits:** `CallDepthGuard` and `EvaluationDepthGuard` in `src/runtime/evaluator/execution_context.cpp` properly protect against call recursion and evaluation nesting, resetting counters cleanly on exception unwinding.
4. **Ownership & Lifetime Controls:** The code adheres strictly to Rule of Zero/Rule of Five. Raw pointers never own memory. Copy and move operations are deleted for polymorphic bases (`ASTNode`, `Object`) preventing object slicing (CERT OOP51).
5. **Integer Safety:** `Integer::exitStatus` computes modulo 256 using checked non-negative mapping without overflow hazard.
6. **Exception Class Design:** All language errors inherit from `std::runtime_error`, ensuring exception copy constructors are noexcept / cannot throw (CERT ERR60-C).

---

## Security Gate Decision

### **Gate Decision: NO-GO**

**Rationale:**
1. Core security protections (diagnostic escape sanitization `SEC-0001-01`, safe bounded source loading with `O_NOCTTY`/`O_NOFOLLOW` `SEC-0001-02`, and top-level exception handling `SEC-0001-03`) are currently stubbed.
2. Key pipeline components (lexer, semantic validator, module registration, evaluator) are unfinished stubs, causing 49 unit tests to fail.
3. Once the remaining implementation tasks from Plan 0001 are completed and all security tests pass under AddressSanitizer and UndefinedBehaviorSanitizer, a subsequent review round should be performed.
