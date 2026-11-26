# Plan 0003 — Implementation Tasks

- **Plan ID:** `0003`
- **Plan:** `plans/0003.md` (revision 5; implementation explicitly authorized by the developer on
  2026-11-21; OQ-15 confirmed: `Symbol` is reserved)
- **Review:** `plans/0003_review.md` (overall verdict Go)
- **Grill record:** `plans/0003_grill.md` (D1–D21)
- **Architecture decisions:** `plans/0003-architecture_decisions.md`
- **Shared implementation notes:** `plans/0003-implementation_notes.md` (created by later stages)
- **Created:** 2026-11-21 by the implementation-architect stage

This file is the handoff contract between the stub creator, implementation generator, reviewer,
and reviser. The tasks follow plan §8, T1–T12, with the same numbers (Task *n* = plan T*n*).
Every task traces to plan sections and §9 test IDs. Nothing here adds behavior or scope beyond
the plan.

## Progress values and rules
- **Field values.**

  | Field | Allowed values | Initial value |
  |---|---|---|
  | `Stub and tests` | `Pending`, `In progress`, `Done` | `Pending` |
  | `Implementation` | `Pending`, `In progress`, `Done` | `Pending` |
  | `Review` | `Pending`, `In progress`, `GO`, `NO-GO` | `Pending` |
  | `Revision` | `Not required`, `Pending`, `Done` | `Not required` |
- **Completed dependency.** A dependency is complete only when its `Stub and tests` and
  `Implementation` fields are `Done` and its `Review` field is `GO`. Do not start a task until
  every task listed in its `Dependencies` field is complete.
- **Who updates what.** Each agent updates only its own progress field for the selected task:
  - the stub creator updates `Stub and tests`;
  - the generator updates `Implementation`;
  - the reviewer updates `Review`;
  - the reviser updates `Revision`.

  No agent changes a task's scope, files, or acceptance criteria during implementation. If a
  task cannot be done within its scope, stop and report to the developer.
- **Developer decisions.** A task marked **Blocked by DR-*n*** must not start (not even its stub
  stage) until the developer resolves that item in "Developer decisions required" below, and the
  resolution is recorded in "Change log".
- **Task numbers.** Task numbers are stable. On a re-run, preserve the recorded progress. Change a
  task only if the approved plan changes or a developer decision is recorded, and record the
  change in "Change log".
- **Order.** Numeric order 1 → 12 is a valid dependency order and is the recommended processing
  order (plan §8: "The tasks are ordered").
- **Gates for every task (plan §8, AGENTS.md Verification).**
  - The build is warning-free under `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`.
  - `clang-tidy` (`cert-*`, `bugprone-*`, warnings as errors) reports no diagnostics.
  - The full suite passes in the Debug and ASan/UBSan (`build-asan/`) builds, including the
    task's tests.
  - Every existing unit and CLI test passes unchanged (AC-2). The only expected mechanical test
    change is the `parser_test.cpp` `asFunctionDecl` adaptation in task 2. Any other affected
    existing test is reported, not rewritten.
  - Unit tests use Catch2 and mirror `src/` under `tests/unit/`. CLI cases use
    `tests/cli/cases/` fixtures registered in `tests/cli/cli_cases.cmake`.
  - Boolean results are checked by unit tests through `tests/unit/support/source_pipeline.*`
    (plan §9 "Observability note").
- **Preservation.** Change only the lines a task requires. Never delete, reorder, rename,
  regroup, re-indent, or reformat existing content (code, docs, TOCs, placeholders) unless the
  plan names that exact change. If such a change seems necessary, stop and ask the developer.
- **Boundaries.**
  - Do not edit `docs/language_rules.md` or any earlier plan (D2). Only task 11 edits `AGENTS.md`.
  - Do not edit any plan file other than this breakdown and the notes file.
  - Do not add dependencies.
  - Use only a direct Bash tool inside the Linux environment. Never use `wsl.exe`, PowerShell,
    `cmd.exe`, Cygwin, or Git for Windows Bash.
  - Do not access the network or run mutating `git` commands.

## Summary
| Task | Title | Dependencies | Blocked by | Tests covered (§9) |
|---|---|---|---|---|
| 1 | Lexer | None | — | T3-LEX-1 to T3-LEX-6 (lexer parts) |
| 2 | AST | None | DR-1 | T3-ARCH-9; T3-NEST-5 (AST destruction part); AC-2 `parser_test.cpp` adaptation |
| 3 | Parser | 1, 2 | — | T3-PARSE-1 to T3-PARSE-10; T3-NEST-1 to T3-NEST-4, T3-NEST-6; T3-ARCH-1; parser parts of T3-LEX-3, T3-LEX-5, T3-LEX-6, T3-SYM-5 |
| 4 | Object model | None | DR-1 | `Boolean`/`Symbol` object unit tests (support T3-EQ, T3-SYM, T3-TYPE) |
| 5 | Type checks and equality | 2, 4 | — | T3-EQ-1 to T3-EQ-4, T3-SYM-6 (`valuesEqual` unit parts); T3-TYPE-1, T3-TYPE-2 (acceptance unit parts) |
| 6 | Validator: scopes and names | 2, 3 | — | T3-DECL-2 to T3-DECL-5; T3-SCOPE-1 to T3-SCOPE-7 (validation parts); T3-MOD-3 to T3-MOD-5; T3-CONST-1, -2, -4, -11 (static parts); T3-ASSIGN-5; T3-SYM-5 (target part) |
| 7 | Validator: static types | 6 | — | T3-DECL-6, -7, -9; T3-CONST-5, -6; T3-ASSIGN-3, -7 (static parts); T3-SYM-3, -4; T3-OP-2; T3-EQ-2 (passes validation); T3-MOD-9; T3-ARCH-7 |
| 8 | Environment | 2, 4 | DR-1 | T3-CONST-9 (second, environment part); parameter binding (DA2) |
| 9 | Errors and evaluator | 2, 3, 4, 5, 6, 7, 8 | — | T3-ARCH-2 to T3-ARCH-6, T3-ARCH-8; runtime parts of T3-DECL, T3-CONST, T3-ASSIGN, T3-SYM, T3-OP, T3-EQ, T3-TYPE, T3-MOD, T3-SCOPE-7, T3-NEST-5 |
| 10 | CLI cases | 1, 2, 3, 4, 5, 6, 7, 8, 9 | — | All `t3_*` CLI cases in §9; AC-1; AC-2 (`t_decl_2_*` unchanged) |
| 11 | Documentation and AGENTS.md | 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 | DR-3 (placement only) | T3-DOC-1, T3-DOC-2; AC-4 |
| 12 | Verification | 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 | — | AC-3; sanitizer runs of T3-ARCH-2, -4, -8, -9; T3-NEST-3 |

