# Plan 0003 — Code Review

## Task 1 — Lexer — Round 1

- **Confirmed plan ID:** `0003`
- **Confirmed plan path:** `plans/0003.md`
- **Task number:** `1` (Lexer, plan T1)
- **Reviewed files:** `src/frontend/lexer/token.hpp`, `src/frontend/lexer/lexer.cpp`, `tests/unit/frontend/lexer/lexer_test.cpp`

### Findings

No findings. The implementation satisfies plan §4.2.1–§4.2.4, §5.1, §8 T1, and the lexer-level coverage for T3-LEX-1 through T3-LEX-6. Maximal munch, EOF lookahead, negative-literal contexts, identifier rejection (`_`, `__`, and underscore-digit forms), byte columns, and continuation behavior are covered and correct. No RAII, lifetime, API-contract, const-correctness, or style issues were found.

### Scope and preservation audit

The read-only Git diff contains only the three task-scoped files. All changed or replaced existing lines are directly required to add the identifier rule, seven reserved words, four operator token kinds, maximal-munch lexing, negative-literal contexts, documentation for those token contracts, and regression tests. No unauthorized deletion, move, rename, regrouping, re-indentation, or reformatting was found.

`src/frontend/lexer/lexer.hpp` still has the known stale comments about 28 reserved words and the old negative-literal contexts. Per the developer direction, it is outside T1 scope and is noted without a finding.

### Verification

- `cmake --build build`: passed, warning-free.
- `ctest --test-dir build --output-on-failure`: 383/383 passed.
- `cmake --build build-asan`: passed, warning-free.
- `ctest --test-dir build-asan --output-on-failure`: 383/383 passed; no sanitizer failure.
- `clang-tidy-18 -p build src/frontend/lexer/lexer.cpp tests/unit/frontend/lexer/lexer_test.cpp`: passed with no user-code diagnostics.
- `git diff --check` for all task files: passed.

### Verdict

**GO**


## Task 2 — AST — Round 1

- **Confirmed plan ID:** `0003`
- **Confirmed plan path:** `plans/0003.md`
- **Task number:** `2` (AST, plan T2)

### Findings

No findings. The AST nodes satisfy the required ownership, const-correctness, RAII, and iterative `noexcept` destruction contracts. Every named AST node owns its text. `asFunctionDecl` checks null, uses the node kind, shares the existing control block for function declarations, and returns null for other kinds. Coverage is adequate for T3-ARCH-9, the T3-NEST-5 destruction portion, node shapes, and `asFunctionDecl`.

### Scope and preservation audit

The read-only Git diff was audited for every Task 2 file and DR-1(a) placeholder. Source changes stay within Task 2 scope or the expressly authorized compile-only edits in `validator.cpp`, `evaluate.cpp`, `module.cpp`, and `type_check.cpp`. The `parser_test.cpp` changes only route existing accesses through `asFunctionDecl`; test inputs and assertion semantics are unchanged. No unauthorized deletion, move, rewording, regrouping, re-indentation, or reformatting was found.

The known stale `Module::load` comment in `module.hpp` was left unchanged as directed and is not a finding.

### Verification

- `cmake --build build`: passed, warning-free.
- `cmake --build build-asan`: passed, warning-free.
- `ctest --test-dir build --output-on-failure`: 383/383 passed.
- `ctest --test-dir build-asan --output-on-failure`: 383/383 passed; no sanitizer failure.
- `clang-tidy-18 -p build` on all Task 2-touched `.cpp` files: passed with no user-code diagnostics.
- Task 2-scoped `git diff --check`: passed.
- Repository-wide `git diff --check`: blocked only by pre-existing, out-of-scope trailing whitespace in `plans/0005.md`; no Task 2 file was reported.

### Verdict

**GO**


## Task 4 — Object model — Round 1

- **Confirmed plan ID:** `0003`
- **Confirmed plan path:** `plans/0003.md`
- **Task number:** `4` (Object model, plan T4)

### Findings

