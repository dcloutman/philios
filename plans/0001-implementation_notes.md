# Plan 0001 — Implementation Notes

## Identity
- Plan ID: `0001`
- Plan path: `plans/0001.md`

## Progress in this implementation pass
- Updated build scaffolding to target `philios_core`, `philios`, and `philios_tests` with C++20 and Catch2 3.16.1 wiring.
- Added root `.clang-tidy` and `tests/unit/.clang-tidy` stubs.
- Implemented several foundational runtime/object-model pieces:
  - `Integer::exitStatus` modulo mapping.
  - `String::contentEquals`.
  - `ExecutionContext` depth guards for call depth and evaluation depth.
  - `Environment` basic define/lookup/release behavior and iterative enclosing-scope unlinking.
  - `matchesDeclaredType` exact type matching.
- Implemented parser skeleton logic for the specified grammar (`src/frontend/parser/parser.cpp`).
- Implemented AST iterative worklist teardown paths in `src/frontend/ast/ast.cpp`.
- Implemented UTF-8 decode/classification and integer-literal parsing helpers (`utf8.cpp`, `integer_literal.cpp`).

## Current blockers / incomplete scope
The approved plan is not fully implemented yet in this pass. Major planned components are still stubbed and currently drive test failures:
- CLI pipeline and top-level handling (`src/cli/cli.cpp`)
- source loading/read seam (`src/cli/source_file.cpp`)
- lexer (`src/frontend/lexer/lexer.cpp`)
- semantic validator (`src/analysis/semantic_analyzer/validator.cpp`)
- module load/registration (`src/runtime/evaluator/module.cpp`)
- evaluator and call/interpreter pipeline (`evaluate.cpp`, `call.cpp`, `interpreter.cpp`)

## Verification snapshot
- Configure/build command run:
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
  - `cmake --build build -j4`
- Test command run:
  - `ctest --test-dir build --output-on-failure`
- Result: build completed; tests failed (21 passed, 49 failed), with failures aligned to the remaining stubs above.


## Progress in this implementation pass (bounded generator pass 1)
- Implemented `src/common/errors.cpp` diagnostic escaping and formatter behavior.
- Implemented `src/cli/source_file.cpp` bounded source loading with POSIX open/fstat/read/close flow, non-copyable descriptor RAII, `O_NOCTTY`, and the `readBoundedSource` seam semantics (size cap, EINTR retry, invariant checks).
- Implemented `src/cli/cli.cpp` top-level boundary handling (`runTopLevel` catch chain), CLI argument usage path, and `runProject` loading handoff to `loadMainSource` (interpreter pipeline still intentionally stubbed).
- Added CTest registrations for:
  - `philios_tests` excluding T-CLI-10,
  - dedicated T-CLI-10 entry (`SKIP_REGULAR_EXPRESSION`, `TIMEOUT 10`),
  - T-BUILD-6 POSIX confinement probe,
  - T-BUILD-7 zero-match filter guard (`WILL_FAIL TRUE`).
- Added `tests/build/check_posix_confinement.cmake` to enforce D32 header confinement with regex self-check coverage for whitespace/delimiter variants.
- Extended `tests/unit/cli/cli_test.cpp` with direct checks for class/status mapping and usage behavior.

## Verification for this pass
- Configure/build:
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
  - `cmake --build build --clean-first`
- Focused Catch2 checks for this pass:
  - `./build/philios_tests "T-SEC-7*,T-SEC-8*,T-CLI-8*,T-CLI-9*"` → passed.
- Full ctest run:
  - `ctest --test-dir build --output-on-failure`
  - Result: dedicated T-CLI-10, T-BUILD-6, and T-BUILD-7 entries passed; the main `philios_tests` entry still fails due unrelated stubbed lexer/parser/validator/evaluator/module areas.
- `ctest --test-dir build -N` confirms the expected four CTest registrations and arguments.

## Residual notes
- Existing parser `-Wunused-result` warnings (known in `src/frontend/parser/parser.cpp`) remain unchanged in this pass.
- No new language or architecture decision was required.


## Progress in this implementation pass (bounded generator pass 2)
- Implemented `src/frontend/lexer/lexer.cpp` end-to-end for plan §3.3 rules exercised in task 5/unit coverage:
  - single bounds-checked byte access via `peek`, UTF-8 validation paths, control/bidi rejection,
  - reserved-word classification for all 23 keywords,
  - line-break normalization (`LF`, `CRLF`, lone `CR`),
  - indentation/`INDENT`/`DEDENT` handling with continuation-line indentation suppression,
  - `//` and `/* ... */` comment behavior including multi-line closing-line restrictions,
  - `;` continuation semantics and continuation error cases,
  - integer literal parsing/limits/leading-zero checks,
  - string-literal escapes and `${` reservation,
  - end-of-input finalization (`NEWLINE`, pending `DEDENT`s, `EndOfInput`),
  - one-shot `tokenize()` contract and checked-overflow-safe `peek()` indexing.
