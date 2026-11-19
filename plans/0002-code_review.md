# Plan 0002 — Code Review

## Task 1 — Review round 1 (2026-11-18)

- **Confirmed plan ID:** `0002`
- **Confirmed plan path:** `plans/0002.md`
- **Task:** `1` — CLI driver support for stdout
- **Reviewed files:** `tests/cli/run_case.cmake`, `tests/cli/cli_cases.cmake`

### Findings by severity

- **Critical:** None. **Actionable:** No.
- **High:** None. **Actionable:** No.
- **Medium:** None. **Actionable:** No.
- **Low:** None. **Actionable:** No.

No revision items are required. The implementation stays within task scope and meets the declared acceptance criteria. The stdout capture/hex comparison, default empty-stdout check, all four `STDOUT_MODE` values, generated modes, status handling, and skip plumbing conform to plan §7 task 1 and the task record.

### Verification

- Debug build completed successfully; full Debug suite passed: 262/262.
- ASan build completed successfully with `-j2`; full ASan suite passed: 262/262.
- Manually exercised `capture`, `dev_full`, `closed`, and `broken_pipe`.
- Verified exact nonempty stdout hex acceptance, mismatch rejection, and unexpected-stdout rejection.
- Exercised `pseudo_nesting` at nesting 256 and 257 and validated the generated structure/location.
- Exercised and inspected `broken_pipe_program`, including its 65,536-byte argument and line 2, column 5 `echo`.
- Verified invalid stdout modes and incompatible `EXPECT_STDOUT_HEX` usage are rejected.

### Verdict

**GO**


## Task 2 — Review round 1 (2026-11-18)

- **Confirmed plan ID:** `0002`
- **Confirmed plan path:** `plans/0002.md`
- **Task:** `2` — Lexer: reserved words, `-`, negative literals, `?`

### Findings by severity

- **Critical:** None. **Actionable:** No.
- **High:** None. **Actionable:** No.
- **Medium:** None. **Actionable:** No.
- **Low:** None. **Actionable:** No.

No revision items are required. The implementation remains within Task 2 scope and satisfies T-UPD-1 through T-UPD-3 and T2-LEX-1 through T2-LEX-7. Bounds-checked lookahead uses `Lexer::peek`; negative conversion parses into `std::uint64_t`, caps the magnitude at 2^63, negates with unsigned arithmetic, and safely accepts `INT64_MIN`. Expression-start recognition is limited to the previous emitted `(`, `,`, or `return` token.

The ten T-UPD-3 fixtures were verified byte-positionally: function names begin at line 3, column 10 and parameter names at line 3, column 13, matching the established T-LEX-10 fixture form. Their registrations require syntax errors at those exact locations. T-UPD-1 fixture edits are exact `-` to `@` substitutions, and the expected classes/locations remain intact; T-UPD-2 now tests successful loading and exit 255 rather than weakening the former failure assertion. Task 1 driver behavior and the existing review section remain intact.

### Verification

- Debug build succeeded with clang-tidy enabled; full suite passed: 298/298.
- ASan+UBSan build succeeded with clang-tidy enabled; full suite passed: 298/298.
- Task 2 CLI subset passed: 37/37.
- T2-LEX unit subset passed: 7 cases, 161 assertions; T-UPD-2 unit case passed.

### Verdict

**GO**


## Task 3 — Review round 1 (2026-11-18)

- **Confirmed plan ID:** `0002`
- **Confirmed plan path:** `plans/0002.md`
- **Task:** `3` — Types

Interpreting the function header as `function f():` for an omitted annotation and `function f(): <type>:` for an annotation is planned behavior, not an unplanned grammar change. Both plan 0001 §3.4 and plan 0002 §3.3 specify an optional colon-plus-type annotation followed by the required body colon, and the parser implements that exactly.

### Findings by severity

- **Critical:** None. **Actionable:** No.
- **High:** None. **Actionable:** No.
- **Medium:** None. **Actionable:** No.
- **Low: ASan long-line timeout margin.** `t_parse_5_long_line` can approach or exceed its fixed 10-second driver timeout under parallel ASan load. This is pre-existing load-related flakiness, not a Task 3 regression: plan 0001 notes record the same failure before Task 3 (10.24/10.29 seconds under load versus 3.8–6.7 seconds on rerun); the long-line path parses approximately 699,000 call arguments, while Task 3 parser changes add only constant work to the two function headers. The current full ASan run passed this test in 9.73 seconds; three isolated runs passed in 4.39, 5.75, and 7.31 seconds. **Actionable:** No for Task 3; the project-wide timeout margin may be addressed separately.

