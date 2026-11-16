# Plan 0001 — Architecture Decisions

- **Plan ID:** `0001`
- **Plan:** `plans/0001.md` (revision 8, the developer patch for D49; decisions D1–D49)
- **Review:** `plans/0001_review.md` (overall verdict Go; latest Plan, Architecture, and Security
  rounds all Go)
- **Stage:** implementation-architect (stubs, preliminary unit tests, this record)
- **Date:** 2026-11-14

This record lists only decisions that the approved plan or its recorded developer decisions
already make. It adds no decision. §5 lists the interface shapes that the stubs had to spell out
where the plan names a component but not every signature. They are not architecture decisions, and
the generator may adjust them within the plan. §6 lists items this stage could not settle.

## 1. Approved decisions this stage relies on

### Source layout and dependencies (D41–D45, §7.1)
- Directories follow `docs/developers/src/getting-started/directory-structure.md`, plus
  `src/cli/` and `src/runtime/object_model/`. `src/main.cpp` is the only file directly in `src/`.
- Headers sit beside their sources. There is no `include/` directory and no public-header tier.
  The single include root is `src/`. Includes use full paths in double quotes, never `..`.
- `src/frontend/` includes nothing from `src/runtime/` or `src/analysis/`. `ast.hpp` refers to
  `ExecutionContext`, `Environment`, and `Completion` only through forward declarations. Every
  `evaluate` definition lives in `src/runtime/evaluator/evaluate.cpp`.
- `src/runtime/object_model/` includes nothing from `src/runtime/evaluator/`. The call sequence
  (`callFunction`) is in `src/runtime/evaluator/call.hpp/.cpp`. `Function` holds only its
  declaration and its `closureEnv`.
- Unit tests mirror `src/` under `tests/unit/`. Single-file fakes stay in the anonymous namespace
  of their test file. Shared helpers live in `tests/unit/support/` as `.hpp`/`.cpp` pairs.
  `tests/unit/` is a second include root, used only by `philios_tests` (D44).

### Object model and ownership (D1, D2, D3, D16, D18)
- Every value is a non-null `std::shared_ptr<Object>`. `Integer` (64-bit signed) and `String`
  derive from `ImmutableObject`, and their storage is private. `NullObject` and `Function` derive
  from `Object`. `ObjectType` plus the pure virtual `objectType()` is the only way to identify a
  Philios type. `matchesDeclaredType` is the single comparison against a declared type.
- Each `ExecutionContext` has one `NullObject`. Nothing is process-wide.
- Each declared function is exactly one `Function`. It holds a
  `std::shared_ptr<const FunctionDecl>` that it shares with the AST, plus a **strong**
  `closureEnv` that points to the module environment. This forms the cycle module env → Function →
  module env. No edge is weakened. `Module` teardown (its destructor and `release()`, which is safe
  to call twice) breaks the cycle by swapping the environment table with an empty local. It then
  releases the AST, and the source buffer last.
- `Module` owns, in declaration order, the source buffer, the AST, and the module environment.
  `Module` cannot be copied or moved. Only `Module::load` creates one, through a passkey
  constructor plus `std::make_unique`. The source is moved into the heap `Module` before any view
  into it is created. A module that fails to load is never returned.
- `ASTNode`, `Object`, and `Module` delete copy and move. Concrete classes are `final`.

### Evaluation (D5, D6, D7, D14, D24, D31b)
- `evaluate` returns a `Completion` (`Normal` or `Return`, with a non-null value). Control flow
  never uses exceptions.
- Call arguments are evaluated left to right, each into a named local. The callee kind, the
  argument count, and the argument types are checked afterward, in that order.
- The limits are `static constexpr` members of `ExecutionContext`: `kMaxParseNesting = 256`,
  `kMaxCallDepth = 1000`, and `kMaxEvalDepth = 10000`. The parser receives only the nesting value.
  RAII guards maintain the call depth and the evaluation depth. The evaluation-depth guard takes a
  `SourceLocation`, not a node. When both limits would trip, the call-depth error is reported.
- A failed runtime lookup is a `RuntimeError` (UndefinedIdentifier), not an internal error (D31b,
  a developer-accepted choice).

