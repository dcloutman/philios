# Plan 0001 — Code Review

## Review round 1 — 2026-11-15

### Identity and scope

- Confirmed Plan ID: `0001`
- Confirmed plan path: `plans/0001.md`
- Approved review record: `plans/0001_review.md` (latest approval: GO)
- Architecture record: `plans/0001-architecture_decisions.md`
- Implementation notes: `plans/0001-implementation_notes.md`
- Code-review report: `plans/0001-code_review.md`

The exact `0001` stem is consistent across all required paths. The implementation was reviewed against the approved plan, `AGENTS.md`, `docs/language_rules.md`, and `.github/instructions/cpp-style-guide.instructions.md`.

### Findings

#### High — The required acceptance suite is not implemented

**Evidence:** `CMakeLists.txt:57-71` registers only four CTest entries: the unit-test aggregate, T-CLI-10, T-BUILD-6, and T-BUILD-7. `tests/cli/run_case.cmake`, all CLI golden cases, `tests/build/check_tidy_scope.cmake`, and `tests/build/dynamic_static_init.cpp` are absent. Thus T-BUILD-1 through T-BUILD-5 and the required real-binary checks for T-CLI-1 through T-CLI-7, lexer/parser/validation failures, runtime behavior, error locations, and limits do not run. The implementation notes’ “full suite” claim refers only to this incomplete suite.

**Revision required:** Implement and register every acceptance test required by plan §9, including the CLI driver/golden cases and T-BUILD-1 through T-BUILD-5. Ensure all run in normal, ASan/UBSan, and required release configurations. Add regression cases for every defect below.

#### High — The mandatory clang-tidy gate is disabled and fails when enabled

**Evidence:** `CMakeLists.txt:9` defaults `PHILIOS_CLANG_TIDY` to `OFF`, contrary to plan §7. A clean configure/build with `-DPHILIOS_CLANG_TIDY=ON` failed at `src/analysis/semantic_analyzer/validator.cpp:29` (`bugprone-unchecked-optional-access`). Independent checks also found many required-as-error diagnostics in tests, including unchecked optional access, unused non-trivial locals, and easily swappable parameters. `tests/unit/.clang-tidy` also lacks the plan-required comment explaining its sole `cert-err58-cpp` suppression.

**Revision required:** Default the option to `ON`; make all three targets clean under `cert-*` and `bugprone-*` with warnings as errors; document the one approved test-only suppression; add and pass T-BUILD-1, T-BUILD-2, and T-BUILD-5.

#### High — Shipped language behavior violates the approved lexical and diagnostic contracts

Three independently reproduced defects are currently hidden by the missing CLI suite:

1. `src/frontend/lexer/lexer.cpp:531-532` accepts a line containing only `;`, although plan §3.3 requires it to be a lexical error. A program whose body is `;` followed by `pass` exits 0.
2. `src/frontend/lexer/lexer.cpp:499` reports indentation failures at the first token rather than at the start of the line required by plan §4. Three leading spaces report `main.phil:2:4`; the required location is line 2, column 1.
3. `src/runtime/evaluator/call.cpp:126` reports return-type mismatch at the function declaration. Plan §4 requires the `return` location. `main` returning a string reports `main.phil:1:10`; it should report the `return` on line 2, column 5.

**Revision required:** Reject a continuation marker unless code precedes it on that physical line; retain the physical line-start location for indentation diagnostics; propagate the executed return statement’s location into return-type checking. Add real-binary regression tests asserting class, line, column, status, empty stdout, and expected stderr.

#### Medium — Approved documentation updates were omitted

**Evidence:** Plan §6/task 1 requires updates to `AGENTS.md`, `docs/developers/src/getting-started/directory-structure.md`, and only the include-rule bullet of the C++ style guide. The current AGENTS open-decision table still says to ask about evaluator control flow, call-argument order, build/test framework, compiler baseline, and recursion limits; the directory document omits `src/cli/`, `src/runtime/object_model/`, `src/main.cpp`, and the headers/tests rules; the style guide omits the D43 include-root rules.

**Revision required:** Apply exactly the task-1 documentation changes specified by plan §§6 and 8 without changing `docs/language_rules.md`.

### Verification