No revision items are required. `DeclaredType` remains a plain, non-owning aggregate, and the added `noexcept` contracts are sound. D25 adjacency, the §3.4 acceptance table, D29 bare returns and fall-through behavior, `scriptTypeName`, `main` annotations, and error locations conform to the plan.

### Verification

- Debug build and full suite: 337/337 passed.
- ASan+UBSan build and full suite: 337/337 passed.
- clang-tidy build: clean.
- Targeted Task 3 and plan 0001 compatibility subset: 61/61 passed.

### Verdict

**GO**


## Task 4 — Review round 1 (2026-11-18)

- **Confirmed plan ID:** `0002`
- **Confirmed plan path:** `plans/0002.md`
- **Task:** `4` — Output seam
- **Reviewed files:** `src/runtime/evaluator/script_output.hpp`, `src/runtime/evaluator/execution_context.hpp/.cpp`, `src/common/errors.hpp`, `src/cli/standard_output.hpp/.cpp`, `src/cli/cli.hpp/.cpp`, `tests/unit/support/capturing_output.hpp/.cpp`, and the Task 4 test adaptations

### Findings by severity

- **Critical:** None. **Actionable:** No.
- **High:** None. **Actionable:** No.
- **Medium:** None. **Actionable:** No.
- **Low: Project include ordering in `execution_context.cpp`.** The style guide requires the matching header first, followed by one alphabetically sorted project-header group. `src/runtime/evaluator/execution_context.cpp:3-7` places `runtime/evaluator/script_output.hpp` in a second project group after `runtime/object_model/null_object.hpp`, so the project headers are neither one group nor alphabetically ordered. **Actionable:** Yes. Consolidate and alphabetize the project includes (or remove a redundant direct include if include-what-you-use remains satisfied).

No requirement, API-contract, RAII/lifetime, const-correctness, or task-scope defects were found. The output writer is owned by value, empty writers are rejected before `NullObject` construction, `writeOutput` forwards bytes and the exact error code, the shipped writer handles short writes and flush failures as specified, and `SIGPIPE` is ignored before loading `main.phil`. The include-order deviation requires a revision before approval.

### Verification