## Developer decisions required

### DR-1 (blocking for tasks 2, 4, and 8): per-task build gate vs. task file lists
Plan §8 says "Each task adds its own unit tests and keeps the build warning-free and the whole
suite passing." The current code shows that tasks 2, 4, and 8, limited to their listed files,
cannot meet that gate, because their required changes break files the plan assigns to later
tasks:

| Task | Required change | Breaks | File owned by |
|---|---|---|---|
| 2 | Replace `Program` (`ast.hpp:554`, now `std::vector<std::shared_ptr<const FunctionDecl>>`) with `std::vector<ModuleItem>` | **Compile error:** `validator.cpp:143` and `:165` call `FunctionDecl` members on each item; `module.cpp:30` binds each item to `const std::shared_ptr<const FunctionDecl>&` | T6/T7 (`validator.cpp`), T9 (`module.cpp`) |
| 2 | Add new `NodeKind` enumerators | **`-Wswitch`** (and `clang-tidy` warnings-as-errors) in the two exhaustive, `default`-free switches in `validator.cpp:60` and `:98` | T6/T7 |
| 2 | Add `TypeName::Boolean` and `TypeName::Symbol` | **`-Wswitch`** in `matchesDeclaredType`, `type_check.cpp:14` | T5 |
| 2 | Add node classes `LetDecl`, `ConstDecl`, `SymbolDecl`, `AssignStmt`, `BooleanLiteral`, `UnaryExpr`, `BinaryExpr`, each overriding the pure virtual `ASTNode::evaluate` (`ast.hpp:115`) | **Link error** (missing vtable/`evaluate` definitions) as soon as T3-ARCH-9 or any test constructs them; existing `evaluate` overrides live in `evaluate.cpp` | T9 |
| 4 | Add `ObjectType` tags for `Boolean` and `Symbol` | **`-Wswitch`** in `scriptTypeName`, `type_check.cpp:39` | T5 |
| 8 | `Environment::assign` throws `RuntimeError(ConstantReassignment)` (§4.14.3), tested by T3-CONST-9's environment part | **Compile error:** `RuntimeErrorKind::ConstantReassignment` is added to `src/common/errors.hpp` only in T9 | T9 |

The plan does not say how to reconcile this, and this stage does not choose. Possible
resolutions, for the developer to pick or replace:
- (a) Allow each listed task a minimal, mechanical, compile-preserving change in the named
  downstream files (for example: iterate `Program` through `asFunctionDecl`; add explicit `case`
  labels that throw `InternalError`; add `evaluate` definitions for new nodes that throw
  `InternalError`; add the four `RuntimeErrorKind` values in task 8), which the owning later task
  then completes. This extends the listed file scopes of tasks 2, 4, and 8.