No Critical, High, Medium, or Low findings. `Boolean` and `Symbol` have the required tags, inheritance, immutability/identity representation, `noexcept` and const-correct interfaces, and disabled copy/move operations. No mutable global or static state was added. `Symbol` has no data members, AST pointer, or source view. The DR-1(a) `scriptTypeName` cases are compile-only placeholders for Task 5, and `CMakeLists.txt` correctly remains unchanged under DR-2. Test coverage is adequate for the Task 4 acceptance criteria.

The known stale `ImmutableObject` comment in `object.hpp` is pending developer direction and is not a finding.

### Scope and preservation audit

The read-only Git diffs for every Task 4 file and the DR-1(a) placeholder were audited. Existing-file changes are additions only: two `ObjectType` enumerators, the authorized placeholder cases, includes/documentation, and appended tests. The four new object-model files contain only the required classes and implementations. No existing line was deleted, moved, renamed, regrouped, re-indented, reformatted, or reworded. No out-of-scope source change was found.

### Verification

- `cmake --build build`: passed, warning-free.
- `cmake --build build-asan`: passed, warning-free.
- Debug and ASan `ctest`: 382/383 passed in each build. The sole failed entry was aggregate `philios_tests`; in each build it contained exactly the 19 known Task 3 parser cases (110 failed assertions). Every other ctest passed.
- Targeted Task 4 tests in both builds: 36/36 assertions passed across 4 test cases; no sanitizer diagnostic.
- `clang-tidy-18 -p build` on `boolean.cpp`, `symbol.cpp`, `type_check.cpp`, and `object_test.cpp`: passed with no user-code diagnostics.
- Task 4-scoped `git diff --check`: passed.

### Verdict

**GO**

## Task 3 — Parser — Round 1

- **Confirmed plan ID:** `0003`
- **Confirmed plan path:** `plans/0003.md`
- **Task number:** `3` (Parser, plan T3)
- **Reviewed files:** `src/frontend/parser/parser.hpp`, `src/frontend/parser/parser.cpp`, `tests/unit/frontend/parser/parser_test.cpp`

### Findings

1. **Medium — `src/frontend/parser/parser.cpp:251`; `tests/unit/frontend/parser/parser_test.cpp:460` — Statement-position `a === b` and `a !== b` do not satisfy or test the specific §5.2 location contract.** `parseStatement()` rejects these forms at the first `==`/`!=` token because it applies the generic identifier-statement rule. Plan §5.2 also explicitly lists `a === b` and `a !== b` in the second-`=` row and requires the error at that `=`; the test intentionally excludes statement position. This leaves an acceptance-criteria condition unresolved while Task 3 is marked Done. **Revision:** obtain/record the developer resolution for the conflicting §5.2 rows, then implement the resolved location and add statement-position regression cases for both malformed forms.

2. **Low — `src/frontend/parser/parser.cpp:371`; `tests/unit/frontend/parser/parser_test.cpp:275` — Completed code still claims Task 3 stubs throw `InternalError`.** The parser section says its bodies are stubs that throw until implementation, and the test helper still describes the “InternalError of a task 3 stub.” Both are false after implementation. **Revision:** replace these stale stub-stage statements with present-tense descriptions without reformatting unrelated text.

3. **Low — `src/frontend/parser/parser.cpp:482` — New code exceeds the project’s 100-column limit.** The `symbolKeyword` declaration is 106 columns. **Revision:** wrap only this declaration to comply with the C++ style guide.

No RAII, lifetime, ownership, API-contract, or const-correctness defects were otherwise found.

### Scope and preservation audit

The read-only Git diff for all three Task 3 scope files was audited. The parser changes implement the required module-item grammar, declarations, assignment, precedence levels, Boolean/Symbol types, syntax errors, and nesting accounting. The existing parser-test changes before the appended Task 3 tests are the Task 2 mechanical `asFunctionDecl` adaptation already authorized by AC-2. No unauthorized deletion, move, rename, regrouping, re-indentation, reformatting, or documentation-placeholder removal was found; therefore there is no preservation High finding.

### Verification