### Destruction (§3.9, CERT DCL57)
- Every AST, `Environment`, and `Module` destructor, every `detachChildren` member, and
  `Module::release()` are `noexcept`, allocate nothing, and never recurse once per node. The AST
  uses an intrusive worklist through a private `link_` member, with the unlink invariant. The
  environment chain is unlinked in a loop. The module table is released by a swap. No
  allocation-failure injection test is built.

### Loading and the CLI (D19–D21, D30, D32, D35, D47, D48)
- `main.phil` is opened with `O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC | O_NOCTTY`. The code
  then calls `fstat` on the open descriptor, reads in bounded chunks with `EINTR` retried
  (deliberately unbounded), and closes the descriptor. A non-copyable, non-movable RAII descriptor
  owns it, and the reader lambda captures that descriptor by reference. POSIX calls and headers
  appear only in `src/cli/source_file.cpp`.
- The read seam is `ChunkReadResult`, `ChunkReader` (a `std::function` over `std::span<char>`),
  and `readBoundedSource`. That function is the only place that enforces the 2,097,152-byte limit.
  `loadMainSource` takes no read operation.
- `runTopLevel(const std::function<int()>&)` catches, in order: the Philios error base,
  `StartupError`, `InternalError`, `std::bad_alloc`, `std::exception`, and `...`. `main` adds a
  last-resort `catch (...)` that returns 70. The exit statuses are 64, 65, 66, and 70.
  `philios.json` is never opened.
- A T-CLI-10 skip on Ubuntu 24.04 is acceptable (D48). In that case, code review checks
  `O_NOCTTY`.

### Build and test (D9, D10, D33, D36, D49)
- The toolchain is CMake 3.28, strict ISO C++20, GCC 13+, and `clang-tidy` 18+ with `cert-*` and
  `bugprone-*`.
- The test framework is Catch2 3.16.1 (D49, amending D10). It is a test-only library and a
  developer prerequisite. The build finds it with `find_package(Catch2 3.16.1 EXACT REQUIRED)` and
  links `Catch2::Catch2WithMain` to `philios_tests` only. Catch2 is never vendored or fetched (no
  `FetchContent`), and `philios` never links it. `Catch2::Catch2WithMain` supplies the test
  program's `main`, so there is no project test `main`. Test sources include Catch2 through its own
  headers, for example `<catch2/catch_test_macros.hpp>`. The imported target's include directories
  are `SYSTEM` and reach `philios_tests` only.
- `ctest` entries are registered explicitly with `add_test`, not with `catch_discover_tests`.
- `tests/unit/.clang-tidy` disables only `cert-err58-cpp`. This is the D36 test-only exception for
  Catch2's static `TEST_CASE` registration.
- The T-CLI-10 test case is named exactly `T-CLI-10 terminal device main.phil`. The CMake variable
  `PHILIOS_TCLI10_TEST_CASE` holds that name. The dedicated entry passes the name as a Catch2 test
  spec, and the main entry passes it prefixed with `~`. Catch2 fails a run that executes no test
  case, so no entry passes `--allow-running-no-tests`. T-BUILD-7's `WILL_FAIL` entry checks this
  behavior.
- The T-CLI-10 test file is the only place that may call `_exit`, a test-only ERR50 exception.

## 2. Trade-offs the plan records
| Decision | Trade-off |
|---|---|
| D24 | Evaluation-depth safety assumes the default 8 MiB stack. There is no `getrlimit` check. |
| D28(a) | Lone CR and mixed line endings are accepted. A display-spoofing risk is accepted. |
| D31(b) | A lookup that valid source cannot reach is a runtime error, not an internal error. |
| §3.9 literals | Literals create a new object on every evaluation. This keeps `frontend/ast/` independent of the runtime, and performance is a non-goal (D34). |
| §3.2 `EINTR` | The retry is unbounded, because the plan installs no signal handlers. |
| D3 | Cycles exist until `Module` teardown. The general cycle-reclamation question stays open (AGENTS.md). |