- Implemented `src/analysis/semantic_analyzer/validator.cpp` for plan §3.5 declaration validation:
  - `main` presence/shape rules,
  - function and parameter uniqueness checks,
  - parameter-name conflict checks against module functions,
  - non-`main` return-annotation requirement,
  - bare-return legality checks (`main` and non-`null` cases rejected),
  - statement-after-return rejection,
  - whole-file identifier validation for value/callee usage (including the `main` identifier restriction),
  - explicit malformed-AST internal-error guards.
- Addressed the known parser `-Wunused-result` warning set in `src/frontend/parser/parser.cpp` by consuming `[[nodiscard]]` results explicitly.
- Fixed parser optional return-type lookahead (`:` only starts a return annotation when followed by a type token), restoring parser acceptance behavior required by existing task-6 unit tests.

## Verification for this pass
- Build:
  - `cmake --build build -j4` → passed.
- Focused tests:
  - `./build/philios_tests "T-LEX-*"` → passed.
  - `./build/philios_tests "T-DECL-3*,T-DECL-4*,T-DECL-6*,T-DECL-7*"` → passed.
  - `./build/philios_tests "T-PARSE-1*,T-LEX-10 Integer2*"` → passed.
- Full ctest:
  - `ctest --test-dir build --output-on-failure`
  - Result: `t_cli_10_terminal_device_main_phil`, `t_build_6_posix_confinement`, and
    `t_build_7_tcli10_filter_guard` passed; `philios_tests` still fails only in evaluator/module
    areas that remain intentionally stubbed for later plan tasks (`Module::load`, AST evaluate paths).

## Remaining blockers after this pass
- Out-of-scope stubs still block full-suite green status:
  - `src/runtime/evaluator/module.cpp` (`Module::load`/release wiring)
  - evaluator path bodies in `src/runtime/evaluator/evaluate.cpp` and related runtime call/interpreter integration.


## Progress in this implementation pass (bounded generator pass 3)
- Implemented `src/runtime/evaluator/module.cpp` load pipeline and teardown:
  - `Module::load` now allocates the module via passkey + `std::make_unique`, moves source into the module-owned buffer, lexes/parses/validates, builds the module environment, and registers one `Function` object per declaration.
  - `Module::release()` now deterministically breaks the module-env/function cycle via `releaseBindings()`, then clears AST ownership and source bytes; repeated calls remain safe.
- Implemented runtime evaluation bodies in `src/runtime/evaluator/evaluate.cpp`:
  - Added `EvaluationDepthGuard` entry to all concrete `evaluate` methods.
  - Implemented identifier lookup with runtime undefined-identifier error path.
  - Implemented literal evaluation (`Integer`, `String`, `null`), call expression sequencing, expression-statement discard behavior, return semantics, and block early-exit on `Return` completion.
  - Implemented §3.7/D24 limit precedence for `CallExpr`: if both limits would trip, report call-depth at the callee identifier.
- Implemented call sequence in `src/runtime/evaluator/call.cpp`:
  - Added call-depth guarding, callee kind check, arity check, left-to-right argument type checking, fresh call environment creation with closure chaining, parameter binding, body execution, missing-return handling, `main` fallthrough-to-zero handling, and return type checks via `matchesDeclaredType`.
- Implemented interpreter entry in `src/runtime/evaluator/interpreter.cpp`:
  - `runMain` now resolves module `main`, invokes it through `callFunction`, verifies Integer result, and maps to process status via `Integer::exitStatus()`.
- Integrated runtime pipeline in `src/cli/cli.cpp`:
  - `runProject` now runs `loadMainSource` → `Module::load` → `runMain` with a fresh `ExecutionContext`.

## Verification for this pass
- Debug build/test:
  - `cmake --build build -j4` → passed
  - `ctest --test-dir build --output-on-failure` → passed (4/4 tests)
- Sanitizer build/test:
  - `cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DPHILIOS_SANITIZE=ON` → passed
  - `cmake --build build-asan -j4` → passed
  - `ctest --test-dir build-asan --output-on-failure` → passed (4/4 tests)
- Release build/test:
  - `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release` → passed
  - `cmake --build build-release -j4` → passed
  - `ctest --test-dir build-release --output-on-failure` → passed (4/4 tests)

## Residual status after this pass
- No remaining failing tests in the current suite.
- No new unresolved plan-scope blockers were introduced.