- `cmake --build build`: passed, warning-free.
- `ctest --test-dir build --output-on-failure`: 383/383 passed.
- `cmake --build build-asan`: passed, warning-free.
- `ctest --test-dir build-asan --output-on-failure`: 383/383 passed; no sanitizer failure. `t_parse_5_long_line` passed in this full run.
- `clang-tidy -p build-tidy` with `cert-*,bugprone-*` and warnings as errors on all three Task 3 files: passed with no user-code diagnostics.
- Task 3-scoped `git diff --check`: passed.

### Verdict

**NO-GO**



## Task 3 — Parser — Round 2

- **Date:** 2026-11-24
- **Confirmed plan ID:** `0003`
- **Confirmed plan path:** `plans/0003.md`
- **Task number:** `3` (Parser, plan T3)
- **Reviewed files:** `src/frontend/parser/parser.hpp`, `src/frontend/parser/parser.cpp`, `tests/unit/frontend/parser/parser_test.cpp`

### Findings

No Critical, High, Medium, or Low findings. No actionable revision items remain.

### Round 1 finding resolution

1. **Resolved (Medium):** The developer decision dated 2026-11-22 is recorded in the task change log and implementation notes. `parseStatement()` now reports statement-position `a === b` and `a !== b` at the second `=` with `invalid expression`, while `a == b` and `a != b` still fail at their operator. The added T3-PARSE-2 statement-position regression test covers all four forms.
2. **Resolved (Low):** The stale parser and test-helper stub comments now describe implemented behavior in present tense.
3. **Resolved (Low):** The `symbolKeyword` declaration is wrapped within the 100-column limit. The unchanged 104-column line at `parser.cpp:160` predates Task 3 and is not a Task 3 finding.

### Scope and preservation audit

The complete read-only Git diff for all three Task 3 files was re-audited. The implementation satisfies the full declared parser scope and acceptance criteria without relying on incomplete Task 5 work. Existing parser-test edits are limited to Task 2’s authorized `asFunctionDecl` adaptation; Task 3 additions implement and test the planned grammar, errors, AST shapes, and nesting limits. The removal of the obsolete statement-position exclusion comment is authorized by the developer decision. No unauthorized deletion, move, rename, reorder, regrouping, re-indentation, reformatting, or documentation-placeholder removal was found.

No RAII/lifetime, API-contract, const-correctness, requirement, or guideline regression was found. Recorded decisions DR-1(a), DR-3, and DR-4(target) remain respected. Task 5’s `type_check.*` work was excluded from this review.

### Verification

- `cmake --build build`: passed, warning-free.
- `cmake --build build-asan`: passed, warning-free.
- `ctest --test-dir build --output-on-failure`: 383/383 passed.
- `ctest --test-dir build-asan --output-on-failure`: 383/383 passed; `t_parse_5_long_line` passed, so no rerun was needed.
- `clang-tidy -p build-tidy` with `cert-*,bugprone-*` and warnings as errors on all three Task 3 files: passed with no user-code diagnostics.
- Task 3-scoped `git diff --check`: passed.

### Verdict

**GO**


## Task 5 — Type checks and equality — Round 1

- **Date:** 2026-11-24
- **Confirmed plan ID:** `0003`
- **Confirmed plan path:** `plans/0003.md`
- **Task number:** `5` (Type checks and equality, plan T5)
- **Reviewed files:** `src/runtime/evaluator/type_check.hpp`, `src/runtime/evaluator/type_check.cpp`, `tests/unit/runtime/evaluator/type_check_test.cpp`, `src/runtime/object_model/integer.hpp`, `src/runtime/object_model/integer.cpp`, and the Task 5 additions to `tests/unit/runtime/object_model/object_test.cpp`

### Findings

1. **Low — `src/runtime/evaluator/type_check.hpp:15-30` — The public acceptance contract is incomplete and retains a false statement.** The table documents `Integer`, `String`, `Function`, `Object`, and `Null`, but omits the newly supported `Boolean` and `Symbol` declared types. Line 28 also says `ObjectType is unchanged`, although plan 0003 Task 4 added `Boolean` and `Symbol` tags. **Revision:** add the `Boolean` and `Symbol` rows to the table and replace the stale `ObjectType is unchanged` sentence with accurate wording, without changing unrelated documentation.