- Warning-enabled clean Debug build completed successfully with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`.
- Clean clang-tidy build completed with `cert-*` and `bugprone-*` and no diagnostics.
- Full Debug suite passed: 337/337.
- Clean ASan+UBSan build and full suite passed: 337/337; no sanitizer reports and no long-line rerun was needed.
- T2-ECHO-9 passed directly in Debug and ASan+UBSan: 2 test cases, 18 assertions in each build.
- T-BUILD-6 passed directly in Debug and ASan+UBSan.

### Verdict

**NO-GO**

## Task 4 — Review round 2 (2026-11-18)

- **Confirmed plan ID:** `0002`
- **Confirmed plan path:** `plans/0002.md`
- **Task:** `4` — Output seam
- **Reviewed files:** `src/runtime/evaluator/script_output.hpp`, `src/runtime/evaluator/execution_context.hpp/.cpp`, `src/common/errors.hpp`, `src/cli/standard_output.hpp/.cpp`, `src/cli/cli.hpp/.cpp`, `tests/unit/support/capturing_output.hpp/.cpp`, and the Task 4 test adaptations

### Findings by severity

- **Critical:** None. **Actionable:** No.
- **High:** None. **Actionable:** No.
- **Medium:** None. **Actionable:** No.
- **Low:** None. **Actionable:** No.

The round 1 Low include-order finding is resolved. `src/runtime/evaluator/execution_context.cpp` now keeps its matching header first and places `common/errors.hpp`, `runtime/evaluator/script_output.hpp`, and `runtime/object_model/null_object.hpp` in one alphabetically sorted project-header group. The direct `script_output.hpp` include remains appropriate because the implementation directly uses `OutputWriter`.

No revision items are required. Rechecking all Task 4 scope found no requirement, API-contract, RAII/lifetime, const-correctness, guideline, or scope defects. The task satisfies plan §3.6, plan §7 task 4, its task-record acceptance criteria, `AGENTS.md`, and the C++ style guide without depending on unfinished Task 5 behavior.

### Verification

- Clean warning-enabled Debug build completed successfully with no compiler warnings.
- Clean `build-tidy/` build completed with no clang-tidy diagnostics.
- Full Debug suite passed: 337/337.
- Clean ASan+UBSan build and full suite passed: 337/337; no sanitizer reports. `t_parse_5_long_line` passed in the full run, so no isolated rerun was needed.
- T2-ECHO-9 passed directly in Debug and ASan+UBSan: 2 test cases, 18 assertions in each build.
- T-BUILD-6 passed directly in Debug and ASan+UBSan.

### Verdict

**GO**

## Task 5 — Review round 1 (2026-11-18)

- **Confirmed plan ID:** `0002`
- **Confirmed plan path:** `plans/0002.md`
- **Task:** `5` — Pseudo-functions

### Findings by severity

- **Critical:** None. **Actionable:** No.
- **High:** None. **Actionable:** No.
- **Medium:** None. **Actionable:** No.
- **Low: Task 5 test code exceeds the 100-column limit.** The style guide applies to test C++ and requires a 100-column maximum. Task 5 added overlong lines at `tests/unit/frontend/ast/ast_test.cpp:143,178`, `tests/unit/frontend/parser/parser_test.cpp:156`, and `tests/unit/runtime/evaluator/evaluate_test.cpp:304,490,495` (101–104 columns). **Actionable:** Yes. Reflow the constructions and split or shorten the affected `TEST_CASE` names so every added line is at most 100 columns.

No behavioral, API-contract, RAII/lifetime, const-correctness, or scope defect was found. The pseudo-call AST ownership and iterative destruction, parser checks and nesting accounting, validation, evaluation sequencing and guards, output failure propagation, `String::bytes()` exception, and authorized `pseudo_nesting` correction conform to the plan. The style deviation requires revision before approval.

### Verification

- Debug, ASan, and clang-tidy builds completed successfully with no build diagnostics.
- Full Debug suite passed: 381/381.
- Full serial ASan suite passed 380/381; only the explicitly documented pre-existing `t_parse_5_long_line` 10-second driver-timeout flake failed. Two isolated retries also crossed that fixed timeout, with no sanitizer report. This is not a Task 5 finding.
- Targeted Task 5 subsets passed in Debug and ASan: 46/46 in each, including the unit binary, all `echo`/`typeof` CLI cases, `nullnull`, T2-BUILD-1, and T-BUILD-6.

### Verdict

**NO-GO**



## Task 5 — Review round 2 (2026-11-19)

- **Confirmed plan ID:** `0002`
- **Confirmed plan path:** `plans/0002.md`
- **Task:** `5` — Pseudo-functions

The plan, review, architecture-decision, task-record, implementation-notes, and code-review paths all use the exact `0002` stem. Task 5 exists and its `Implementation` field is `Done`.

### Findings by severity

- **Critical:** None. **Actionable:** No.
- **High:** None. **Actionable:** No.
- **Medium:** None. **Actionable:** No.
- **Low: Missing direct standard-library include in `evaluate.cpp`.** Task 5 added a direct use of `std::error_code` at `src/runtime/evaluator/evaluate.cpp:187`, but the file does not include `<system_error>` and currently obtains the declaration transitively through `execution_context.hpp`. The style guide requires include-what-you-use. **Actionable:** Yes. Add `#include <system_error>` to the alphabetically sorted standard-library include group in `src/runtime/evaluator/evaluate.cpp`.

The round 1 line-length finding is resolved: all six cited Task 5 test lines are now at most 100 columns. Pre-existing overlong lines and the Task 3 line identified in the revision notes were treated as out of scope. No behavioral, API-contract, RAII/lifetime, const-correctness, or task-scope defect was found. The authorized `String::bytes()` accessor and `pseudo_nesting` correction remain within their approved exceptions.

### Verification

- Debug, ASan, and clang-tidy builds completed successfully with no diagnostics.
- The Task 5 Debug subset passed: 46/46.
- The Task 5 ASan subset passed: 46/46, with no sanitizer reports.
- The full Debug suite passed 380/381 under parallel load; only the known `t_parse_5_long_line` timing flake failed, and its isolated rerun passed in 2.72 seconds.
- The required serial ASan command ran 380/381; only `t_parse_5_long_line` crossed its fixed 10-second driver timeout. Isolated ASan retries also crossed that timeout without a sanitizer report. This is the documented pre-existing plan 0001 baseline flake and is not a Task 5 finding.
- `git diff --check` passed for the Task 5 files.

### Verdict

**NO-GO**


## Task 5 — Review round 3 (2026-11-19)