## Review-round-1 revision — Pass A (clang-tidy / build gate) — 2026-11-15
Scope: only the "mandatory clang-tidy gate" finding in `plans/0001-code_review.md` (round 1).
Lexical/diagnostic contracts, CLI golden cases, T-BUILD-1..5 probes, and documentation are deferred
to later passes.

Triage (partial edits inspected first, kept as-is where correct):
- `PHILIOS_CLANG_TIDY` default `ON` (plan §7): already applied in `CMakeLists.txt`, with the
  missing-tool `FATAL_ERROR` message naming the tool and how to disable it. Verified by a fresh
  configure without `-D` (cache shows `PHILIOS_CLANG_TIDY:BOOL=ON`); `CXX_CLANG_TIDY` appears in
  the generated `build.make` for `philios_core`, `philios`, and `philios_tests`.
- `validator.cpp` unchecked optional (`bugprone-unchecked-optional-access`): already fixed
  (`declaredReturnType` checks `has_value()` before dereferencing). Clean.
- Test diagnostics (unchecked optional, unused non-trivial locals, swappable parameters): already
  fixed via `tests/unit/support/require_value.{hpp,cpp}` (`[[noreturn]]` failure path visible to
  the dataflow analysis) and test edits. No `NOLINT` was added anywhere.
- `tests/unit/.clang-tidy` comment: present; cites `cert-err58-cpp`, the Catch2 registration
  reason, D36/D49, and the `src/` scope verified by T-BUILD-5. It remains the sole suppression.
- **New defect found and fixed:** root `.clang-tidy` had `HeaderFilterRegex: '^(src|tests/unit)/'`.
  clang-tidy matches against the resolved header path, which is absolute here, so no project
  header was ever analyzed (probe: a scratch tree with `src/nested/x.hpp` containing a
  `cert-err58-cpp` and a `bugprone-branch-clone` violation produced no diagnostics under the old
  regex and both errors under the new one). Changed to
  `'(^|/)(src|tests/unit)/.+\.(h|hpp)$'` with an explanatory comment, satisfying plan §7's
  "project headers under `src/`, including every nested subdirectory, and under `tests/unit/`".
  Catch2 (installed under `/usr/local`) is not matched. With headers now analyzed, the build is
  still clean.

Commands and results:
- `timeout 600 cmake -S . -B build-tidy -DCMAKE_BUILD_TYPE=Debug` (fresh dir, default options) → ok.
- `timeout 1800 cmake --build build-tidy -j8 -- -k` → exit 0, zero clang-tidy/compiler diagnostics.
- `timeout 900 ctest --test-dir build-tidy --output-on-failure --timeout 300 </dev/null` → 4/4 passed.
- `timeout 1800 cmake --build build-asan -j8` → exit 0, warning-free;
  `timeout 900 ctest --test-dir build-asan --output-on-failure --timeout 300 </dev/null` → 4/4 passed.

Pass A outcome: **done** for the in-scope build-gate items. Still open from the same finding (deferred
by pass bounds): register T-BUILD-1, T-BUILD-2, T-BUILD-5 (`tests/build/check_tidy_scope.cmake`,
`tests/build/dynamic_static_init.cpp`). Other round-1 findings (acceptance suite, lexical/diagnostic
defects, documentation) untouched.


## Review-round-1 revision — Pass B (shipped-language contract defects) — 2026-11-15
Scope: only the High finding "Shipped language behavior violates the approved lexical and diagnostic
contracts" in `plans/0001-code_review.md` (round 1). Identity confirmed: Plan ID `0001`, plan path
`plans/0001.md`; all referenced artifacts use the exact `0001` stem.

Triage of earlier partial edits (inspected, verified on the real binary, kept):
1. `;`-only line (§3.3, §4): `src/frontend/lexer/lexer.cpp` already rejects a `;` unless code
   precedes it on the same physical line (`lineHasCode` check), at the `;`. Covers body lines,
   module-level lines, a `;` after only a comment, and a `;`-only continuation line.
2. Indentation location (§4 "start of the line"): `applyIndentation` and the tab check already
   report at `lineStartLocation` (physical line, column 1).
3. Return-type location (§4 "the `return`"): `Completion` already carries the executed `return`'s
   location and `callFunction` reports `ReturnType` there.

New changes in this pass:
- `src/runtime/evaluator/interpreter.cpp`: `runMain` still had a second, unreachable `ReturnType`
  report at `main`'s declaration. It is now an `InternalError` (invariant: `callFunction` has
  already enforced `Integer` at the `return`), so no path reports a return-type error at the
  declaration.