- (b) Move the conflicting pieces between tasks (for example, move the four `RuntimeErrorKind`
  values from T9 into T8 or earlier; fold T5's `type_check.cpp` switch cases into T2/T4), or
  merge tasks.
- (c) Waive the per-task "warning-free, whole suite passing" gate between tasks 2 and 9, and
  enforce it from task 9 on. This conflicts with plan §8 and the AGENTS.md Verification section.

**Resolution (developer, 2026-11-21): "a".** Tasks 2, 4, and 8 may make minimal, compile-only
edits in later tasks' files: placeholder `case` labels, `evaluate` bodies that throw
`InternalError`, iteration of `Program` through `asFunctionDecl`, and the four `RuntimeErrorKind`
values (added in task 8). The owning later tasks complete the real work.

Task 1 is not affected and may proceed. Tasks 3, 5, 6, 7, and 9–12 are blocked transitively
through their dependencies.

### DR-2 (non-blocking; task 4): `CMakeLists.txt`
Plan T4 lists `CMakeLists.txt` with "Add the new source files to the build." `CMakeLists.txt`
already collects `src/*.cpp` with `GLOB_RECURSE ... CONFIGURE_DEPENDS`, so the new
`boolean.cpp` and `symbol.cpp` are built with no edit. Task 4 therefore expects **no**
`CMakeLists.txt` change. If an implementer finds a build change is truly needed, stop and ask.

### DR-3 (non-blocking until task 11): documentation placement
- Plan T11 adds `reference/variables-and-constants.md` and `reference/operators.md` "linked from
  `docs/end-users/src/index.md`" but does not say where in the table of contents. The
  "Language Reference" list has unlinked placeholders (`Keywords`, `Statements`, …). Linking or
  rewording an existing placeholder line, or moving any line, is not authorized; only new lines
  may be added. The developer should confirm the insertion point.
- `docs/sitemap.xml` lists every current reference page but is not named by the plan. Task 11
  does not edit it unless the developer authorizes it.

**Resolution (developer, 2026-11-21):** "Add them as new lines at the end of the Language
Reference list. Leave the existing placeholders alone." Task 11 may also update
`docs/sitemap.xml` for the two new pages.

### DR-4 (non-blocking; tasks 6, 7, 10): T3-CONST-5 second case location
T3-CONST-5 says `n = null` on an unassigned nullable constant is a declaration error at `null`,
"and so is `n = null` after an assignment." For the second case the constant is statically
*assigned*, so §5.3 also gives `cannot assign to constant` at the target `n`, and §4.10 says the
first error **in source order** is reported (`n` precedes `null`). The plan does not state which
check wins. Until the developer answers, tests for the second case assert only class
`declaration` and exit 65, not the location or message. The first case (at `null`,
`constant value must not be null`) is unambiguous.

**Resolution (developer, 2026-11-21): "target".** The second case is reported at the target `n`
with `cannot assign to constant` (class `declaration`, exit 65). Tests assert that location and
message.

## Task 1: Lexer
- **Plan references:**
  - §8 T1
  - §3 C1–C4
  - §4.2.1, §4.2.2, §4.2.3, §4.2.4
  - §5.1
  - §6.1 (malformed operator sequences via maximal munch)
  - Tests: T3-LEX-1 to T3-LEX-6 (lexer-level parts)
- **Dependencies:** None
- **Scope:**
  - **Files:** `src/frontend/lexer/token.hpp`, `src/frontend/lexer/lexer.cpp`,
    `tests/unit/frontend/lexer/lexer_test.cpp`.
  - Identifier rule check with the lexical error `invalid identifier` at the word's first byte.
  - 7 new reserved words, each with its own token kind (35 total).
  - Tokens `Bang`, `Assign`, `EqualEqual`, `BangEqual` with maximal munch.
  - Negative literals also after `=`, `==`, `!=`, `!`.
- **Implementation objective:**
  - A maximal word `[A-Za-z_][A-Za-z0-9_]*` that is not reserved and does not match
    `_*[A-Za-z][A-Za-z0-9_]*` fails with `invalid identifier` at its first byte. `1count` keeps
    its existing error.
  - `let`, `const`, `true`, `false`, `symbol`, `Boolean`, `Symbol` are never emitted as
    identifiers.
  - `==`/`!=` are single tokens when adjacent: `===` → `==`,`=`; `!==` → `!=`,`=`; `= =` → `=`,`=`;
    `! =` → `!`,`=`. Columns are in bytes.
  - All plan 0002 negative-literal rules (`-0`, range, following identifier character) apply in
    the new contexts.
- **Stub and unit-test work:**
  - Stubs: the new token kinds in `token.hpp`; no new functions are named by the plan.
  - Unit tests in `lexer_test.cpp`:
    - T3-LEX-1: `count`, `_count`, `__count`, `count_2`, `a1b2`, `A`, `_a` lex as identifiers.
    - T3-LEX-2: `_`, `__`, `_1count`, `__9a` give `invalid identifier` at the first byte.
    - T3-LEX-3: each new reserved word lexes as its own kind; `Let`, `TRUE`, `symbols`,
      `boolean`, `letx`, `_true` lex as identifiers.
    - T3-LEX-4: token streams and byte columns for `a==b`, `a == b`, `a!=b`, `!a`, `a=b`, `===`,
      `!==`, `= =`, `! =`, and `!` at end of file.
    - T3-LEX-5: negative literals after `=`, `==`, `!=`, `!` (including
      `-9223372036854775808`); `x = -0` and `a == -007` are lexical errors at the `-`;
      `x = - 1` lexes `-` as `Minus` (its syntax error is task 3).
    - T3-LEX-6: `;` continuation after `=`, after `==`, and between `let x:` and the type
      produces the expected token stream.
- **Acceptance criteria:**
  - All listed lexer tests pass; every existing lexer test passes unchanged.
  - Every-task gates pass.
- **Stub and tests:** Done
- **Implementation:** Done
- **Review:** GO
- **Revision:** Done

## Task 2: AST
**Blocked by DR-1.**
- **Plan references:**
  - §8 T2
  - §3 C15
  - §4.3 (`TypeName::Boolean`, `TypeName::Symbol`)
  - §4.12 (one unary and one binary node with operator enumerators)
  - §4.14.2 (Representation, Ownership, Name storage, Destruction)
  - §6.1 (iterative destruction), §6.3 (source-buffer lifetime)
  - AC-2 (mechanical `parser_test.cpp` adaptation)
  - Tests: T3-ARCH-9; T3-NEST-5 (AST destruction part)
- **Dependencies:** None
- **Scope:**
  - **Files:** `src/frontend/ast/ast.hpp`, `src/frontend/ast/ast.cpp`,
    `src/frontend/parser/parser.hpp` (comment only), `src/runtime/evaluator/module.hpp`
    (comments only), `tests/unit/frontend/ast/ast_test.cpp`,
    `tests/unit/frontend/parser/parser_test.cpp`, `tests/unit/support/ast_builders.hpp`,
    `tests/unit/support/ast_builders.cpp`, `tests/unit/support/source_pipeline.hpp` (comment
    only). Any further file depends on the DR-1 resolution.
  - Add `TypeName::Boolean` and `TypeName::Symbol`.
  - Add `LetDecl`, `ConstDecl`, `SymbolDecl`, `AssignStmt`, `BooleanLiteral`, `UnaryExpr`
    (operator enumerator `Not`), `BinaryExpr` (operator enumerators `Equal`, `NotEqual`) with
    `NodeKind` values. `LetDecl` and `ConstDecl` expose `type()` (`DeclaredType`) and an optional
    initializer.
  - Replace the `Program` alias with `ModuleItem`, `Program`, and `asFunctionDecl` exactly as in
    §4.14.2.
  - Owned names: `name_` of `Identifier`, `Param`, `FunctionDecl` becomes `std::string`; new
    named nodes own `std::string` names from the start.
  - Comment updates named by the plan: the `ast.hpp` file comment and constructor/`name()`
    comments ("views into the source buffer", "no node may outlive the Module"); `FunctionDecl`
    ownership comment (module items); `parser.hpp` ownership comment; `module.hpp` class and
    `release()` comments; `ast_builders.hpp` (names no longer need to outlive the node);
    `source_pipeline.hpp` (source must outlive the tokens, not the AST).
- **Implementation objective:**
  - Every new node implements `detachChildren`. `LetDecl`, `ConstDecl`, `AssignStmt`,
    `UnaryExpr`, `BinaryExpr` define a `noexcept` destructor that moves children onto a local
    worklist and runs `destroyWorklist`, following `ExprStmt::~ExprStmt`.
  - Constructors keep `std::string_view name` parameters and copy them; `Identifier` and `Param`
    constructors drop `noexcept` and document `std::bad_alloc`. `name()` keeps
    `[[nodiscard]] std::string_view name() const noexcept`.
  - `asFunctionDecl` uses `kind()` and `std::static_pointer_cast`, shares the item's control
    block, returns null for non-`FunctionDecl` items, and throws `InternalError` on a null item.
  - `parser_test.cpp`: only `program[i]->name()`, `->params()`, `->returnType()`, `->body()`,
    `->location()` accesses and `*program[i]` dereferences are rewritten through
    `asFunctionDecl(program[i])`; inputs and assertions are unchanged.
- **Stub and unit-test work:**
  - Stubs: the new `TypeName` enumerators, `NodeKind` values, node class declarations with
    their constructors/accessors, `ModuleItem`, `Program`, and `asFunctionDecl` declarations, with
    stub bodies as the DR-1 resolution permits.
  - Unit tests in `ast_test.cpp`:
    - T3-ARCH-9: build `Identifier`, `Param`, `FunctionDecl`, `LetDecl`, `ConstDecl`,
      `SymbolDecl`, `AssignStmt` from a heap `std::string` name longer than 22 bytes; destroy or
      overwrite the string; each `name()` still returns the original text.
    - `asFunctionDecl`: null for non-function items, shared ownership for a `FunctionDecl` item,
      `InternalError` on a null item.
    - T3-NEST-5 (destruction part): destroying an AST with 10,000 `LetDecl`s and 10,000
      `AssignStmt`s, and an expression at maximum nesting built from `UnaryExpr`/`BinaryExpr`,
      causes no per-node recursion.
  - Mechanical `parser_test.cpp` adaptation (AC-2).
- **Acceptance criteria:**
  - No AST node holds a `std::string_view` into a source buffer.
  - All listed tests pass; all existing tests pass with only the AC-2 mechanical change.
  - Every-task gates pass (subject to DR-1).
- **Stub and tests:** Done
- **Implementation:** Done
- **Review:** GO
- **Revision:** Not required

## Task 3: Parser
- **Plan references:**
  - §8 T3
  - §3 C7–C10
  - §4.4, §4.11.5 (`echo(true)`/`echo(false)`), §4.12
  - §4.14.2 (module items created with `std::make_shared<const …>` in source order)
  - §5.2
  - §6.1 (nesting counting; iterative equality chains)
  - Tests: T3-PARSE-1 to T3-PARSE-10; T3-NEST-1 to T3-NEST-4, T3-NEST-6; T3-ARCH-1; parser parts
    of T3-LEX-1, T3-LEX-3, T3-LEX-5 (`x = - 1`), T3-LEX-6, T3-SYM-5 (`symbol x = 1`)
- **Dependencies:** 1, 2
- **Scope:**
  - **Files:** `src/frontend/parser/parser.hpp`, `src/frontend/parser/parser.cpp`,
    `tests/unit/frontend/parser/parser_test.cpp`.
  - Module items (`function`, `const`, `symbol`; `let` is an error at `let`), statements
    (`let`, `const`, `symbol`, assignment, existing ones), and the expression levels
    `expression` → `equality` → `unary` → `primary`.
  - `typeName` gains `Boolean`, `Symbol`, `?Boolean`, `?Symbol`.
- **Implementation objective:**
  - Module items are created with `std::make_shared<const FunctionDecl|ConstDecl|SymbolDecl>`
    and appended to `Program` in source order.
  - A statement starting with an identifier is a call if the next token is `(`, an assignment if
    it is `=`, and otherwise a syntax error at that token. `=` never appears inside an
    expression.
  - Equality chains are left-associative and parsed iteratively; each operator adds one nesting
    level; level 257 is a syntax error at the operator that would open it. A function body is
    level 1; a module-level initializer starts at level 0.
  - `!` takes a `primary`; `!` followed by `!` is `repeated ! is not allowed` at the second `!`.
  - `echo(true)` and `echo(false)` are syntax errors at the argument; `typeof(true)` is accepted.
  - Every §5.2 condition at its listed location.
- **Stub and unit-test work:**
  - Stubs: parse functions per the chosen §4.12 structure (architecture decisions §6).
  - Unit tests in `parser_test.cpp`: T3-PARSE-1 to T3-PARSE-10; T3-NEST-1, -2, -4, -6 at the
    256/257 boundaries; T3-NEST-3 (100,000 `!` tokens → `repeated ! is not allowed` at the
    second `!`; 100,000-operator `==` chain → nesting error; no per-operator recursion, checked
    with a 256 KiB thread stack or the sanitizer run); T3-ARCH-1 (four items, kinds `ConstDecl`,
    `FunctionDecl`, `SymbolDecl`, `FunctionDecl`; `asFunctionDecl` results); reserved-word-as-name
    syntax errors (T3-LEX-3) and accepted names (T3-LEX-1); `x = - 1` error at `-` (T3-LEX-5);
    `;` continuation (T3-LEX-6); `symbol x = 1` error at `=` (T3-SYM-5).
- **Acceptance criteria:**
  - All listed tests pass; existing parser tests pass (with task 2's mechanical change only).
  - Every-task gates pass.
- **Stub and tests:** Done
- **Implementation:** Done
- **Review:** GO
- **Revision:** Done

## Task 4: Object model
**Blocked by DR-1.**
- **Plan references:**
  - §8 T4
  - §3 C5, C6
  - §4.3 (runtime representation; no process-global or `static` `Boolean`)
  - §4.9 (symbol identity), §4.14.2 (`Symbol` holds no AST pointer or source view)
  - §6.3
  - Tests: object unit tests supporting T3-EQ, T3-SYM, T3-TYPE
- **Dependencies:** None
- **Scope:**
  - **Files:** `src/runtime/object_model/boolean.hpp`, `src/runtime/object_model/boolean.cpp`,
    `src/runtime/object_model/symbol.hpp`, `src/runtime/object_model/symbol.cpp`,
    `src/runtime/object_model/object.hpp`, `src/runtime/evaluator/execution_context.hpp`,
    `src/runtime/evaluator/execution_context.cpp`, `CMakeLists.txt` (see DR-2: no edit
    expected), `tests/unit/runtime/object_model/object_test.cpp`.
  - New `ObjectType` tags for `Boolean` and `Symbol`.
- **Implementation objective:**
  - `Boolean` (immutable, value `true`/`false`) and `Symbol` (pure identity) extend `Object` and
    return their new `ObjectType` tags.
  - Canonical `true`/`false` instances, if used, are owned by `ExecutionContext` like the `null`
    instance; no `static` or global instance.
- **Stub and unit-test work:**
  - Stubs: `boolean.{hpp,cpp}`, `symbol.{hpp,cpp}`, the `ObjectType` tags, and any
    `ExecutionContext` accessors for canonical Booleans if the implementation uses them,
    following the `Integer`/`NullObject` pattern (architecture decisions §6).
  - Unit tests in `object_test.cpp`: `objectType()` of each new class; `Boolean` value;
    distinct `Symbol` objects are distinct instances; non-copyable/non-movable like `Object`.
- **Acceptance criteria:**
  - No mutable global or static state added.
  - All listed tests pass; every-task gates pass (subject to DR-1).
- **Stub and tests:** Done
- **Implementation:** Done
- **Review:** GO
- **Revision:** Done

## Task 5: Type checks and equality
- **Plan references:**
  - §8 T5
  - §3 C5, C6
  - §4.3 (acceptance table; `typeof` results)
  - §4.11.2, §4.11.3 (D11 equality; D21)
  - §6.2 (exact `std::int64_t` comparison), §6.3 (identity under owning copies; linear string
    compare)
  - Tests: T3-EQ-1 to T3-EQ-4 and T3-SYM-6 (`valuesEqual` unit parts); T3-TYPE-1, T3-TYPE-2
    (acceptance unit parts)
- **Dependencies:** 2, 4
- **Scope:**
  - **Files:** `src/runtime/evaluator/type_check.hpp`, `src/runtime/evaluator/type_check.cpp`,
    `tests/unit/runtime/evaluator/type_check_test.cpp`.
- **Implementation objective:**
  - `matchesDeclaredType` accepts `Boolean`/`Symbol` for `Boolean`/`Symbol`, and `Object` accepts
    every non-`null` value; `?T` adds `null`.
  - `scriptTypeName` returns `"Boolean"` and `"Symbol"`.
  - `valuesEqual(const Object&, const Object&) noexcept`: different runtime classes → `false`;
    `Integer` exact value; `String` length then bytes; `Boolean` value; `null == null`;
    `Function`, `Symbol`, and other reference-like values by identity.
- **Stub and unit-test work:**
  - Stub: the `valuesEqual` declaration and a stub body.
  - Unit tests in `type_check_test.cpp`: the acceptance table for `Boolean`, `?Boolean`, `Symbol`,
    `?Symbol`, `Object`, `?Object`; `scriptTypeName` for the new classes; `valuesEqual` for every
    T3-EQ-1 pair (including `INT64_MIN == INT64_MIN` and `INT64_MAX != -1`), every T3-EQ-2
    mixed-class pair (`false`), identity for `Function` and `Symbol` (T3-EQ-3, T3-SYM-6), and
    `Boolean` chains' element comparisons (T3-EQ-4).
- **Acceptance criteria:**
  - All listed tests pass; existing type-check tests pass unchanged.
  - Every-task gates pass.
- **Stub and tests:** Done
- **Implementation:** Done
- **Review:** GO
- **Revision:** Done

## Task 6: Validator: scopes and names
- **Plan references:**
  - §8 T6
  - §3 C7, C8, C12, C13
  - §4.5 (scopes, D19, DA3, check order, report timing, name resolution, visibility, `main`)
  - §4.5.1 (DA1 b initializer contents; DA1 d no writes from functions)
  - §4.6 and §4.7 (initializer required; nullable-constant *assigned* tracking)
  - §4.8 (assignment-target rules; assignable parameters, DA2)
  - §4.9 (assignment to a symbol, D14)
  - §4.14.2 (item iteration with a `kind()` `switch`)
  - §5.3 rows: `initializer required`, `duplicate declaration`,
    `name must not match a module-level declaration`, `used before declaration`,
    `not allowed in a module-level initializer`, `undefined identifier`, `main` message,
    `cannot assign to this name`, `cannot assign to constant`
  - §5.4 final paragraph (message rename)
  - Tests: T3-DECL-2 to T3-DECL-5; T3-SCOPE-1 to T3-SCOPE-7 (validation parts); T3-MOD-3,
    T3-MOD-4, T3-MOD-5; T3-CONST-1, T3-CONST-2 (second write), T3-CONST-4, T3-CONST-11 (static
    parts); T3-ASSIGN-5; T3-SYM-5 (assignment-target part)
- **Dependencies:** 2, 3
- **Scope:**
  - **Files:** `src/analysis/semantic_analyzer/validator.hpp`,
    `src/analysis/semantic_analyzer/validator.cpp`,
    `tests/unit/analysis/semantic_analyzer/validator_test.cpp`.
  - Keep and generalize the plan 0001 D15 check (`validator.cpp`, around lines 189–192);
    replace the flat `parameters`/`functionNames` sets in `FunctionContext` with module-level and
    function-scope name tables. `validateProgram` keeps its signature and `void` return (AD1).
- **Implementation objective:**
  - Iterate `Program` items with a `kind()` `switch`; a null item or another kind is an
    `InternalError`.
  - First pass: collect every module-level function, constant, and symbol name
    (order-independent); module-level duplicates are `duplicate declaration` at the second name.
  - Per function, before checking its body: check each parameter and body `let`/`const`/`symbol`
    name in source order against the `main` rule, then D19 duplicates, then the module-level
    set (`name must not match a module-level declaration` at the function-scope name).
  - Visibility: a local name is visible from the statement after its declaration; earlier uses
    and assignments (including in its own initializer) are `used before declaration`.
  - Assignment targets: `let`, parameter, or a function-scope constant that may still be
    written; function names and symbols → `cannot assign to this name`; non-nullable constants,
    statically *assigned* nullable constants, and module-level constants →
    `cannot assign to constant`; `main` → existing `main` message; undefined → `undefined
    identifier`.
  - Module-level initializers: only allowed constructs; later/own names → `used before
    declaration`; calls, pseudo-calls, function names as values →
    `not allowed in a module-level initializer`.
  - Non-nullable `let`/`const` without an initializer → `initializer required` at the keyword.
  - Functions and module-level initializers are checked in source order; every function is
    checked, including uncalled ones.
- **Stub and unit-test work:**
  - No new public interface (internal helpers only).
  - Unit tests in `validator_test.cpp` via `source_pipeline`: every listed test ID, including
    every kind pair in T3-SCOPE-2, order independence (T3-SCOPE-3), report order (T3-SCOPE-4),
    check order (T3-SCOPE-5), and uncalled-function cases. Each asserts class, location, and the
    §5.3 message. For T3-CONST-5's second case see DR-4 (task 7).
- **Acceptance criteria:**
  - All listed tests pass; existing validator tests pass unchanged (no existing unit test asserts
    the replaced message, AC-2).
  - Every-task gates pass.
- **Stub and tests:** Done
- **Implementation:** Done
- **Review:** GO
- **Revision:** Done

## Task 7: Validator: static types
- **Plan references:**
  - §8 T7
  - §3 C11
  - §4.7 rule 5 (D15), §4.9 (bare-symbol placement, D17, D18), §4.10 (static types, rules 1–5,
    DA6, DA7), §4.11.1 (`!` operand), §4.11.5 (`typeof` of a bare symbol)
  - §4.14.1 (AD1: no artifact, no AST writes; update the `validateProgram` documentation comment)
  - §5.3 rows: `type mismatch`, `nullable value for non-nullable target`,
    `constant value must not be null`, `operand of ! must be Boolean`, `symbol not allowed here`
  - Tests: T3-DECL-6, T3-DECL-7, T3-DECL-9 (static parts); T3-CONST-5, T3-CONST-6; T3-ASSIGN-3,
    T3-ASSIGN-7 (static parts); T3-SYM-3, T3-SYM-4; T3-OP-2; T3-EQ-2 (passes validation);
    T3-MOD-9; T3-ARCH-7; T3-LEX-5 (`!-1` declaration error)
- **Dependencies:** 6
- **Scope:**
  - **Files:** same as task 6.
- **Implementation objective:**
  - Compute static types per the §4.10 table inside `validator.cpp` (including *dynamic* for
    calls through non-function bindings and *symbol name* for bare symbols).
  - Apply rules 1–5 at `let`/`const` initializers (function- and module-level), assignment
    right-hand sides, `!` operands (`E` = `Boolean`), and bare-symbol placement. Call arguments,
    return values, `echo` arguments, and callees stay runtime checks (DA7).
  - The validator writes nothing into the AST and returns nothing (AD1).
- **Stub and unit-test work:**
  - No new public interface.
  - Unit tests in `validator_test.cpp`: every listed test ID with class, location, and message;
    T3-ARCH-7 as a `static_assert` that `validateProgram` is `void (*)(const Program&)`.
    T3-CONST-5's second case asserts only the class (DR-4).
- **Acceptance criteria:**
  - All listed tests pass; existing validator tests pass unchanged.
  - Every-task gates pass.
- **Stub and tests:** Done
- **Implementation:** Done
- **Review:** GO
- **Revision:** Done

## Task 8: Environment
**Blocked by DR-1.**
- **Plan references:**
  - §8 T8
  - §3 C13
  - §4.8 step 4, §4.13, §4.14.3
  - §6.3 (owning copies; no reference into the table escapes)
  - Tests: T3-CONST-9 (second, environment part); DA2 parameter binding
- **Dependencies:** 2, 4
- **Scope:**
  - **Files:** `src/runtime/environment/environment.hpp`,
    `src/runtime/environment/environment.cpp`, `src/runtime/evaluator/call.cpp`,
    `tests/unit/runtime/environment/environment_test.cpp`.
- **Implementation objective:**
  - `BindingKind`, `BindingInfo`, the binding record, the four-argument `define`, `bindingInfo`,
    and `assign`, exactly as in §4.14.3. The two-argument `define` keeps its signature and records
    `BindingKind::Function`.
  - `assign` implements §4.8 step 4, including the store-time constant re-check
    (`ConstantReassignment`), with the strong guarantee.
  - `callFunction` binds parameters through the four-argument `define` with
    `BindingKind::Parameter` and the parameter's declared type (DA2).
- **Stub and unit-test work:**
  - Stubs: the declarations above with stub bodies.
  - Unit tests in `environment_test.cpp`: `bindingInfo` for each kind and through enclosing
    scopes; `assign` success; `assign` to `Function`/`Symbol`/unbound → `InternalError`;
    T3-CONST-9 second part (step-1 check on an unassigned constant, direct store, then step-4
    store raises `ConstantReassignment` and the value is unchanged); parameters report
    `Parameter` with their declared type.
- **Acceptance criteria:**
  - All listed tests pass; existing environment and call tests pass unchanged.
  - Every-task gates pass (subject to DR-1: `ConstantReassignment` is added by task 9 per the
    plan).
- **Stub and tests:** In progress
- **Implementation:** Pending
- **Review:** Pending
- **Revision:** Not required

## Task 9: Errors and evaluator
- **Plan references:**
  - §8 T9
  - §3 C9, C13, C14
  - §4.5.1 (module initialization, runtime failures), §4.6–§4.9 (execution), §4.11 (operators,
    D5 order), §4.13
  - §4.14.1 (runtime-check table), §4.14.2 (load sequence, teardown), §4.14.3 (`AssignStmt`)
  - §5.4 (four new `RuntimeErrorKind` values)
  - §6.1 (depth guards), §6.3
  - Tests: T3-ARCH-2, T3-ARCH-3, T3-ARCH-4, T3-ARCH-5, T3-ARCH-6, T3-ARCH-8; runtime parts of
    T3-DECL-1, -7, -8, -9; T3-CONST-2, -3, -7, -8, -9 (first part), -10, -11; T3-ASSIGN-1, -2,
    -4, -6, -7; T3-SYM-1, -2, -3, -6, -7, -8; T3-OP-1, T3-OP-3; T3-EQ-1 to T3-EQ-6; T3-TYPE-1 to
    T3-TYPE-3; T3-MOD-1, -2, -6, -7, -8; T3-SCOPE-7; T3-NEST-5 (validates and runs)
- **Dependencies:** 2, 3, 4, 5, 6, 7, 8
- **Scope:**
  - **Files:** `src/common/errors.hpp`, `src/common/errors.cpp`,
    `src/runtime/evaluator/evaluate.cpp`, `src/runtime/evaluator/module.hpp`,
    `src/runtime/evaluator/module.cpp`, `tests/unit/runtime/evaluator/evaluate_test.cpp`,
    `tests/unit/runtime/evaluator/module_test.cpp`, `tests/unit/common/errors_test.cpp`.
  - `Module::release`, `runMain`, and `src/cli/cli.cpp` keep their current code.
- **Implementation objective:**
  - Add `AssignmentType`, `OperandType`, `ConstantReassignment`, `ConstantNullWrite`.
  - Evaluate `LetDecl`, `ConstDecl`, `SymbolDecl`, `AssignStmt`, `BooleanLiteral`, `UnaryExpr`,
    `BinaryExpr` with explicit D5 sequencing into named locals and the existing depth guards.
  - Perform every §4.14.1 runtime check unconditionally; `AssignStmt::evaluate` follows §4.14.3
    steps 1–4.
  - A function-scope `symbol` creates a new `Symbol` each time it executes (DA4).
  - `Module::load`: register all functions, then run module-level `ConstDecl`/`SymbolDecl` items
    once in source order; non-`Normal` completion → `InternalError`; document `RuntimeError` and
    update the `Module` class comment.
- **Stub and unit-test work:**
  - Stubs: the new `RuntimeErrorKind` values; `evaluate` overrides for the new nodes (if not
    already present per DR-1).
  - Unit tests:
    - `errors_test.cpp`: the four new kinds.
    - `evaluate_test.cpp`: T3-ARCH-6 (hand-built ASTs without validation); T3-CONST-9 first part
      (no `R` in captured output); T3-ASSIGN-4 (old value kept); Boolean results of T3-EQ-1 to
      T3-EQ-4, T3-SYM-1, T3-SYM-6, T3-SYM-7, T3-SYM-8, T3-OP-1, T3-MOD-1, T3-MOD-7, T3-SCOPE-7 via
      `source_pipeline`; runtime errors of T3-DECL-8, T3-DECL-9 (dynamic), T3-CONST-7, T3-CONST-8,
      T3-OP-3, T3-ASSIGN-7; T3-NEST-5 run.
    - `module_test.cpp`: T3-ARCH-2, T3-ARCH-3, T3-ARCH-4, T3-ARCH-5, T3-ARCH-8 (retained
      `Function` after `module.reset()` and after `release()`).
- **Acceptance criteria:**
  - All listed tests pass under Debug and ASan/UBSan (no heap-use-after-free, no leak).
  - Existing evaluator, module, and error tests pass unchanged.
  - Every-task gates pass.
- **Stub and tests:** Pending
- **Implementation:** Pending
- **Review:** Pending
- **Revision:** Not required

## Task 10: CLI cases
- **Plan references:**
  - §8 T10
  - §9.1–§9.11 (every `t3_*` CLI case), AC-1, AC-2
  - §3 C12 (unchanged `t_decl_2_*` expectations)
- **Dependencies:** 1, 2, 3, 4, 5, 6, 7, 8, 9
- **Scope:**
  - **Files:** new `tests/cli/cases/t3_*.phil` fixtures; `tests/cli/cli_cases.cmake`
    (new registrations only).
  - Leave `t_decl_2_parameter_named_like_function` (`STATUS 65 KIND located CLASS declaration
    LINE 3 COLUMN 12`), `t_decl_2_parameter_named_main`, and their fixtures unchanged.
- **Implementation objective:**
  - Each §5 condition has at least one CLI or unit test (AC-1). CLI cases check class, line,
    column, exit status, stdout, and message text only where the plan gives it.
  - Includes the CLI-observable cases: T3-LEX-1 to -3, -5, -6; T3-PARSE-*; T3-NEST-1, -2, -4,
    -6; T3-DECL-*; T3-CONST-*; T3-ASSIGN-1, -3, -5, -6, -7; T3-SYM-1 to -5; T3-OP-2, -3;
    T3-EQ-5, -6; T3-TYPE-*; T3-SCOPE-1 to -6; T3-MOD-2 to -6, -8, -9.
- **Stub and unit-test work:**
  - No C++ stubs. Fixtures and registrations are the tests. T3-CONST-5's second case asserts
    only class and exit status until DR-4 is answered.
- **Acceptance criteria:**
  - All new and existing CLI cases pass in Debug and ASan builds.
  - Every-task gates pass.
- **Stub and tests:** Pending
- **Implementation:** Pending
- **Review:** Pending
- **Revision:** Not required

## Task 11: Documentation and AGENTS.md
**Placement of new TOC links blocked by DR-3; the rest may proceed.**
- **Plan references:**
  - §8 T11
  - §4.1 (D1, D2), §7 (AGENTS.md impact)
  - §9.13 (T3-DOC-1, T3-DOC-2), AC-4
- **Dependencies:** 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
- **Scope:**
  - **Files:** `docs/end-users/src/reference/source-files.md`,
    `docs/end-users/src/reference/types.md`, `docs/end-users/src/reference/functions.md`,
    `docs/end-users/src/reference/errors-and-limits.md`, new
    `docs/end-users/src/reference/variables-and-constants.md`, new
    `docs/end-users/src/reference/operators.md`, `docs/end-users/src/index.md` (new link lines
    only), `AGENTS.md`.
  - Not edited: `docs/language_rules.md`, earlier plans, `docs/sitemap.xml` (DR-3).
- **Implementation objective:**
  - `source-files.md`: names, 35 reserved words, punctuation, negative-literal contexts.
  - `types.md`: `Boolean`, `Symbol`, `Object`.
  - `functions.md`: statement table, `typeof` results, assignable parameters, program contents
    (module-level constants and symbols).
  - `errors-and-limits.md`: phases, new conditions, module initialization, nesting.
  - `variables-and-constants.md`: `let`, `const`, `symbol`, assignment, module-level
    declarations and initialization order, scope, name resolution, no reuse of module-level
    names.
  - `operators.md`: `!`, `==`, `!=`, evaluation order, precedence, no `!!`.
  - `AGENTS.md`: Document Precedence (D1, D2); Open Decisions evaluation-order row reads
    "Resolved by plan 0003 (D5)"; Data Type Blueprint adds `Boolean` and `Symbol` and D11
    equality; recursion-limit note that expression operators count toward the parser nesting
    limit. Only the §7 items change.
- **Stub and unit-test work:**
  - No stubs or unit tests. The review checklist T3-DOC-1 and T3-DOC-2 is the test.
- **Acceptance criteria:**
  - T3-DOC-1, T3-DOC-2, AC-4.
  - No existing line is deleted, reordered, regrouped, or reformatted beyond the named updates.
- **Stub and tests:** Pending
- **Implementation:** Pending
- **Review:** Pending
- **Revision:** Not required

## Task 12: Verification
- **Plan references:**
  - §8 T12, AC-3
  - §6 (sanitizer coverage; T3-NEST-3 no recursion)
  - Tests: T3-ARCH-2, T3-ARCH-4, T3-ARCH-8, T3-ARCH-9 under ASan/UBSan
- **Dependencies:** 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
- **Scope:**
  - No file changes (commands only), using the existing `build/`, `build-asan/`, and
    `build-tidy/` (or `build-release/`) directories.
- **Implementation objective:**
  - Warning-free build with the AGENTS.md flags.
  - Full suite under ASan and UBSan in `build-asan/`, including T3-ARCH-2, -4, -8, -9, with no
    heap-use-after-free and no leak.
  - `clang-tidy` with `cert-*` and `bugprone-*` clean.
- **Stub and unit-test work:**
  - None.
- **Acceptance criteria:**
  - AC-3. Results recorded in `plans/0003-implementation_notes.md`.
- **Stub and tests:** Pending
- **Implementation:** Pending
- **Review:** Pending
- **Revision:** Not required

## Traceability: plan §9 test IDs to tasks
| Test IDs | Tasks |
|---|---|
| T3-LEX-1, T3-LEX-3, T3-LEX-6 | 1, 3, 10 |
| T3-LEX-2, T3-LEX-4 | 1 (T3-LEX-2 also 10) |
| T3-LEX-5 | 1, 3, 7, 10 |
| T3-PARSE-1 to T3-PARSE-10 | 3, 10 |
| T3-NEST-1, -2, -4, -6 | 3, 10 |
| T3-NEST-3 | 3, 12 |
| T3-NEST-5 | 2, 9 |
| T3-DECL-1 | 9, 10 |
| T3-DECL-2 to T3-DECL-5 | 6, 10 |
| T3-DECL-6 | 7, 10 |
| T3-DECL-7, T3-DECL-8, T3-DECL-9 | 7, 9, 10 |
| T3-CONST-1, -4 | 6, 10 |
| T3-CONST-2, -3, -11 | 6, 9, 10 |
| T3-CONST-5, -6 | 7, 10 |
| T3-CONST-7, -8, -10 | 9, 10 |
| T3-CONST-9 | 8, 9 |
| T3-ASSIGN-1, -2, -4, -6 | 9, 10 |
| T3-ASSIGN-3 | 7, 10 |
| T3-ASSIGN-5 | 6, 10 |
| T3-ASSIGN-7 | 6, 7, 8, 9, 10 |
| T3-SYM-1, -2, -3 | 7, 9, 10 |
| T3-SYM-4 | 7, 10 |
| T3-SYM-5 | 3, 6, 10 |
| T3-SYM-6 | 5, 9 |
| T3-SYM-7, T3-SYM-8 | 9 |
| T3-OP-1 | 9 |
| T3-OP-2 | 7, 10 |
| T3-OP-3 | 9, 10 |
| T3-EQ-1 to T3-EQ-4 | 5, 9 (T3-EQ-2 also 7) |
| T3-EQ-5, T3-EQ-6 | 9, 10 |
| T3-TYPE-1, T3-TYPE-2 | 5, 9, 10 |
| T3-TYPE-3 | 9, 10 |
| T3-SCOPE-1 to T3-SCOPE-6 | 6, 10 |
| T3-SCOPE-7 | 6, 9 |
| T3-MOD-1, -7 | 9 |
| T3-MOD-2, -6, -8 | 9, 10 (T3-MOD-2 also 6) |
| T3-MOD-3, -4, -5 | 6, 10 |
| T3-MOD-9 | 7, 10 |
| T3-ARCH-1 | 3 |
| T3-ARCH-2, -3, -4, -5, -6, -8 | 9 (T3-ARCH-2, -4, -8 also 12) |
| T3-ARCH-7 | 7 |
| T3-ARCH-9 | 2, 12 |
| T3-DOC-1, T3-DOC-2 | 11 |
| AC-1 | 10 (with 1, 3, 6, 7, 9) |
| AC-2 | every task; 2 (`parser_test.cpp`), 10 (`t_decl_2_*`) |
| AC-3 | 12 |
| AC-4 | 11 |

## Change log
- **2026-11-21 (implementation-architect):** Created. Tasks 1–12 follow plan §8 T1–T12.
  Recorded DR-1 (blocking for tasks 2, 4, 8 and, transitively, 3, 5–7, 9–12), DR-2, DR-3, and
  DR-4. All progress fields initialized.
- **2026-11-21 (implementation-loop):** Recorded developer resolutions. DR-1: option (a); tasks
  2, 4, and 8 are unblocked and may make the minimal compile-only edits listed under DR-1 in
  later tasks' files. DR-3: new TOC lines go at the end of the Language Reference list, existing
  placeholders untouched; `docs/sitemap.xml` may be updated. DR-4: T3-CONST-5's second case is
  reported at the target `n` as `cannot assign to constant`. Task 1 revision: comment-only fix
  in `src/frontend/lexer/lexer.hpp` (35 reserved words; new negative-literal contexts), as
  directed by the parent; task 1 Revision set to Done.
- **2026-11-22 (implementation-loop):** Developer directed that the stale `ImmutableObject` doc comment in `src/runtime/object_model/object.hpp` be fixed under plan 0003 as a comment-only correction (adds `Boolean`). Applied as a Task 4 revision; Task 4 Revision set to Done.
- **2026-11-24 (implementation-loop):** Recorded developer decision (2026-11-22) on Task 3 review Round 1 finding 1: statement-position `a === b` and `a !== b` are reported at the second `=` (the "second `=`" row of plan §5.2 wins, consistent with T3-PARSE-2). Task 3 revision authorized to set that location and add statement-position tests for both forms. Developer also approved continuing with Task 5 and then Tasks 6–12 in dependency order.
- **2026-11-24 (implementation-loop):** Recorded developer decision (2026-11-22) for Task 5, option (b): add `[[nodiscard]] bool Integer::equals(const Integer&) const noexcept` to `src/runtime/object_model/integer.hpp`/`.cpp`. Integer storage stays private; no value accessor is added. Task 5 file scope is extended to those two files and to a focused test in `tests/unit/runtime/object_model/object_test.cpp` (including boundary values such as `INT64_MIN` vs `0`, which share an exit status).