- Normal debug build: passed, warning-free.
- ASan+UBSan build: passed.
- Release build: passed.
- CTest after each build: 4/4 entries passed; T-CLI-10 passed (not skipped) in all three builds.
- Required clang-tidy build: failed as described above.
- `git diff --check`: passed.
- Manual real-binary probes reproduced all three language/diagnostic defects above.

## Latest verdict: **NO-GO**

The implementation cannot pass the review gate until the high-severity conformance, static-analysis, and acceptance-suite gaps are corrected and verified.


## Review round 2 — 2026-11-15

### Identity and scope

- Confirmed Plan ID: `0001`
- Confirmed plan path: `plans/0001.md`
- Approved review record: `plans/0001_review.md`
- Architecture record: `plans/0001-architecture_decisions.md`
- Implementation notes: `plans/0001-implementation_notes.md`
- Code-review output: `plans/0001-code_review.md`
- Security report: `plans/0001-security_review.md`

All supplied paths exist and use the exact `0001` filename stem. This round inspected current code, full Plan §9, and every round-1 finding.

### Round-1 status

- **Acceptance suite:** Partially resolved. The real-binary CLI suite is now extensive and passes, but T-BUILD-1 through T-BUILD-5 remain incomplete as acceptance artifacts.
- **clang-tidy:** Implementation defect resolved: the option defaults to `ON`, all three targets are analyzed, the required checks pass, and the test-only suppression is documented. The required T-BUILD-5 scope probe remains absent.
- **Lexical/diagnostic defects:** Resolved. The semicolon, indentation-location, and executed-return-location regressions pass.
- **Documentation task 1:** Unresolved.

### Findings

#### High — T-BUILD-1 through T-BUILD-5 are still not completed as required acceptance coverage

**Evidence:** `CMakeLists.txt` registers only T-BUILD-6 and T-BUILD-7. The plan-required `tests/build/check_tidy_scope.cmake` and `tests/build/dynamic_static_init.cpp` do not exist, so T-BUILD-5 cannot run at all. This directly contradicts Plan §8 task 2 and §9, and the implementation notes themselves still list T-BUILD-1..5 as open. There is no committed acceptance check for the clean all-target tidy build, missing-tool failure, strict `c++20` compile database, or production/test dependency separation.

Manual review found the underlying T-BUILD-1, T-BUILD-3, and T-BUILD-4 properties currently sound: a clean all-target clang-tidy build passed; all 40 project compile-database entries use `-std=c++20`; production compile commands contain no test include path; and generated rules invoke clang-tidy for all three targets. The missing-tool branch has a suitable diagnostic in CMake, but no acceptance probe verifies it. These manual observations do not replace the plan-required tests, especially the three-part T-BUILD-5 confinement test.

**Revision required:** Add and register the T-BUILD-1..5 verification required by §9. At minimum, add the exact T-BUILD-5 script and fixture, verify production enables `cert-err58-cpp`, unit tests disable only that check, and prove the fixture fails under the production configuration. Add durable checks for T-BUILD-1..4 rather than relying on reviewer inspection, then run them in the required configurations.

#### Medium — Documentation task 1 remains entirely unapplied

**Evidence:** `AGENTS.md:58-67` still lists evaluator completion flow, argument evaluation order, compiler baseline, build/test framework, and depth values as open. `docs/developers/src/getting-started/directory-structure.md` still omits `src/cli/`, `src/runtime/object_model/`, `src/main.cpp`, and the required headers/tests section. The include-rule bullet in `.github/instructions/cpp-style-guide.instructions.md` still omits the single `src/` include root, full-path/double-quote, no-`..`, and angle-bracket rules.

**Revision required:** Apply exactly Plan §8 task 1 and §6. Do not change `docs/language_rules.md` or unrelated style-guide text.

#### Medium — The `12abc` source-location contract is unresolved and the acceptance test deliberately omits the column

**Evidence:** `tests/cli/cli_cases.cmake:219-223` passes no `COLUMN` for `t_lex_8_digits_then_identifier`, and `run_case.cmake` therefore accepts any positive column. The implementation notes acknowledge that the approved text can mean either the literal start (`1`, column 12, current behavior) or the offending identifier byte (`a`, column 14). Plan §9 says CLI golden tests check status, class, line, and column, so this weakened assertion is not final acceptance coverage.