- `CMakeLists.txt`: added `philios_cli_case(...)` helper that registers a CTest entry running the
  real `philios` binary through the existing `tests/cli/run_case.cmake` driver (status, stderr
  class/line/column prefix, single diagnostic line, empty stdout), and registered 18 regression
  cases: T-LEX-6 (`;`-only body/module/after-comment/continuation lines, mid-line `;`, accepted
  continuation exit 2), T-LEX-7 (2/3/6/8-space bodies, inconsistent dedent, all at column 1),
  T-LEX-3 (tab, and spaces+tab, in indentation at column 1), T-RUN-5 (`main` returning String,
  `null`, a Function, and a `return ;` continuation; helper mismatch after a comment at 5:13).
- `tests/cli/cases/*.phil`: 18 committed fixtures for the cases above.
- `tests/unit/runtime/evaluator/evaluate_test.cpp`: unit regression asserting kind `ReturnType`,
  class runtime, and location 3:13 (the `return`), not the declaration 1:10.
- Harness sanity: running `run_case.cmake` with the old defective expectations (1:10 for the return
  mismatch; exit 0 for a `;`-only line) fails, so the cases detect the reviewed defects.

Commands and results:
- `timeout 1800 cmake --build build -j8` (Debug, `PHILIOS_CLANG_TIDY=ON`) → exit 0, no compiler or
  clang-tidy diagnostics; `timeout 900 ctest --test-dir build --output-on-failure --timeout 300
  </dev/null` → 22/22 passed.
- `timeout 1800 cmake --build build-asan -j8` → exit 0; ctest (same flags) → 22/22 passed.
- `timeout 1800 cmake --build build-release -j8` → exit 0; ctest (same flags) → 22/22 passed.
- `git diff --check` → only a pre-existing trailing blank line in `plans/0001-security_review.md`
  (outside Pass B scope, untouched).

Pass B outcome: **done** for the three defects. Still open (outside Pass B bounds): T-BUILD-1/2/5
probes, the rest of the §9 CLI golden acceptance suite (the new helper is reusable for it), and the
task-1 documentation finding.


## Review-round-1 revision — Pass C (§9 CLI golden acceptance suite) — 2026-11-15
Scope: only the High finding "The required acceptance suite is not implemented", CLI/golden part.
T-BUILD-1..5 and the task-1 documentation are out of scope for this pass. Identity confirmed: Plan
ID `0001`, plan path `plans/0001.md`; all referenced artifacts use the exact `0001` stem.