## 3. Pre-existing `src/common/errors.hpp/.cpp` (from the interrupted run)
I preserved both files unchanged. Assessment against plan task 2 and §4:
- **Header: complete for task 2.** It declares `SourceLocation` (1-based, byte columns) and
  `ErrorClass`. `RuntimeErrorKind` has one kind for each runtime row in §4: UndefinedIdentifier,
  NotCallable, ArgumentCount, ArgumentType, ReturnType, MissingReturn, CallDepthExceeded, and
  EvaluationDepthExceeded. It also declares the base `PhiliosError` with `LexicalError`,
  `SyntaxError`, `DeclarationError`, and `RuntimeError`, the separate `StartupError` and
  `InternalError`, all derived from `std::runtime_error` (CERT ERR60), and `escapeForDiagnostic`
  and the three `formatDiagnostic` overloads. The Doxygen blocks are complete and the header
  follows the style guide.
- **Source: partial.** The constructors and accessors are complete. The following are stubs:
  - `errorClassName` returns an empty view, because it is `noexcept` and cannot throw.
  - `escapeForDiagnostic` and the three `formatDiagnostic` overloads throw
    `InternalError("not yet implemented")`.
- **Fit:** The new stubs and tests use this interface unchanged. Both files compile warning-free
  under the plan's flags and are clean under `clang-tidy` with `cert-*` and `bugprone-*`.
- **Note:** The `escapeForDiagnostic` postcondition covers C0 controls and DEL. That matches the
  plan's minimum. See §6 for the open Security Review round 3 Low on escaping scope.

## 4. Files created by this stage
- `src/`:
  - `main.cpp`
  - `cli/` — `cli.hpp/.cpp`, `source_file.hpp/.cpp`, `exit_status.hpp`
  - `frontend/lexer/` — `utf8.hpp/.cpp`, `token.hpp`, `integer_literal.hpp/.cpp`,
    `lexer.hpp/.cpp`
  - `frontend/parser/parser.hpp/.cpp`
  - `frontend/ast/ast.hpp/.cpp`
  - `analysis/semantic_analyzer/validator.hpp/.cpp`
  - `runtime/environment/environment.hpp/.cpp`
  - `runtime/object_model/` — `object`, `integer`, `string_object`, `null_object`, `function`
    (each `.hpp/.cpp`)
  - `runtime/evaluator/` — `completion.hpp`, `execution_context.hpp/.cpp`,
    `type_check.hpp/.cpp`, `call.hpp/.cpp`, `module.hpp/.cpp`, `interpreter.hpp/.cpp`,
    `evaluate.cpp`
- Stub convention, following the precedent in `errors.cpp`:
  - Constructors, trivial accessors, and `kind()`/`objectType()` are complete.
  - Behavioral functions throw `InternalError("not yet implemented: …")`.
  - `noexcept` functions and destructors have placeholder bodies marked
    `TODO(plan 0001 task N)`.
  - `evaluate.cpp` is exactly the task 6 placeholder.
  - `main.cpp` is the complete §4 wiring.
- `tests/unit/` (an earlier run of this stage also created `test_main.cpp`, a doctest entry point;
  §4a records why it was deleted):
  - `support/` — `ast_builders`, `source_pipeline`, `stderr_capture` (each `.hpp/.cpp`)
  - `common/errors_test.cpp` (T-SEC-7)
  - `cli/` — `cli_test.cpp` (T-SEC-8), `source_file_test.cpp` (T-CLI-8, T-CLI-9),
    `terminal_device_test.cpp` (T-CLI-10)
  - `runtime/object_model/integer_test.cpp` (T-RUN-4)
  - `runtime/evaluator/execution_context_test.cpp` (T-NULL-4(a), T-SEC-3, T-SEC-5)
  - `frontend/lexer/integer_literal_test.cpp` (T-LEX-8 unit)
  - `frontend/lexer/lexer_test.cpp` (accepted cases of T-LEX-1 and T-LEX-3–8; T-LEX-2, -9,
    -10 lexer side, -11(a), -12)
  - `frontend/parser/parser_test.cpp` (accepted cases of T-PARSE-1 and T-LEX-10)
  - `frontend/ast/ast_test.cpp` (T-OWN-4, T-OWN-5 AST)
  - `analysis/semantic_analyzer/validator_test.cpp` (accepted cases of T-DECL-3, -4, -6, -7)
  - `runtime/environment/environment_test.cpp` (T-DECL-9, T-OWN-4 environment chain)
  - `runtime/evaluator/module_test.cpp` (T-OWN-1, T-OWN-2, T-OWN-5 module)
  - `runtime/evaluator/evaluate_test.cpp` (T-FN-1 unit, T-FN-4, T-FN-6, T-FN-8 unit,
    T-NULL-1, T-NULL-4(b), T-DECL-8, T-SEC-6)