- **Confirmed plan ID:** `0002`
- **Confirmed plan path:** `plans/0002.md`
- **Task:** `5` — Pseudo-functions

The plan, review, architecture-decision, task-record, implementation-notes, and code-review paths all use the exact `0002` stem. Task 5 exists and its `Implementation` field is `Done`.

### Findings by severity

- **Critical:** None. **Actionable:** No.
- **High:** None. **Actionable:** No.
- **Medium:** None. **Actionable:** No.
- **Low:** None. **Actionable:** No.

Both prior findings are resolved. The round 1 Task 5 test lines cited in that review are now at most 100 columns. The round 2 include-what-you-use finding is resolved by the directly included `<system_error>` in the sorted standard-library include group of `src/runtime/evaluator/evaluate.cpp`.

No regressions or new requirement, API-contract, RAII/lifetime, const-correctness, guideline, or task-scope defects were found in the complete Task 5 pass. The pseudo-call AST ownership and iterative destruction, parsing and nesting rules, validation, evaluation guards and sequencing, output-error handling, tests, the authorized `String::bytes()` accessor, and the authorized `pseudo_nesting` correction conform to the approved plan and architecture decisions. No revision items are required.

### Verification

- Debug, clang-tidy, and ASan builds completed successfully with no diagnostics.
- Full Debug suite passed: 381/381.
- Full serial ASan suite passed: 381/381 with no sanitizer reports, using `ctest --test-dir build-asan --timeout 300 </dev/null` as required.
- The targeted Task 5 Debug subset passed: 46/46, including all `echo`/`typeof` cases, `nullnull`, T2-BUILD-1, and T-BUILD-6.
- `t_parse_5_long_line` passed in the serial ASan run in 2.86 seconds; its documented timing flake did not occur.
- Task 5 files passed `git diff --check`. Pre-existing or unrelated whitespace and overlength-line issues outside Task 5 remain out of scope.

### Verdict

**GO**

## Task 1 — Review round 2 (2026-11-19)

- **Confirmed plan ID:** `0002`
- **Confirmed plan path:** `plans/0002.md`
- **Task:** `1` — CLI driver support for stdout; developer-authorized long-line timeout correction

The plan, architecture-decision, task-record, implementation-notes, and code-review paths all use the exact `0002` stem. Task 1 exists and its `Implementation` field is `Done`.

### Findings by severity

- **Critical:** None. **Actionable:** No.
- **High:** None. **Actionable:** No.
- **Medium:** None. **Actionable:** No.
- **Low:** None. **Actionable:** No.

No revision items are required. The correction is limited to the authorized scope. `run_timeout` defaults to 10 seconds and changes to 20 seconds only for `MODE long_line`; all four interpreter `execute_process` branches use that value. `t_parse_5_long_line` is the only registered CMake case using `MODE long_line`. The header documents both limits. The correction does not alter `tests/cli/cli_cases.cmake` or `CMakeLists.txt`; the CTest `TIMEOUT 120` remains unchanged. No driver behavior beyond the authorized run-time limit changed. This CMake-only correction introduces no C++ API, ownership, lifetime, RAII, or const-correctness concern.

The implementation notes record Debug, parallel ASan, and serial ASan timings. Their maximum observed long-line duration was 9.03 seconds, and they explicitly state that no run exceeded 20 seconds, so no deeper over-20-second explanation is required.

### Verification

- `cmake --build build` completed successfully with no diagnostics.
- `ctest --test-dir build -j8 </dev/null` passed: 381/381.
- `ctest --test-dir build-asan -R t_parse_5_long_line --output-on-failure` passed: 1/1 in 2.79 seconds, with no sanitizer report.
- Relevant files passed `git diff --check`.

### Verdict

**GO**

## Task 6 — Review round 1 (2026-11-19)

- **Confirmed plan ID:** `0002`
- **Confirmed plan path:** `plans/0002.md`
- **Task:** `6` — Documentation

The plan, architecture-decision, task-record, implementation-notes, and code-review paths all use
the exact `0002` stem. Task 6 exists and its `Implementation` field is `Done`.

### Findings by severity