Changes:
- `CMakeLists.txt`: new keyword helper `philios_cli_test(...)` (MODE, FIXTURE, ARGS, STATUS, KIND,
  CLASS/LINE/COLUMN, MESSAGE, FORBID, SIZE, EXIT_VALUE, NESTING, DEPTH, SKIP_LABEL, NO_CRASH;
  `SKIP_REGULAR_EXPRESSION` for T-CLI-4's root skip). The Pass B positional `philios_cli_case` now
  forwards to it; all 18 Pass B registrations are kept unchanged. Includes the new manifest.
- `tests/cli/cli_cases.cmake` (new): 235 registrations covering every CLI-run part of §9:
  T-CLI-1..7; T-LEX-1..11 error cases (class/line/column) and the task-8 CLI exit checks of their
  accepted cases; T-PARSE-1..5; T-DECL-1..7; T-RUN-1/2/3 (T-RUN-5 is Pass B; T-RUN-6 is every
  `KIND empty` case); T-FN-1/2/3/5/7/8; T-NULL-2/3; T-SEC-1/2/4. T-OWN-3 is these runs in
  `build-asan` (normal, lexical, syntax, declaration, runtime, call-depth, evaluation-depth).
  Locations were derived from §3.3–§4 rules, not from the binary. Unit-only items (T-CLI-8..10,
  T-LEX-12, T-RUN-4, T-FN-4/6, T-NULL-1/4, T-SEC-3/5..8, T-OWN-1/2/4/5, T-DECL-8/9) were not touched.
- `tests/cli/cases/*.phil`: 211 new committed fixtures (some deliberately contain CR, CR LF, NUL,
  control, bidi, or malformed UTF-8 bytes). Large/structural inputs stay driver-generated (§7).
- `.gitattributes` (new): `tests/cli/cases/*.phil -text`, so line endings in fixtures are byte-exact.
- `tests/cli/run_case.cmake`: `KIND located` with an empty `EXPECT_COLUMN` checks class and line
  only (used by exactly one case, see open question 1); clearer error when CLASS/LINE are missing.
- **Defect found and fixed** (`src/frontend/parser/parser.cpp`): `fo/*c*/o()` was reported at
  2:5 (`fo`, "invalid statement"); §3.3 D40 and T-LEX-11 require 2:12 (the second `o`). A statement
  that starts with an identifier can only be `exprStmt := callExpr NEWLINE` (§3.4), so it is now
  parsed as a call and the token after the callee is the offending token. Covered by
  `t_lex_11_comment_splits_identifier`.
- Harness sanity: wrong column/line/status expectations (e.g. 2:16 for the T-LEX-2 byte column,
  4:470 for T-SEC-4, status 0 for a FIFO) make the driver fail.

Commands and results:
- `timeout 600 cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` (clang-tidy ON) → ok;
  `timeout 1800 cmake --build build -j8` → exit 0, no compiler or clang-tidy diagnostics;
  `timeout 900 ctest --test-dir build --output-on-failure --timeout 300 -j8 </dev/null` → 257/257.
- `timeout 1800 cmake --build build-release -j8` → exit 0;
  `timeout 900 ctest --test-dir build-release --output-on-failure --timeout 300 -j8 </dev/null` → 257/257.
- `timeout 1800 cmake --build build-asan -j8` → exit 0;
  `timeout 900 ctest --test-dir build-asan --output-on-failure --timeout 300 </dev/null` (serial) →
  256/257; **`t_sec_4_evaluation_depth` fails** (see blocker). T-CLI-10 passed in this run;
  T-CLI-4 passed (not root). With `-j8`, `t_parse_5_long_line` also exceeded the driver's 10 s
  `execute_process` timeout under load; serially the ASan binary needs about 3.6 s for that input.
- `git diff --check` → only the pre-existing blank line at EOF of `plans/0001-security_review.md`.

**Blocker — T-SEC-4 fails under ASan at `kMaxEvalDepth = 10,000` (plan §5, §8 task 8, D24: stop and
ask; the limit was not changed).** With `ulimit -s` = 8192 the `build-asan` binary (Debug, `-g`,
no optimization, `PHILIOS_SANITIZE=ON`) dies with `AddressSanitizer: stack-overflow` (status 1 via
the driver). The same input exits 70 with `main.phil:4:468: runtime error: evaluation nested too
deeply (limit 10000)`, the plan-derived location, in `build` and `build-release`; the ASan binary
also passes with `ulimit -s` 12288 or more. Question for the developer: how should T-SEC-4 be made to
pass under ASan on the default 8 MiB stack: lower `kMaxEvalDepth`, change the `build-asan`
configuration (for example its optimization level), or require a reduction of the evaluator's
per-level stack use (and if so, is that within plan 0001)? The case stays registered and failing.

**Open question 1 — `12abc` column.** §4 locates "Unexpected character …; malformed number" at "the
character", and T-LEX-8 gives no location. It is not stated whether that is the literal's first byte
(the `1`, column 12; current behavior) or the identifier character that makes it malformed (the
`a`, column 14). `t_lex_8_digits_then_identifier` checks status 65, class `lexical`, and line 2 only
until the developer decides.

Interpretation recorded (plan-backed, not a new decision): an unexpected indent (T-PARSE-2) is
checked at the first token of the over-indented line (3:9), consistent with §3.3's D40 example that
reports the dedent case at its `return` token.

Pass C outcome: CLI golden suite implemented and registered; one ASan acceptance failure (T-SEC-4)
and one column question await the developer. Still open (outside Pass C bounds): T-BUILD-1..5 and
the task-1 documentation finding.


## Review-round-1 revision — Pass D (T-SEC-4 native stack under ASan/UBSan) — 2026-11-15
Scope (developer decision on the Pass C blocker): make T-SEC-4 pass in `build-asan` on the default
8 MiB stack by using less native stack per evaluator/parser recursion level. Not changed:
`kMaxEvalDepth` (10,000), every other limit, sanitizer/build flags, the threading model, the plan.
No architecture added. Identity confirmed: Plan ID `0001`, plan path `plans/0001.md`; all artifacts
use the exact `0001` stem. Toolchain: GCC 13.3.0 (`/usr/bin/c++`).

Measurement method (reproducible; scripts lived in `/tmp`, not committed):
1. *Dynamic threshold.* The T-SEC-4 input, generated byte-identically to `run_case.cmake` MODE
   `eval_depth` (890 bytes, sha256
   `e19470f653c74473b7e77b53afe42b16975f7229fd451f549de36240120656a5`; `cmp` against the
   driver's own `string(REPEAT …)` code run with `cmake -P` → identical), was run as
   `bash -c "ulimit -s N; timeout 60 <bin> </dev/null"`, binary-searching N (KiB, 256–65,536, step
   ≤16) for the smallest stack that gives exit 70 plus `evaluation nested too deeply`. Baseline:
   `function main(): return 0` runs in 64 KiB for every build, so the threshold is almost all
   per-level cost.
2. *Static frame sizes.* Each hot file was compiled to a scratch directory with the build's exact
   flags plus `-fstack-usage` (ASan: `-g -fsanitize=address,undefined -fno-sanitize-recover=all
   -fno-omit-frame-pointer`; Debug `-g`; Release `-O3 -DNDEBUG`; all `-std=c++20`). Project flags
   were not changed.
3. *Recursion shape.* In T-SEC-4, each `f` recursion is `Block` → `ReturnStmt` → 255 nested
   `CallExpr` levels (254 `g(` plus `f(`), so ≈ 99 % of the 10,000 levels are `CallExpr::evaluate`
   calling itself directly through argument evaluation. The parser recursion is bounded by
   `kMaxParseNesting` (256 levels, ≈ 240 KiB peak under ASan) and finishes before evaluation, so it
   does not contribute to the T-SEC-4 peak. It was measured and left unchanged.

Results (frame bytes from `-fstack-usage`; threshold = minimal `ulimit -s`, KiB):

| Build | `CallExpr::evaluate` before → after | `Block` / `ReturnStmt` / `ExprStmt` after | Threshold before → after |
|---|---|---|---|
| ASan+UBSan (`build-asan`) | 1,120 → 592 | 496 / 384 / 320 (were 720 / 544 / 368) | **10,997 → 5,881** |
| Debug (`build`) | 352 → 224 | 176 / 144 / 128 | 3,475 → 2,232 |
| Release (`build-release`) | 352 → 304 | 144 / 112 / 112 | 3,459 → 2,997 |

The two methods agree: 10,000 × (1,120 + return address/frame pointer) ≈ 10.9 MiB ≈ 10,997 KiB
measured. ASan now has about 2.3 MiB (28 %) of headroom under 8,192 KiB.

Cause: GCC gives each addressable local and temporary its own stack slot, and ASan adds redzones and
does not reuse slots across scopes. In `CallExpr::evaluate`, every
`throw InternalError("…")`/`RuntimeError` site built a `std::string` temporary, and the callee
`Completion` plus the by-value `callFunction` operands (a copied `shared_ptr` and a moved `vector`)
were part of the same frame. All of these were in every nested level, even though none is live
across the recursive argument evaluation.

Change (`src/runtime/evaluator/evaluate.cpp` only, file-local, anonymous namespace):
- `[[noreturn]] throwInternal(std::string_view)` and `[[noreturn]] throwCallDepthExceeded(const
  CallExpr&)` build the exceptions out of line. They are used by `CallExpr`, `Block`, `ReturnStmt`,
  and `ExprStmt::evaluate`. The messages, error kinds, and locations are identical; the D24
  precedence rule still reports at the callee identifier.
- `evaluateCallee(...)` (callee `Completion` and its checks) and `completeCall(...)` (takes rvalue
  references so the by-value `callFunction` arguments are built in its frame) take the
  non-recursive work out of `CallExpr::evaluate`. The §3.7/D6 order is unchanged: callee, then
  arguments left to right, then the call.
- No GNU attributes were used (MSVC `/W4` portability). The ASan build is `-O0`, so the helpers are
  not inlined there.

Commands and results (`ulimit -s` = 8192):
- `timeout 1800 cmake --build build-asan -j8` → exit 0, no warnings.
- `bash -c 'ulimit -s 8192; cd <tmp>; timeout 60 build-asan/philios </dev/null'` → status 70,
  `main.phil:4:468: runtime error: evaluation nested too deeply (limit 10000)`, the expected
  10,000-depth diagnostic.
- `timeout 900 ctest --test-dir build-asan --output-on-failure --timeout 300 </dev/null` (serial)
  → **257/257 passed**, including `t_sec_4_evaluation_depth`, `t_sec_1_*`, `t_sec_2_*`, and the
  T-SEC-5/T-SEC-6 unit tests.
- `timeout 1800 cmake --build build -j8` (Debug, `PHILIOS_CLANG_TIDY=ON`, tidy runs on
  `evaluate.cpp` via `__run_co_compile --tidy`) → exit 0, no compiler or clang-tidy diagnostics;
  `timeout 900 ctest --test-dir build --output-on-failure --timeout 300 -j8 </dev/null` → 257/257.
- `timeout 1800 cmake --build build-release -j8` → exit 0, no warnings; ctest (same flags) → 257/257.
- `timeout 1800 cmake --build build-tidy -j8` (Debug, clang-tidy ON) → exit 0, no diagnostics;
  ctest (same flags) → 257/257.
- `git diff --check` → only the pre-existing blank line at EOF of `plans/0001-security_review.md`.

Pass D outcome: **done.** The Pass C T-SEC-4 blocker is resolved without changing any limit, flag,
or plan item. Still open (outside Pass D bounds, unchanged): T-BUILD-1..5, the task-1
documentation finding, and Pass C open question 1 (`12abc` column).

## Review-round-1 revision — Pass E (`12abc` column) — 2026-11-15
Developer answer `first_digit` resolves Pass C open question 1: a digit sequence immediately
followed by an identifier character is reported at the literal's first byte, the first digit
(column 12 in `    return 12abc`), matching the leading-zero rule and the existing lexer.
- `plans/0001.md`: recorded only in the Integer-literals lexer rule (§3.3; the brief cited "§6",
  which is "AGENTS.md impact" and holds no lexer rule) and in §9 T-LEX-8. No other plan text
  changed (§4's table row and §12 revision history untouched).
- `tests/cli/cli_cases.cmake`: `t_lex_8_digits_then_identifier` now also requires `COLUMN 12`.
- Lexer code unchanged (`src/frontend/lexer/lexer.cpp` already throws at `tokenLocation`).

Commands and results:
- `timeout 1800 cmake --build build -j8` (Debug, `PHILIOS_CLANG_TIDY=ON`) → exit 0, CMake
  reconfigured, no warnings or clang-tidy diagnostics (no C++ source was recompiled).
- `timeout 300 ctest --test-dir build -R t_lex_8_digits_then_identifier -V --timeout 60
  </dev/null` → passed with `-DEXPECT_COLUMN=12`.
- `timeout 900 ctest --test-dir build --output-on-failure --timeout 300 -j8 </dev/null` → 257/257.
- `git diff --check -- plans/0001.md tests/cli/cli_cases.cmake` → clean.

Pass E outcome: **done.** Still open (outside Pass E bounds, unchanged): T-BUILD-1..5 and the
task-1 documentation finding.

## Code-review round 2 revision — Pass F (T-BUILD-1..5) — 2026-11-15
Scope: only the round-2 High finding on T-BUILD-1..5 (plan §7, §8 task 2, §9). Task-1
documentation, language behavior, limits, and the plan are unchanged. Identity confirmed: Plan ID
`0001`, plan path `plans/0001.md`; all artifacts use the exact `0001` stem.

Changes:
- `CMakeLists.txt`: `add_library(philios_core STATIC …)` (§7 "a static library"; was implicit via
  `BUILD_SHARED_LIBS`). Writes `philios_build_facts.cmake` at generate time (`TYPE`,
  `CXX_CLANG_TIDY`, `SOURCES`, `INCLUDE_DIRECTORIES`, `LINK_LIBRARIES`,
  `INTERFACE_LINK_LIBRARIES` of the three targets). Registers `t_build_1_clang_tidy_all_targets`
  and `t_build_5_clang_tidy_scope` only when `PHILIOS_CLANG_TIDY` is on (§7), and
  `t_build_2_missing_clang_tidy`, `t_build_3_strict_cxx20`, `t_build_4_test_separation` always.
- New `tests/build/`: `dynamic_static_init.cpp` (T-BUILD-5 fixture, in no target),
  `check_tidy_scope.cmake` (T-BUILD-5 a/b/c: `--dump-config` and `--list-checks` for
  `src/main.cpp` and a `tests/unit/` file; the unit config must equal the src one plus
  `,-cert-err58-cpp` with every other key identical; the dumped src config rejects the fixture with
  `[cert-err58-cpp]`), `check_tidy_enabled.cmake` (T-BUILD-1: tidy property on all three targets,
  every one of the 40 project sources has a `__run_co_compile --tidy=` rule in the generated
  Makefiles, `WarningsAsErrors: '*'`, target source sets match §7), `check_tidy_missing.cmake`
  (T-BUILD-2: configures a scratch build with `CMAKE_FIND_ROOT_PATH` rerooted to an empty
  directory, `CMAKE_FIND_ROOT_PATH_MODE_PROGRAM=ONLY`; expects failure, `CLANG_TIDY_EXE-NOTFOUND`,
  and the message naming clang-tidy and `-DPHILIOS_CLANG_TIDY=OFF`),
  `check_compile_database.cmake` (T-BUILD-3: every project file has one entry whose only
  `-std=` flag is `-std=c++20`; T-BUILD-4: `philios_core` is `STATIC_LIBRARY` built from `src/`
  minus `main.cpp`, `philios` from `src/main.cpp` and links only `philios_core`; no `tests/` or
  Catch2 path in either target's include directories, any `src/` compile command, or
  `CMakeFiles/philios.dir/link.txt`), and shared `probe_common.cmake`.
- Generated-build inspection (T-BUILD-1, T-BUILD-4 link check) supports the `Unix Makefiles`
  generator only (the §7 target environment; Ninja is not installed); other generators fail the
  probe with an explicit message rather than passing silently.

Mutation checks (temporary copies under `mktemp -d`, repository untouched), each failed the probe
as intended: extra `tests/unit/` suppression → T-BUILD-5 (b); `cert-err58-cpp` removed for `src/`
→ (a); benign fixture → (c); `-std=gnu++20` → T-BUILD-3; `-I…/tests/unit` on a `src/` file and
Catch2 on the `philios` link line → T-BUILD-4; `SHARED_LIBRARY` → T-BUILD-4; one core rule
without `--tidy` → T-BUILD-1.

Commands and results (`ulimit -s` = 8192, GCC 13.3.0, clang-tidy 18.1.3, CMake 3.28.3):
- `build-asan` and `build-release` had cached `PHILIOS_CLANG_TIDY=OFF`; reconfigured with
  `-DPHILIOS_CLANG_TIDY=ON` per §7 ("acceptance always runs with it on"). The Makefiles
  generator does not rebuild on a tidy-only rule change, so each tree was rebuilt clean.
- `timeout 1800 cmake --build build --clean-first -j8` (Debug, tidy ON) → exit 0, 40 files
  compiled and analyzed, no warnings or clang-tidy diagnostics; `timeout 900 ctest --test-dir
  build --output-on-failure --timeout 300 -j8 </dev/null` → **262/262** (257 + T-BUILD-1..5).
- `timeout 1800 cmake --build build-asan --clean-first -j8` → exit 0, 40 files, no diagnostics;
  serial `timeout 900 ctest --test-dir build-asan --output-on-failure --timeout 300` →
  **262/262** on the final run. Two earlier runs (each the first run after a fresh build) failed
  only `t_parse_5_long_line` with the driver's fixed 10 s `execute_process` timeout (10.29 s,
  10.24 s); reruns passed in 4.5–6.7 s, and the binary alone takes 3.8–4.8 s under ASan. This is a
  pre-existing timing margin unrelated to Pass F (the binary is identical with tidy on or off);
  not changed here because the 10 s value is fixed by §7.
- `timeout 1800 cmake --build build-release --clean-first -j8` → exit 0, 40 files, no
  diagnostics; `ctest … -j8` → **262/262**, including `t_sec_4_evaluation_depth`.
- T-CLI-10 **passed (not skipped)** in Debug, ASan/UBSan, and Release.
- A scratch `-DPHILIOS_CLANG_TIDY=OFF` configure succeeds and registers T-BUILD-2/3/4/6/7 but not
  T-BUILD-1/5.
- `build-tidy` was not rebuilt in this pass (duplicate of `build`).

Pass F outcome: **done** for T-BUILD-1..5. Still open (outside Pass F bounds): the task-1
documentation finding, and the ASan `t_parse_5_long_line` timing margin above (needs a developer
decision if the fixed 10 s driver timeout is to change).

## Code-review round 2 revision — Pass G (task-1 documentation) — 2026-11-15
Scope: only the round-2 Medium finding on task-1 documentation (plan §6, §8 task 1, D46). Identity
confirmed: Plan ID `0001`, plan path `plans/0001.md`; all artifacts use the exact `0001` stem. No
code, test, build, or other plan file changed. `docs/language_rules.md` unchanged (D37).

Changes:
- `AGENTS.md` (§6 only): Data Type Blueprint records `Integer` as 64-bit signed (D2) and leaves
  only the `UnsignedInteger` width open; numeric-semantics row narrowed accordingly. Rows marked
  resolved in place, citing plan 0001: evaluator control flow (D5), source encoding/column units
  (D8), compiler baseline (D9), build system/test framework with Catch2 3.16.1 test-only (D10,
  D49), and recursion limits (parser 256, call 1,000 per D7; evaluation depth 10,000 per D24, added
  to the row). Evaluation-order row records left-to-right call arguments (D6) and keeps operands and
  assignment targets open. Other rows and text unchanged.
- `docs/developers/src/getting-started/directory-structure.md`: added `cli/`,
  `runtime/object_model/`, and `main.cpp` to the tree and responsibilities, and a "Headers and
  tests" section stating D42 and D44 (§7.1).
- `.github/instructions/cpp-style-guide.instructions.md`: extended only the `#pragma once` /
  include-order bullet under "Naming & Layout" with the D43 rules (single `src/` root, full paths
  in double quotes, no `..`, angle brackets only for standard library and third-party headers);
  the include-order rule is kept.

Commands and results:
- `timeout 30 git --no-pager diff --check -- AGENTS.md docs .github` → clean.
- `timeout 30 git --no-pager diff --quiet -- docs/language_rules.md` → exit 0 (unchanged).
- New documentation lines are within 100 columns. No build or tests were rerun because only
  Markdown files changed.

Pass G outcome: **done.** Still open (outside Pass G bounds, unchanged): the ASan
`t_parse_5_long_line` timing margin noted in Pass F (needs a developer decision if the fixed 10 s
driver timeout is to change).