2. **Low — `src/runtime/evaluator/type_check.hpp:39`; `src/runtime/evaluator/type_check.cpp:87` — New Task 5 lines exceed the 100-column style limit.** The documentation line is 139 columns and the Boolean comparison line is 103 columns. **Revision:** wrap only these two lines within 100 columns.

No functional, RAII/lifetime, const-correctness, or runtime API defects were found. `Integer::equals` keeps storage private and performs exact `std::int64_t` comparison. `valuesEqual` verifies the runtime tag before each downcast, compares value classes and identity classes as required, and its suppression names `bugprone-easily-swappable-parameters` with an adjacent symmetry reason as required by `AGENTS.md`.

### Scope and preservation audit

The complete read-only Git diff for every Task 5 scope file was audited. The Task 5 changes implement the declared type checks, script type names, equality helper, developer-authorized `Integer::equals` API, focused Integer boundary test, and required equality/type tests. Earlier Task 2 and Task 4 lines in `type_check.cpp` and `object_test.cpp` were not edited by Task 5 except where Task 5 expressly replaces the type-check placeholders and appends the focused Integer test. No unauthorized deletion, move, rename, reorder, regrouping, re-indentation, reformatting, or documentation-placeholder removal was found.

Recorded decisions DR-1(a), DR-3, DR-4(target), and the 2026-11-22 Task 5 option (b) decision are respected.

### Verification

- `cmake --build build`: passed, warning-free.
- `cmake --build build-asan`: passed, warning-free.
- `ctest --test-dir build --output-on-failure`: 383/383 passed.
- `ctest --test-dir build-asan --output-on-failure`: 383/383 passed; `t_parse_5_long_line` passed, so no rerun was needed.
- `clang-tidy -p build-tidy` with `cert-*,bugprone-*` and warnings as errors on all six Task 5 changed files: passed with no user-code diagnostics.
- Task 5-scoped `git diff --check`: passed.

### Verdict

**NO-GO**


## Task 5 — Type checks and equality — Round 2

- **Date:** 2026-11-24
- **Confirmed plan ID:** `0003`
- **Confirmed plan path:** `plans/0003.md`
- **Task number:** `5` (Type checks and equality, plan T5)
- **Reviewed files:** `src/runtime/evaluator/type_check.hpp`, `src/runtime/evaluator/type_check.cpp`, `tests/unit/runtime/evaluator/type_check_test.cpp`, `src/runtime/object_model/integer.hpp`, `src/runtime/object_model/integer.cpp`, and the Task 5 additions to `tests/unit/runtime/object_model/object_test.cpp`

### Findings

No Critical, High, Medium, or Low findings. No actionable revision items remain.

### Round 1 finding resolution

1. **Resolved (Low):** `type_check.hpp` now documents the `Boolean` and `Symbol` acceptance rows and accurately states that they have their own `ObjectType` tags.
2. **Resolved (Low):** The formerly overlong lines in `type_check.hpp` and `type_check.cpp` are wrapped within the 100-column limit.

### Scope and preservation audit

The complete read-only Git diff for every Task 5 scope file was re-audited. The implementation satisfies the declared type-check, script-type-name, and equality contracts, including exact Integer comparison through the developer-authorized `Integer::equals` API and its focused boundary test. Earlier Task 2 and Task 4 lines remain intact. No unauthorized deletion, move, rename, reorder, regrouping, re-indentation, reformatting, or documentation-placeholder removal was found.

No requirement, API-contract, RAII/lifetime, const-correctness, or guideline regression was found. Recorded decisions DR-1(a), DR-3, DR-4(target), and the 2026-11-22 Task 5 option (b) decision remain respected. The trailing whitespace in `plans/0005.md` is outside plan 0003 and is not a finding.

### Verification