## 4a. Conversion of the partial work from doctest to Catch2 (D49, plan §8)
The earlier runs of this stage wrote the tests for doctest. The files listed in §3 and §4 were kept
and converted, as plan §8 requires. None was discarded.
- I deleted `tests/unit/test_main.cpp`, which defined `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`.
  `Catch2::Catch2WithMain` now supplies the test program's `main`.
- In all 14 test files, I replaced `#include <doctest/doctest.h>` with
  `#include <catch2/catch_test_macros.hpp>`. The include stays in the same position, between the
  project headers and the standard library headers.
- I changed the 38 uses of `SUBCASE` to `SECTION`. These were in `lexer_test.cpp`,
  `evaluate_test.cpp`, `module_test.cpp`, and `source_file_test.cpp`.
- I changed the one `CHECK_MESSAGE` in `terminal_device_test.cpp` to `INFO(message)` followed by
  `CHECK(condition)`.
- `CHECK`, `CHECK_FALSE`, `REQUIRE`, `REQUIRE_FALSE`, `CHECK_NOTHROW`, `CHECK_THROWS_AS`, and
  `FAIL_CHECK` have the same meaning in both frameworks, so they did not change.
- I changed the two comments in `terminal_device_test.cpp` that named doctest, the `_exit`
  exception and the child's "never calls" note, so that they name Catch2. That file had no D36
  comment. The only one was in the deleted `test_main.cpp`.
- No test logic, test name, or `src/` file changed.
- No file outside `plans/` names doctest, includes a doctest header, or uses a `DOCTEST_` macro.
  The plan's history (§11, §12) and the grill record keep their doctest references, as §8 allows.

## 5. Interface shapes the stubs introduce (not architecture decisions)
The plan names these components but not every signature. The stubs chose the following shapes,
and the generator may revise any of them within the plan:
- `kExitUsage`, `kExitDataError`, `kExitNoInput`, `kExitSoftware`, and `kMaxSourceBytes` name
  plan-fixed values.
- `runCli(int argc)` is the body that `main` passes to `runTopLevel`.
- `Utf8Sequence`, `decodeUtf8`, `isDisallowedControl`, and `isBidiFormatting`.
- `TokenKind` includes an `EndOfInput` token. `Token` is an aggregate holding `text`,
  `integerValue`, and `stringValue`.
- `Lexer(std::string_view)` with `tokenize()` (one call only).
- `Parser(std::span<const Token>, std::size_t maxNesting)` with `parseProgram()`.
- `using Program = std::vector<std::shared_ptr<const FunctionDecl>>`.
- `validateProgram(const Program&)`.
- The AST node constructors and accessors. `CallExpr` takes a `std::unique_ptr<Identifier>`
  callee. `ReturnStmt::value()` returns a nullable borrowed pointer.
- The protected statics `ASTNode::pushOntoWorklist` and `ASTNode::destroyWorklist`.
- `Environment::define` and `releaseBindings`. `lookup` returns
  `std::optional<std::shared_ptr<Object>>`. The table is a private
  `std::map<…, std::less<>>`.
- `CallDepthGuard` and `EvaluationDepthGuard` as namespace-scope classes.
- `callFunction(ExecutionContext&, std::shared_ptr<Object> callee, std::vector<…> arguments,
  const CallExpr& callSite)`. `callSite` supplies the error locations.
- `runMain(ExecutionContext&, const Module&)`.
- `String::contentEquals`, which lets T-FN-1 check content without exposing storage.
- Tests include Catch2 as `<catch2/catch_test_macros.hpp>`, which is the header plan §7 gives as
  an example. Any file that needs more Catch2 facilities includes their own Catch2 headers.