- **Critical:** None.
- **High: `copy_folder` can write into its source through a symlinked parent.**
  `tests/cli/run_case.cmake:109-116` resolves `WORK_DIR` only when that exact path already exists;
  otherwise it performs lexical normalization, which does not resolve an existing symlink in a
  parent component. With `FIXTURE_DIR=/tmp/source`, `/tmp/alias -> /tmp/source`, and the initially
  absent `WORK_DIR=/tmp/alias/new-work`, the overlap checks pass and the driver creates
  `/tmp/source/new-work`, writing copied files and `stdout.bin` beneath the source. This violates
  the read-only source and overlap-safety acceptance criteria. **Actionable:** Resolve existing
  ancestors/canonicalize the effective destination before deletion or creation, reject all aliasing
  overlap cases, and add regression coverage for a non-existent `WORK_DIR` beneath a symlink to
  `FIXTURE_DIR` (and the inverse containment case).
- **Medium: The documented string escape list is malformed and inaccurate.**
  `docs/end-users/src/reference/source-files.md:83-84` contains a literal line feed and tab inside
  inline code and lists `\` and `"` as bare characters rather than showing the four Philios escape
  spellings `\\`, `\"`, `\n`, and `\t`. **Actionable:** Restore an unambiguous list or table of
  the exact source spellings and meanings.
- **Medium: The pseudo-function error rules are incomplete.**
  `docs/end-users/src/reference/functions.md:52-66` omits that pseudo-functions are callable-only,
  zero/multiple arguments are syntax errors, and literal integer/negative-integer/`null` arguments
  to `echo` are syntax errors before evaluation. It also mentions only evaluation/depth errors for
  `typeof`, omitting declaration errors from invalid arguments such as `typeof(main)` or an
  undefined name. This does not satisfy the page-specific requirement to document their rules and
  errors. **Actionable:** Add the parse-time and validation-time rules and distinguish them from
  `echo`'s runtime argument-type and output-write failures.

No C++ ownership, lifetime, RAII, or const-correctness issue applies to this documentation/driver
scope. The T2-DOC-1 registration has the required status, stdout bytes, and empty stderr; the sample
and shipped example are byte-identical; protected files are unchanged; and relative links resolve.

### Verification

- `cmake --build build` passed.
- `ctest --test-dir build -j8 </dev/null` passed: 382/382.
- The changed-page Philios program ran from `/tmp`: exit 1, stdout hex
  `48656c6c6f20576f726c64`, empty stderr.
- `cmp` passed for both files in `tests/unit/sample-programs/hello-world/` against
  `docs/examples/hello_world/`.
- `docs/language_rules.md`, `AGENTS.md`, and `CMakeLists.txt` have no diff.
- A temporary-directory probe reproduced the symlink-parent source write described above.

### Verdict

**NO-GO**

## Task 6 — Review round 2 (2026-11-19)

- **Confirmed plan ID:** `0002`
- **Confirmed plan path:** `plans/0002.md`
- **Task:** `6` — Documentation

The plan, review, architecture-decision, task-record, implementation-notes, and code-review paths
all use the exact `0002` stem. Task 6 exists and its `Implementation` field is `Done`.

### Findings by severity

- **Critical:** None.
- **High:** None. The round 1 High finding is resolved. `tests/cli/run_case.cmake:107-144`
  canonicalizes `FIXTURE_DIR` and the deepest existing `WORK_DIR` ancestor before either deletion
  or creation, then rejects overlap in both directions. The regression
  `t2_doc_1_copy_folder_rejects_alias` at `tests/cli/cli_cases.cmake:757-857` covers missing
  destinations below symlinked parents, equal and containing aliases, both containment directions,
  and a non-overlapping control.
- **Medium: The end-user index still does not link `echo` or `typeof`.** The Task 6 file-specific
  requirement says `docs/end-users/src/index.md` must link `echo` and `typeof` under the implemented
  reference pages. Its implemented-reference list at `docs/end-users/src/index.md:9-13` links only
  the four page titles and contains neither pseudo-function name. The planned `echo` and Hello
  World entries were correctly removed, but that does not satisfy the positive link requirement.
  **Actionable:** Add links for both `echo` and `typeof` under the implemented Functions reference
  entry, targeting their sections in `reference/functions.md`.
- **Low:** None.

Both round 1 Medium findings are resolved. The escape table at
`docs/end-users/src/reference/source-files.md:81-88` displays the exact source spellings `\\`,
`\"`, `\n`, and `\t` with their meanings. `docs/end-users/src/reference/functions.md:52-107`
documents callable-only syntax, argument-count and literal-argument parse errors, validation
errors, nesting, and the runtime argument/output errors, with exit 65 distinguished from exit 70.