**Revision required:** Obtain and record a developer decision in the authoritative plan clarifying the malformed-number location, then enforce that exact column in the CLI test. Do not silently choose language behavior in implementation or review.

### Confirmed corrections and verification

- **Pass D stack correction:** The refactoring in `evaluate.cpp` keeps non-recursive work out of recursive evaluator frames without changing limits or evaluation order. On the default 8 MiB stack, the full ASan/UBSan suite passed, including T-SEC-4 at `main.phil:4:468`; the release T-SEC-4 also passed.
- **clang-tidy:** A clean build of all three targets with clang-tidy 18.1.3 and `PHILIOS_CLANG_TIDY=ON` passed with no diagnostics. Effective configurations are `cert-*`,`bugprone-*` for `src/` and the same checks minus only `cert-err58-cpp` under `tests/unit/`.
- **CLI/acceptance:** Full Debug/tidy and ASan/UBSan CTest runs passed 257/257. The CLI suite covers T-CLI-1..7 and the required language/runtime CLI cases. T-CLI-10 passed (not skipped) in Debug/tidy, ASan/UBSan, and Release; `source_file.cpp:108` includes `O_NOCTTY` in the approved open flags.
- **Round-1 regressions:** Real-binary tests now reject invalid semicolon placement, report indentation errors at column 1, and report return-type mismatches at the executed `return`.
- `git diff --check` still reports the pre-existing trailing blank line in `plans/0001-security_review.md`; no implementation file was changed by this audit.

## Latest verdict: **NO-GO**

Runtime, CLI, sanitizer, Pass D, and clang-tidy behavior now pass, but the required T-BUILD-1..5 acceptance work, task-1 documentation, and exact malformed-number source-location contract remain incomplete.


## Review round 3 — 2026-11-15

### Identity and scope

- Confirmed Plan ID: `0001`
- Confirmed plan path: `plans/0001.md`
- Architecture record: `plans/0001-architecture_decisions.md`
- Implementation notes: `plans/0001-implementation_notes.md`
- Code-review report: `plans/0001-code_review.md`

All required paths exist and use the exact `0001` filename stem. This round reviewed Passes E, F,
and G, rechecked the round-2 open items, inspected their surrounding implementation for regressions,
and verified the required Debug/tidy, ASan+UBSan, and Release configurations.

### Round-2 finding status

- **High — T-BUILD-1 through T-BUILD-5:** Resolved. The five probes are implemented and registered.
  They verify all-target tidy execution, missing-tool failure, strict ISO C++20, production/test
  separation, and the sole test-only `cert-err58-cpp` suppression. All five passed in every build.
- **Medium — Task-1 documentation:** Resolved. `AGENTS.md`, the source-directory document, and only
  the style guide include-rule bullet contain the required updates. `docs/language_rules.md` remains
  unchanged.
- **Medium — `12abc` location:** Resolved by the developer decision. The plan records the literal’s
  first digit, and `t_lex_8_digits_then_identifier` now requires line 2, column 12 and passes.

### Findings

No new findings. No requirement, API-contract, RAII/lifetime, const-correctness, or coding-guideline
regression was identified in the reviewed changes.

### Verification

Toolchain: GCC 13.3.0, CMake 3.28.3, clang-tidy 18.1.3.

- **Debug + clang-tidy ON:** configure passed; clean build passed with all 40 project translation
  units compiled/analyzed and no compiler or clang-tidy diagnostics; CTest passed **262/262**.
- **ASan+UBSan + clang-tidy ON:** configure and clean build passed without diagnostics; serial CTest
  passed **262/262**, including T-SEC-4 and the long-line case.
- **Release + clang-tidy ON:** configure and clean build passed without diagnostics; CTest passed
  **262/262**.
- **T-CLI-10:** passed, not skipped, in Debug, ASan+UBSan, and Release. Code review also confirmed
  `O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC | O_NOCTTY` in the loader.
- **CTest registration:** the main unit entry excludes the exact T-CLI-10 name, the dedicated entry
  selects it, and the zero-match guard remains registered.
- `git diff --check` reports only the previously documented blank line at EOF of
  `plans/0001-security_review.md`; no implementation or task-1 documentation whitespace error was
  found.

## Latest verdict: **GO**

The three round-2 blockers are closed, all required configurations and tests pass, and no regression
was found.