## 6. Open items for the developer and the generator
1. **No blockers.** D49 resolved the earlier blocker about the missing doctest release. Catch2
   3.16.1 is installed under `/usr/local`, and `Catch2ConfigVersion.cmake` reports
   `PACKAGE_VERSION "3.16.1"`, so `find_package(Catch2 3.16.1 EXACT REQUIRED)` can find it. No
   agent downloaded or installed Catch2.
2. **Generator work this stage left untouched (plan task 1 and task 2 infrastructure):**
   - the AGENTS.md and documentation updates (§6, D46)
   - `CMakeLists.txt`. Task 2 replaces the current placeholder. That placeholder already calls
     `find_package(Catch2 3.16.1 EXACT REQUIRED)`, but the rest does not match the plan:
     - It requires CMake 3.16, not 3.28.
     - The project is named `my-lang`, not `philios`.
     - It lists `src/lexer.cpp`, `src/parser.cpp`, and `tests/test_lexer.cpp`, which do not exist.
     - It uses a `PUBLIC include` directory.
     - It calls `catch_discover_tests` instead of explicit `add_test` entries.
   - `.clang-tidy` and `tests/unit/.clang-tidy`
   - the CLI driver `tests/cli/run_case.cmake` and every CLI golden case
   - the build probes `tests/build/*` (T-BUILD-1 to T-BUILD-7)