The remaining T2-DOC-1 and T2-DOC-2 checks pass: the Hello World case has the required exact
output and status; the documentation notes account for every listed file; required language and
error rules are documented; changed-page relative links resolve; the sample files match the
shipped example; and `CMakeLists.txt`, `docs/language_rules.md`, `docs/examples/`, and `AGENTS.md`
are unchanged. No C++ API, RAII/lifetime, or const-correctness concern applies to this task.

### Verification

- `cmake --build build && ctest --test-dir build -j8 </dev/null` passed: 383/383.
- T2-DOC-1 includes both `t2_doc_1_hello_world_example` and
  `t2_doc_1_copy_folder_rejects_alias`; both passed in the full suite.
- A clean CMake configure completed successfully.
- All relative links under the linked end-user and developer documentation resolved.
- `git diff --check` passed for the Task 6 documentation and test-driver files.
- Protected files have no diff, and both sample files are byte-identical to
  `docs/examples/hello_world/`.

### Verdict

**NO-GO**

## Task 6 — Review round 3 (2026-11-19)

- **Confirmed plan ID:** `0002`
- **Confirmed plan path:** `plans/0002.md`
- **Task:** `6` — Documentation

The plan, review, architecture-decision, task-record, implementation-notes, and code-review paths
all use the exact `0002` stem. Task 6 exists and its `Implementation` field is `Done`.

### Findings by severity

- **Critical:** None.
- **High:** None.
- **Medium:** None.
- **Low:** None.

The round 2 finding is resolved. `docs/end-users/src/index.md` now links `echo` and `typeof`
beneath the implemented Functions reference entry. Both relative targets exist, and the fragments
`#echostring-str-null` and `#typeofexpr-string` match the GitHub-style slugs of their headings in
`reference/functions.md`. The planned-topics list still omits the now-implemented `echo` and Hello
World entries while retaining the other planned topics.

No regressions were found against Task 6's acceptance criteria. The earlier round 1 fixes remain in
place, the linked documentation contains the required implemented behavior and no prohibited stale
claims, all checked relative links and anchors resolve, the sample files remain byte-identical to
the shipped example, and protected files remain unchanged. No actionable revision items remain.

### Verification

- `cmake --build build -j8 && ctest --test-dir build -j8 </dev/null` passed: 383/383.
- The targeted `t2_doc` subset passed: 2/2.
- Both new index links and heading anchors resolved.
- `git diff --check` passed for Task 6 documentation and test-driver files.
- `docs/language_rules.md`, `AGENTS.md`, `CMakeLists.txt`, and `docs/examples/` have no diff.
- Both sample files are byte-identical to `docs/examples/hello_world/`.

### Verdict

**GO**

## Task 7 — Review round 1 (2026-11-19)

- **Confirmed plan ID:** `0002`
- **Confirmed plan path:** `plans/0002.md`
- **Task:** `7` — End-to-end verification

The plan, review, architecture-decision, task-record, implementation-notes, and code-review paths
all use the exact `0002` stem. Task 7 exists and its `Implementation` field is `Done`.

### Findings by severity

- **Critical:** None.
- **High:** None.
- **Medium:** None.
- **Low:** None.

No actionable revision items were found. The Task 7 acceptance report contains every required
item and its claims were independently confirmed. **Affected task numbers:** None.

Task 7 stayed within scope. No source, test, or documentation file is newer than the Task 7
acceptance report; the latest such change predates it by approximately 29 minutes. The only Task 7
implementation artifacts are the acceptance report in `plans/0002-implementation_notes.md` and
the Task 7 progress fields in `plans/0002-implementation-tasks.md`.

### Verification

- Default stack size: 8192 KiB.
- Clean warning-enabled builds completed without warning or error diagnostics. All configured
  builds have `PHILIOS_CLANG_TIDY=ON`; `t_build_1_clang_tidy_all_targets` and
  `t_build_5_clang_tidy_scope` passed in Debug, ASan, and Release.
- Full Debug suite passed: 383/383.
- Full ASan+UBSan suite passed: 383/383, with no sanitizer report.
- Full Release suite passed: 383/383.
- The four developer examples independently produced the reported exact stdout and statuses:
  `Hello World`/1, `String`/1, `Integer`/1, and empty stdout/255 for `return -1`; stderr was empty.
- `t2_echo_6_dev_full` and `t_cli_10_terminal_device_main_phil` ran and passed, rather than
  skipping, in Debug, ASan, and Release.
- Both T2-DOC-1 cases passed in all three builds; T2-DOC-2 has Task 6 review round 3 `GO`.
- Every `ctest` invocation used standard input redirected from `/dev/null`.

### Verdict

**GO**