- `cmake --build build`: passed, warning-free.
- `cmake --build build-asan`: passed, warning-free.
- `ctest --test-dir build --output-on-failure`: 383/383 passed.
- `ctest --test-dir build-asan --output-on-failure`: 383/383 passed; `t_parse_5_long_line` passed, so no rerun was needed.
- `clang-tidy -p build-tidy` with `cert-*,bugprone-*` and warnings as errors on all six Task 5 changed files: passed with no user-code diagnostics.
- Task 5-scoped `git diff --check` and 100-column checks: passed.

### Verdict

**GO**


## Task 6 — Validator: scopes and names — Round 1

- **Date:** 2026-11-24
- **Confirmed plan ID:** `0003`
- **Confirmed plan path:** `plans/0003.md`
- **Task number:** `6` (Validator: scopes and names, plan T6)
- **Reviewed files:** `src/analysis/semantic_analyzer/validator.cpp`, `src/analysis/semantic_analyzer/validator.hpp` (untouched), `tests/unit/analysis/semantic_analyzer/validator_test.cpp`

### Findings

1. **Medium — `src/analysis/semantic_analyzer/validator.cpp:125-169`; `tests/unit/analysis/semantic_analyzer/validator_test.cpp:198-213` — Module-level constants and symbols named `main` are not rejected with the required `main` diagnostic.** Plan §4.5 says `main` may be used only as its function declaration name and that a `const` or `symbol` named `main` is a declaration error at the name with `main may be used only as its declaration name`; T6 expressly includes the `main` rules. `collectModuleNames()` applies only duplicate handling, while the tests cover `const main` and `symbol main` only inside a function. Consequently, a module-level `const main` or `symbol main` without a `main` function produces `missing main function` at 1:1, and one paired with a real `main` produces `duplicate declaration`, rather than the required error at the invalid declaration name. **Required fix:** enforce the `main`-name rule for module-level `ConstDecl` and `SymbolDecl` items and add focused tests for both module-level forms that assert class, exact name location, and message.

2. **Low — `src/analysis/semantic_analyzer/validator.cpp:360,426` — Two Task 6 lines exceed the project 100-column limit.** The assignment-target condition is 103 columns and the `LocalConstantState` initializer line is 102 columns. **Required fix:** wrap only these new lines within 100 columns.

3. **Low — `tests/unit/analysis/semantic_analyzer/validator_test.cpp:112-114` — The completed test helper still describes an unimplemented validator path.** The comment says an `InternalError` may come from “a validator path that is not implemented yet,” which was stub-stage wording and is false after Task 6 replaced the DR-1 placeholders. **Required fix:** describe an unexpected/internal validator failure without claiming Task 6 remains unimplemented.

No RAII, lifetime, ownership, API-signature, const-correctness, or Task 7 static-type scope violation was otherwise found. DR-1 option `a` and DR-4 `target` are respected; Task 7 type rules were not assessed as missing behavior.

### Scope and preservation audit

The complete read-only Git diff for all Task 6 files was audited. `validator.hpp` is untouched. The source diff replaces the authorized Task 2 DR-1 validator placeholders and the prior flat function/parameter validation with the Task 6 module/function scope model. The test diff adds documentation, helpers, and Task 6 cases without deleting existing tests. All deleted or replaced original `validator.cpp` lines are required by the planned Program reshaping, generalized namespace/message rules, visibility tracking, assignment targets, and module initializer validation. No unauthorized deletion, move, rename, reorder, regrouping, re-indentation, reformatting, or documentation-placeholder removal was found; there is no preservation High finding.

### Gate results

- `cmake --build build`: passed, warning-free.
- `ctest --test-dir build --output-on-failure`: 383/383 passed.
- `cmake --build build-asan`: passed, warning-free.
- `ctest --test-dir build-asan --output-on-failure`: 383/383 passed; no sanitizer failure.
- `clang-tidy-18 -p build-tidy src/analysis/semantic_analyzer/validator.cpp tests/unit/analysis/semantic_analyzer/validator_test.cpp`: passed with no user-code diagnostics and exit code 0.
- Task 6-scoped `git diff --check`: passed.
- Manual 100-column audit: failed at the two new source lines identified in Finding 2; the other reported overlong test line predates Task 6.

### Verdict

**NO-GO**