3. **Non-blocking review findings not applied to the plan (recorded, not decided here):**
   - Plan Review round 5, Medium: which rule wins when a `\` in a string is followed by a
     forbidden or malformed character. No test here places such a character after a `\`.
   - Security Review round 3, Low: in T-CLI-10, the skip line is written before the parent's
     cleanup. `terminal_device_test.cpp` follows the plan's stated order.
   - Security Review round 3, Low: the escaping scope for C1 controls, bidi characters, and
     invalid bytes in diagnostics. T-SEC-7 here tests the plan's minimum.
4. **Expected preliminary failures:** every stub-backed test fails until the generator implements
   its task. Until task 6, the T-OWN-4 structures recurse during destruction, so an unimplemented
   build may crash on those tests instead of reporting a failure.
5. **The preliminary tests are not yet `clang-tidy` clean (generator, before T-BUILD-1).** These
   diagnostics were already in the test code before the conversion. The conversion did not
   introduce them. With `cert-*` and `bugprone-*` enabled and `cert-err58-cpp` disabled (the D36
   test configuration), `clang-tidy` 18.1.3 reports these diagnostics in `tests/unit/`:
   - **`bugprone-unchecked-optional-access`:** about 55 sites in `lexer_test.cpp`,
     `evaluate_test.cpp`, `environment_test.cpp`, `module_test.cpp`, `parser_test.cpp`, and
     `integer_literal_test.cpp`. The check does not treat a preceding `REQUIRE(opt)` as a guard.
   - **`bugprone-unused-local-non-trivial-variable`:** three `[[maybe_unused]]` locals that only
     exist to trigger a throw. They are in `source_file_test.cpp` (lines 94 and 113) and
     `terminal_device_test.cpp` (line 202).
   - **`bugprone-easily-swappable-parameters`:** `withLineBreaks` in `lexer_test.cpp`.

   Plan §7 requires a clean build, and any suppression needs a comment that cites the rule. The
   plan does not specify how to fix these sites, so this record does not choose a fix. The
   generator resolves them when it adds `.clang-tidy` and turns on `PHILIOS_CLANG_TIDY`. The
   `src/` files have no diagnostics.

## 7. Verification at this stage
- Every `src/**/*.cpp` passed `g++ 13.3 -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
  -Isrc -fsyntax-only` with no warnings. Each header also compiled standalone.
- All `src/` sources linked into `/dev/null`, which confirms that every referenced stub has a
  definition.
- `clang-tidy 18.1.3 -checks='-*,cert-*,bugprone-*'` reported no diagnostics on any `src/` file.
- After the D49 conversion (2026-11-14), these checks ran against the installed Catch2 3.16.1
  headers. No stand-in macros were used.
  - Every `src/**/*.cpp` file and all 17 test translation units (14 test files and 3 `support/`
    files) passed `g++ 13.3 -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Isrc
    -Itests/unit -isystem /usr/local/include -fsyntax-only` with no warnings.
  - Every `philios_core` source and every test source linked against `libCatch2Main.a` and
    `libCatch2.a` into a temporary executable outside the repository. The executable was deleted
    afterward.
  - `--list-tests` showed 70 registered test cases. Catch2 rejects duplicate test names, so this
    also confirms that no name is duplicated.
  - The test spec `T-CLI-10 terminal device main.phil` matched exactly one test case.
  - A spec that matches no test case exited with status 2. This matches the behavior that plan §7
    and T-BUILD-7 rely on.
  - No test was run. The stub-backed tests are expected to fail.
- `clang-tidy` again reported no diagnostics on `src/`. For `tests/unit/`, see §6 item 5.
- Re-verification run of this stage (2026-11-14, no file other than this record changed):
  - `tests/unit/test_main.cpp` is absent. No file outside `plans/` names doctest or uses a
    `DOCTEST_` macro, and no test uses `SUBCASE` or `CHECK_MESSAGE`. All 14 test files include
    `<catch2/catch_test_macros.hpp>`.
  - All 38 translation units (`src/` and `tests/unit/`) compiled with the plan's warning flags
    and produced no warnings. `philios_tests` linked against Catch2 3.16.1, and `philios` linked
    from `src/` alone. Both executables were built in a temporary directory outside the
    repository and deleted afterward.
  - `--list-tests` reported 70 test cases. The T-CLI-10 spec matched exactly 1, and a spec that
    matches nothing exited with status 2.
  - `clang-tidy` (`-*,cert-*,bugprone-*`) reported 0 diagnostics on `src/`.
  - No file under `src/` includes `fcntl.h`, `unistd.h`, `stat.h`, or `syscall.h`. No file under
    `src/frontend/` includes `runtime/` or `analysis/`. No file under `src/runtime/object_model/`
    includes `runtime/evaluator/`. No include uses `..`.

## 8. Re-run of this stage (2026-11-15)
The developer authorized implementation. This run confirmed the stage output and changed no file
except this record. It adds no decision.
- **Identity:** plan ID `0001` resolves to `plans/0001.md`, and the review is
  `plans/0001_review.md`. The plan has not changed since revision 8 (D49), so §1 to §6 still
  apply.
- **Stage output is intact:** every file in §4 exists. No header and no test file has changed
  since this record was written. The generator changed only `.cpp` bodies. No stub interface in
  §5 changed.
- **Changes made after this stage (not made by this stage):**
  - The developer committed `CMakeLists.txt` (commits `4f7a902` and `b0d8dfa`). This stage left
    it unchanged.
  - A generator pass added `.clang-tidy` and `tests/unit/.clang-tidy` and implemented some bodies
    (`plans/0001-implementation_notes.md`).
  - A security review then ran (`plans/0001-security_review.md`, NO-GO while the stubs remain).
- **§3 is out of date for `escapeForDiagnostic`:** it no longer throws `InternalError`. It now
  returns its input unchanged. This breaks the header's postcondition and the style guide's ban on
  returning a fake success value (SEC-0001-01). The generator owns this fix. This stage did not
  revert it.
- **Items for the generator (from the plan, not decided here):**
  - `src/frontend/parser/parser.cpp` emits 15 `-Wunused-result` warnings for `expect()` and
    `advance()`. Plan §7 requires a build with no warnings.
  - `CMakeLists.txt` registers one `add_test`. Plan §7 also requires these, which are still
    missing:
    - the dedicated and `~`-prefixed T-CLI-10 entries through `PHILIOS_TCLI10_TEST_CASE`
    - the CLI and T-BUILD entries
  - The items in §6 (items 2, 3, and 5) are still open.
- **Verification:**
  - Ran a scratch out-of-tree build in `/tmp` (deleted afterward) with GCC 13.3, CMake 3.28.3,
    Catch2 3.16.1, and `PHILIOS_SANITIZE=ON`.
  - All targets built. The only warnings were the parser warnings listed above.
  - `--list-tests` reported 70 test cases. The T-CLI-10 spec matched exactly 1, and a spec that
    matches no test case exited with status 2.
  - The run that excluded T-CLI-10 passed 21 test cases and failed 48. No sanitizer stopped the
    run. Every failure is expected while the stubs remain.
