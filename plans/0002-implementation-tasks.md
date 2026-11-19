# Plan 0002 — Implementation Tasks

- **Plan ID:** `0002`
- **Plan:** `plans/0002.md` (status Approved, revision 5)
- **Review:** `plans/0002_review.md` (overall verdict Go)
- **Architecture decisions:** `plans/0002-architecture_decisions.md`
- **Shared implementation notes:** `plans/0002-implementation_notes.md` (created by later stages)
- **Created:** 2026-11-18 by the implementation-architect stage

This file is the handoff contract between the stub creator, implementation generator, reviewer,
and reviser. The tasks follow plan §7, tasks 1–7, with the same numbers. Every task traces to plan
sections and §8 test IDs. Nothing here adds behavior or scope beyond the plan.

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
- **Task numbers.** Task numbers are stable. On a re-run, preserve the recorded progress. Change a
  task only if the approved plan changes, and record that change in "Change log" at the end.
- **Order.** Numeric order 1 → 7 is a valid dependency order and is the recommended processing
  order.
- **Gates for every task (plan §7).**
  - The build is warning-free under `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`.
  - `clang-tidy` (`cert-*`, `bugprone-*`) reports no diagnostics.
  - The full suite passes in the Debug and ASan builds, including the task's tests.
  - Every plan 0001 test passes unless the plan changes it (T-UPD).
  - Unit tests use Catch2 and mirror `src/` under `tests/unit/`.
  - CLI golden cases use `tests/cli/cases/` fixtures registered in `tests/cli/cli_cases.cmake`.
- **Boundaries.**
  - Do not edit `docs/language_rules.md`, `AGENTS.md`, or any plan file other than this
    breakdown and the notes file.
  - Do not add dependencies.
  - Use only a direct Bash tool inside the Linux environment. Never use `wsl.exe`, PowerShell,
    `cmd.exe`, Cygwin, or Git for Windows Bash.
  - Do not access the network or run mutating `git` commands.

## Summary
| Task | Title | Dependencies | Tests covered (§8) |
|---|---|---|---|
| 1 | CLI driver support for stdout | None | Existing suite (driver modes are exercised in task 5) |
| 2 | Lexer: reserved words, `-`, negative literals, `?` | None | T-UPD-1, T-UPD-2, T-UPD-3, T2-LEX-1 to T2-LEX-7 |
| 3 | Types | 2 | T2-TYPE-1 to T2-TYPE-8; T2-TYPE-9 except its `nullnull` program; T-UPD-4 (T-NULL-3, T-FN-5, T-DECL-3) |
| 4 | Output seam | None | T2-ECHO-9; the existing suite; T-BUILD-6 unchanged |
| 5 | Pseudo-functions | 1, 2, 3, 4 | T2-ECHO-1 to T2-ECHO-8, T2-TYPEOF-1 to T2-TYPEOF-8, T2-OWN-1, T2-OWN-2, T2-BUILD-1; the T2-TYPE-9 `nullnull` program |
| 6 | Documentation | 1, 2, 3, 4, 5 | T2-DOC-1, T2-DOC-2 |
| 7 | End-to-end verification | 1, 2, 3, 4, 5, 6 | Full suite in Debug, ASan, and Release; acceptance report |

## Task 1: CLI driver support for stdout
- **Plan references:**
  - §7 task 1
  - §3.1 (the driver row)
  - §3.6
  - §8 preamble ("stdout `text`" means `EXPECT_STDOUT_HEX`)
  - Prepares T2-ECHO-6, T2-TYPEOF-6, and T2-BUILD-1 (registered in task 5)
- **Dependencies:** None
- **Scope:**
  - **Files.** `tests/cli/run_case.cmake` and `tests/cli/cli_cases.cmake`. Update the header
    comment of `run_case.cmake` to document the new variables and modes.
  - **No `src/` change.**
  - **The CMake wrapper.** `philios_cli_test` is defined in `CMakeLists.txt`, which the plan does
    not list. Stay within the two files above, for example with a helper in `cli_cases.cmake`
    that passes the new `-D` variables to `run_case.cmake`. If that is not workable, stop and
    ask the developer (architecture decisions §6, item 2).
- **Implementation objective:**
  - **`EXPECT_STDOUT_HEX`.** This variable holds the exact expected stdout as lowercase
    hexadecimal. The driver:
    - captures stdout to a file with `OUTPUT_FILE`;
    - reads the file with `file(READ ... HEX)`;
    - compares the result exactly with `EXPECT_STDOUT_HEX`.

    When the variable is unset, stdout must be empty, as before.
  - **`STDOUT_MODE`.** It takes one of these values:
    - `capture` (the default).
    - `dev_full`: stdout is `/dev/full`. When `/dev/full` does not exist, the driver writes
      `<SKIP_LABEL> SKIPPED: /dev/full not available`, and the case is reported as skipped
      through `SKIP_REGULAR_EXPRESSION`.
    - `closed`: the binary runs with descriptor 1 closed, through `sh -c 'exec "$0" >&-'
      <binary>`.
    - `broken_pipe`: two `COMMAND`s in one `execute_process` pipe the binary's stdout to
      `${CMAKE_COMMAND} -E true`. The binary's own status comes from `RESULTS_VARIABLE`.

    In `dev_full`, `closed`, and `broken_pipe`, stdout is not checked.
  - **New generated modes.**
    - `pseudo_nesting`: a `main` whose body is `echo(` followed by `NESTING - 2` nested
      `typeof(`, the string `"x"`, and the closing parentheses.
    - `broken_pipe_program`: the T2-ECHO-6 program, in which `spam(s)` echoes its argument and
      recurses, and `main` calls it with a string of 65,536 `x` characters.
  - **Generated inputs.** They are written into the build directory at test time and never
    committed, as for plan 0001's generated modes.
- **Stub and unit-test work:**
  - No C++ stubs.
  - There are no new C++ unit tests.
  - CLI cases that use the new options belong to task 5, because no program can write to stdout
    until then.
  - The stub stage confirms that the existing cases still register and run unchanged.
- **Acceptance criteria:**
  - The whole existing suite passes unchanged in the Debug and ASan builds, with stdout still
    checked as empty by default.
  - The new variables and modes are documented in the header comment of `run_case.cmake`.
  - No `src/` file changes.
- **Correction (2026-11-18; architecture decisions §6 item 6).** As first written, the
  `pseudo_nesting` generator emitted one extra `)` (template `echo(${opens}"x"${closes})` while
  `closes` already included `echo`'s close), so task 5's T2-TYPEOF-6 nesting 255/256 cases
  failed. By developer decision, the fix is made within task 5's scope, limited to the
  `MODE pseudo_nesting` branch of `tests/cli/run_case.cmake`: `closes` repeats `)` exactly
  `NESTING - 2` times, and the template's literal `)` closes `echo(`. Task 1 stays `GO`, and its
  progress fields are unchanged.
- **Correction (2026-11-19; architecture decisions §6 item 8).** `t_parse_5_long_line` takes 8
  to 9.3 seconds under ASan against the hard-coded `TIMEOUT 10` in the `execute_process` calls,
  so it is flaky when tests run in parallel. This predates plan 0002. By developer decision:
  - **Change.** Raise the run-time limit for `t_parse_5_long_line` from 10 to 20 seconds. Every
    other CLI case keeps 10 seconds. The CTest `TIMEOUT` property (120) does not change.
  - **Who applies it.** The implementation-reviser, as a correction to task 1, following the
    precedent of the 2026-11-18 correction.
  - **Scope.** `tests/cli/run_case.cmake` only: the `TIMEOUT` used for `MODE long_line`, and the
    header comment that mentions `TIMEOUT 10`. `tests/cli/cli_cases.cmake` may change only if
    strictly needed for this. `CMakeLists.txt` does not change. No language rule or plan behavior
    changes.
  - **Acceptance.** Every other case still runs with a 10-second limit. The whole suite passes in
    the Debug and ASan builds. The header comment states the limits.
  - **If the test still exceeds 20 seconds**, write a deeper explanation in
    `plans/0002-implementation_notes.md` (developer instruction).
  - **Review.** Task 1 gets a new review round for this correction. Its progress stays
    `Done`/`Done`/`GO` unless the re-review is not `GO`.
- **Stub and tests:** Done
- **Implementation:** Done
- **Review:** GO
- **Revision:** Done

## Task 2: Lexer: reserved words, `-`, negative literals, `?`
- **Plan references:**
  - §7 task 2
  - §3.1 (the reserved-word, punctuation, and negative-literal rows)
  - §3.2
  - §3.7
  - §4 (the lexical rows and the `-` and `?`-outside-a-type syntax rows)
  - §5 (malformed input, numeric edge cases)
  - Decisions D1, D2, D4–D7, D20–D22
- **Dependencies:** None
- **Scope:**
  - **Token kinds.** `src/frontend/lexer/token.hpp` gains `Minus`, `Question`, `KeywordNot`,
    `KeywordGoto`, `KeywordIsa`, `KeywordEcho`, and `KeywordTypeof`. Its comments are updated to
    28 reserved words.
  - **Conversion.** `parseNegativeDecimalInteger` is added to
    `src/frontend/lexer/integer_literal.hpp/.cpp`.
  - **Lexer.** `src/frontend/lexer/lexer.hpp/.cpp` implements §3.2.
  - **Parser.** No parser change. `Minus`, `Question`, and the new keywords already reach its
    unexpected-token and reserved-word errors. Verify their class and location.
  - **Plan 0001 test updates (T-UPD-1, T-UPD-2).**
    - In these CLI fixtures, `-` becomes `@`, and their comments in `tests/cli/cli_cases.cmake`
      say `@`:
      - `tests/cli/cases/lex_4_error_line_3_lf.phil`
      - `tests/cli/cases/lex_4_error_line_3_crlf.phil`
      - `tests/cli/cases/lex_4_error_line_3_cr.phil`
      - `tests/cli/cases/lex_4_error_line_3_mixed.phil`
      - `tests/cli/cases/lex_4_lf_cr_two_breaks.phil`
      - `tests/cli/cases/lex_4_cr_in_block_comment.phil`
      - `tests/cli/cases/lex_2_byte_column_after_utf8.phil`
    - In `tests/unit/frontend/lexer/lexer_test.cpp`, the T-LEX-2 unit test that lexes `return
      "é" -` uses `@` instead.
    - The `lex_8_negative_literal.phil` case now expects exit 255 with empty stderr.
    - The `Module::load` unit test in `tests/unit/runtime/evaluator/module_test.cpp` now expects
      `return -1` to load.
  - **Tests.**
    - New CLI fixtures in `tests/cli/cases/` and cases in `tests/cli/cli_cases.cmake`.
    - Unit tests in `tests/unit/frontend/lexer/integer_literal_test.cpp` and
      `tests/unit/frontend/lexer/lexer_test.cpp`.
- **Implementation objective:**
  - **Reserved words.** Exactly 28 are reserved, and each lexes as a keyword, never as an
    identifier.
  - **New tokens.** `-` and `?` are tokens.
  - **Expression-start context.** The previously emitted token is `(`, `,`, or `return`.
    Whitespace, comments, and `;` continuations are not tokens and do not change the context.
  - **Negative literal.** In expression-start context, a `-` immediately followed by `1`–`9`
    starts a single integer-literal token. The token's text includes the `-`, its location is
    the `-`, and its value is `std::int64_t`.
  - **Lexical errors at the `-`.**
    - a `-` followed by `0` (`-0`, `-00`, `-007`, `-0abc`);
    - a magnitude above 9,223,372,036,854,775,808;
    - a digit run followed by an identifier character (the malformed-number error).
  - **Every other `-`.** It is its own `Minus` token, so `--` is two `Minus` tokens.
  - **Conversion.** `parseNegativeDecimalInteger`:
    - parses the magnitude as `std::uint64_t` with `std::from_chars` and requires full
      consumption;
    - returns no value on out-of-range or a magnitude above 2^63;
    - negates in unsigned arithmetic and converts the result to `std::int64_t`.

    `parseDecimalInteger` is unchanged.
  - **Bounds.** All new lookahead goes through the bounds-checked accessor.
- **Stub and unit-test work:**
  - **Stubs.** The new token kinds and the `parseNegativeDecimalInteger` declaration, with a
    placeholder body.
  - **Unit tests.**
    - T2-LEX-4 (`parseNegativeDecimalInteger` values and no-value cases, including 1,000 digits).
    - T2-LEX-5 (token streams).
    - T2-LEX-6 (end-of-input lookahead on exact-size heap storage under ASan, extending T-LEX-12).
  - **CLI cases.** T2-LEX-1, -2, -3, -7, and T-UPD-3 (the five new words as function names, line
    3, column 10, and as parameter names, line 3, column 13).
  - **Updated tests.** T-UPD-1 and T-UPD-2, as listed under Scope.
- **Acceptance criteria:**
  - T-UPD-1, T-UPD-2, and T-UPD-3 pass.
  - T2-LEX-1 through T2-LEX-7 pass with the exact statuses, classes, lines, and columns given in
    plan §8, including:
    - `return -1` → 255;
    - `return -9223372036854775808` → 0;
    - `second(-1, -2)` → 254;
    - the lexical errors at line 2, column 12;
    - `9223372036854775808` is still a lexical error at line 2, column 12;
    - the `-` syntax errors at their stated columns;
    - `return ?1` is a syntax error at line 2, column 12.
  - Every other plan 0001 test passes unchanged.
  - The gates for every task are met.
- **Stub and tests:** Done
- **Implementation:** Done
- **Review:** GO
- **Revision:** Not required

## Task 3: Types
- **Plan references:**
  - §7 task 3
  - §3.1 (the type-grammar, exact-typing, and bare-`return` rows)
  - §3.3 (`typeName`, `baseType`, `nullableBase`, adjacency, non-declarable words)
  - §3.4
  - §4 (the `?` and type-word syntax rows, the `main` annotation and bare-`return` declaration
    rows, and the acceptance and falling-off-the-end runtime rows)
  - Decisions D9–D11, D19, D23, D25, D29
- **Dependencies:** 2 (`Question` token and `Object` keyword handling)
- **Scope:**
  - **AST.** `src/frontend/ast/ast.hpp/.cpp`: `TypeName::Object`, the plain aggregate
    `DeclaredType { TypeName base; bool nullable; }`, and `Param::type()` and
    `FunctionDecl::returnType()` using it.
  - **Parser.** `src/frontend/parser/parser.hpp/.cpp`: the §3.3 type grammar and the `?`
    adjacency check from token locations.
  - **Validator.** `src/analysis/semantic_analyzer/validator.hpp/.cpp`: the `main` annotation
    check on `DeclaredType`. The plan 0001 D26 bare-`return` check now exempts `?T` functions
    (D29).
  - **Type check.** `src/runtime/evaluator/type_check.hpp/.cpp`: `matchesDeclaredType(const
    Object&, DeclaredType) noexcept` and `scriptTypeName(const Object&) noexcept`.
  - **Callers.** `src/runtime/evaluator/call.cpp` and `src/runtime/evaluator/evaluate.cpp`: a
    bare `return` yields the context's `null`, which the return check accepts for `?T`. Falling
    off the end of a `?T` function stays the missing-return error.
  - **Tests.**
    - Unit tests under `tests/unit/` that mirror these files: `frontend/parser/parser_test.cpp`,
      `analysis/semantic_analyzer/validator_test.cpp`, a test for `type_check` under
      `runtime/evaluator/`, and the object-model `static_assert`s under `runtime/object_model/`.
    - Update any existing unit test or `tests/unit/support/ast_builders` helper that builds
      `Param`/`FunctionDecl` with a bare `TypeName`.
    - New CLI fixtures and cases.
- **Implementation objective:**
  - **Declarable types.** `Integer`, `String`, `Function`, `Object`, `null`, `?Integer`,
    `?String`, `?Function`, and `?Object`.
  - **Adjacency.** A `?` must be immediately followed by the type word, on the same line, one
    column after it. Otherwise the parser reports a syntax error at the `?`.
  - **Syntax errors at the token after the `?`.** `?null`, `??`, `?)`, and `?NullObject`.
  - **Non-declarable type words.** `ImmutableObject`, `NullObject`, `UnsignedInteger`, and
    `Float` are syntax errors at the word.
  - **Acceptance.** Use the §3.4 table. Keep plan 0001's check timing, order, and locations.
  - **`main`.** Its annotation must be omitted or exactly `Integer`.
  - **D29 return rules.**
    - A bare `return` in a `?T` function returns `null`.
    - A bare `return` in an `Object`, `Integer`, `String`, or `Function` function is still the
      declaration error.
    - Falling off the end of a `?T` or `Object` function is the runtime missing-return error at
      the function name.
    - `null` functions and `main` are unchanged.
  - **Calls through the new types.** A call through an `Object` or `?Function` parameter whose
    value is not a `Function` is the not-callable error at the callee.
  - **No object-model change.** No class is added or changed in `src/runtime/object_model/`.
- **Stub and unit-test work:**
  - **Stubs.** `DeclaredType`, `TypeName::Object`, and the new `matchesDeclaredType` and
    `scriptTypeName` signatures, with placeholder bodies.
  - **Unit tests.**
    - T2-TYPE-6: all 36 pairs of the four runtime classes and the nine declarable types, plus
      `scriptTypeName` for the four classes.
    - T2-TYPE-7: `static_assert`s on the inheritance and on `Object` being abstract.
    - T2-TYPE-8: all nine types as parameter and as return types.
  - **CLI cases.** T2-TYPE-1 to T2-TYPE-5 and T2-TYPE-9, with these limits:
    - Task 3 registers only the T2-TYPE-9 programs that do not use `echo` or `typeof`:
      - a `?String` function with `pass` and then a bare `return` exits 0;
      - a statement after a bare `return` in a `?Integer` function is a declaration error;
      - a called `?Integer` function that falls off its end exits 70 at line 1, column 10;
      - plan 0001 T-DECL-4 and the `null` function tests pass unchanged.
    - The T2-TYPE-9 `nullnull` program needs `echo`, `typeof`, and `EXPECT_STDOUT_HEX`, so it is
      registered in task 5 (architecture decisions §6, item 1).
- **Acceptance criteria:**
  - T2-TYPE-1 through T2-TYPE-8 pass with the statuses, classes, lines, and columns in plan §8,
    including:
    - `? Integer`, `?/*c*/Integer`, and `?` followed by a tab → a syntax error at column 15;
    - `?null`, `??Integer`, `?)`, and `?NullObject` → column 16;
    - the `main` annotation errors at line 1, column 10;
    - `null` to `Object` at line 4, column 17.
  - The T2-TYPE-9 cases listed above pass.
  - Plan 0001 T-NULL-3, T-FN-5, T-DECL-3, and T-DECL-4 pass unchanged (T-UPD-4).
  - `matchesDeclaredType` remains the only place a value is compared with a declared type.
  - The gates for every task are met.
- **Stub and tests:** Done
- **Implementation:** Done
- **Review:** GO
- **Revision:** Not required

## Task 4: Output seam
- **Plan references:**
  - §7 task 4
  - §3.6 (destination, output seam, shipped writer, broken pipes, error message)
  - §3.8 (`RuntimeErrorKind::OutputWrite`)
  - §5 (output, concurrency)
  - Decisions D13, D18, D24, D26
- **Dependencies:** None
- **Scope:**
  - **Writer type.** New `src/runtime/evaluator/script_output.hpp` declares `OutputWriter`, using
    standard headers only.
  - **Execution context.** `src/runtime/evaluator/execution_context.hpp/.cpp` gains `explicit
    ExecutionContext(OutputWriter output)`, which stores the writer by value and throws
    `InternalError` if it is empty, and `[[nodiscard]] std::error_code
    writeOutput(std::string_view bytes)`.
  - **Errors.** `src/common/errors.hpp/.cpp` gains `RuntimeErrorKind::OutputWrite`.
  - **Shipped writer.** New `src/cli/standard_output.hpp/.cpp` provides
    `writeStandardOutput(std::string_view) noexcept` and `ignoreBrokenPipeSignal()`.
  - **CLI.** `src/cli/cli.cpp`: `runProject` calls `ignoreBrokenPipeSignal()` and constructs the
    context with `writeStandardOutput` before it loads `main.phil`. In `src/cli/cli.hpp`, replace
    the comment "Nothing is written to stdout".
  - **Fake writer.** New `tests/unit/support/capturing_output.hpp/.cpp` provides a shared
    capturing fake writer.
  - **Existing tests.** Every test that constructs an `ExecutionContext` passes a writer. Today
    these are in:
    - `tests/unit/runtime/evaluator/evaluate_test.cpp`
    - `tests/unit/runtime/evaluator/execution_context_test.cpp`
    - `tests/unit/runtime/evaluator/module_test.cpp`
- **Implementation objective:**
  - **Writer contract.**
    - Write every byte and flush.
    - Return an empty `std::error_code` on success and a non-empty one on any failure.
    - Never report through `errno`.
    - Run no Philios code.
    - Throw nothing but `std::bad_alloc`.
  - **`writeStandardOutput`.**
    - Call `std::fwrite` on `stdout`, skipped for zero bytes.
    - Then call `std::fflush(stdout)`.
    - On a short write or a failed flush, convert `errno` immediately to a
      `std::generic_category()` code, use `std::errc::io_error` if `errno` is 0, and call
      `std::clearerr(stdout)`.
  - **`ignoreBrokenPipeSignal`.** Call `std::signal(SIGPIPE, SIG_IGN)` and throw `InternalError`
    on `SIG_ERR`.
  - **No global state.** No global state, signal handler, or flag is added.
  - **Headers.** Only `<cstdio>`, `<csignal>`, and other standard headers, so T-BUILD-6 POSIX
    confinement still passes.
  - **No language behavior.** No `echo` evaluation is added here (task 5).
- **Stub and unit-test work:**
  - **Stubs.** `script_output.hpp`, the new `ExecutionContext` constructor and `writeOutput`, the
    `OutputWrite` kind, and the `standard_output` declarations with placeholder bodies.
  - **Fake writer.** The capturing fake writer in `tests/unit/support/`.
  - **Unit tests (T2-ECHO-9), in `tests/unit/runtime/evaluator/execution_context_test.cpp`.**
    - An empty `OutputWriter` throws `InternalError`.
    - `writeOutput` passes the bytes to the writer and returns its result.
  - **Existing tests.** Update every existing `ExecutionContext` construction to pass a writer.
- **Acceptance criteria:**
  - T2-ECHO-9 passes.
  - The whole existing suite passes, including T-BUILD-6 with the new
    `src/cli/standard_output.cpp`.
  - No CLI case's stdout changes.
  - The gates for every task are met.
- **Stub and tests:** Done
- **Implementation:** Done
- **Review:** GO
- **Revision:** Done

## Task 5: Pseudo-functions
- **Plan references:**
  - §7 task 5
  - §3.3 (`pseudoCall`, argument count, `echo` literal arguments, bare names, nesting)
  - §3.5
  - §3.6 (exact bytes, one flush per `echo`)
  - §3.8 (AST, validator, evaluator)
  - §4 (the pseudo-function rows, the `echo` argument-type and output-write runtime rows, and
    "errors before execution leave stdout empty")
  - §5 (nesting, lifetimes, output)
  - Decisions D3, D8, D12–D18, D20, D24, D26
- **Dependencies:** 1, 2, 3, 4
  - Task 1 provides `EXPECT_STDOUT_HEX`, `STDOUT_MODE`, `pseudo_nesting`, and
    `broken_pipe_program`.
  - Task 2 provides the `echo` and `typeof` keyword tokens and negative literals.
  - Task 3 provides `Object`, `?T`, and `scriptTypeName`.
  - Task 4 provides `writeOutput`, `OutputWrite`, and the capturing writer.
- **Scope:**
  - **AST.** `src/frontend/ast/ast.hpp/.cpp`: `final` classes `EchoCall` and `TypeofCall`. Each
    holds the keyword location and one `std::unique_ptr<ASTNode>` argument. Add the `NodeKind`
    entries and `detachChildren`, with iterative, `noexcept`, non-allocating destruction.
  - **Parser.** `src/frontend/parser/parser.hpp/.cpp`: `pseudoCall` in `expr` and `exprStmt`, the
    argument-count check, the `echo` literal check, and the nesting counter.
  - **Validator.** `src/analysis/semantic_analyzer/validator.cpp` handles both kinds explicitly.
    There is no casting `default`.
  - **Evaluator.** `src/runtime/evaluator/evaluate.cpp` implements `evaluate` for both nodes.
  - **`String` accessor (developer decision, 2026-11-18; architecture decisions §1.4 and §6
    item 4).** `src/runtime/object_model/string_object.hpp/.cpp` gain exactly one read-only
    accessor, `[[nodiscard]] std::string_view String::bytes() const noexcept`, which `echo` uses
    to read the bytes it writes. This is the only authorized exception to plan §3.4 ("No class is
    added or changed"). The accessor is the only change to these two files. The hierarchy,
    `ObjectType`, and behavior are unchanged, and no other class may change.
  - **`pseudo_nesting` correction (developer decision, 2026-11-18; architecture decisions §6
    item 6; correction to task 1).** `tests/cli/run_case.cmake`, limited to the
    `MODE pseudo_nesting` branch: remove the extra `)` so `closes` repeats `)` exactly
    `NESTING - 2` times and the template's literal `)` closes `echo(`. No other part of the file
    may change under this item.
  - **Tests.**
    - Unit tests in `tests/unit/frontend/ast/ast_test.cpp`,
      `tests/unit/frontend/parser/parser_test.cpp`, and
      `tests/unit/runtime/evaluator/evaluate_test.cpp`.
    - New CLI fixtures and cases.
    - The T2-BUILD-1 `WILL_FAIL` cases in `tests/cli/cli_cases.cmake`.
- **Implementation objective:**
  - **Parsing.**
    - A pseudo-call needs exactly one argument. Otherwise the parser reports a syntax error at the
      keyword after it parses the arguments.
    - An integer literal or `null` literal argument to `echo` is a syntax error at the argument's
      first byte.
    - A bare `echo` or `typeof` is the reserved-word syntax error at the word.
    - A pseudo-call's argument list counts as one nesting level toward 256.
  - **Validation.** Undefined identifiers and any use of `main` in the argument are declaration
    errors.
  - **Evaluation guards.** Each evaluation starts with the evaluation-depth guard, at the keyword.
    It adds no call depth, no `Environment`, and no call-depth guard.
  - **Argument sequencing.** The argument is evaluated once, into an owning `std::shared_ptr`
    local, before any check.
  - **`echo`.**
    1. If the value is not a `String`, raise `ArgumentType` at the argument and write nothing.
    2. Otherwise write the exact bytes through `writeOutput`. On failure, raise `OutputWrite` at
       the keyword, with a message containing `standard output` and the escaped reason.
    3. Return the context's `null`.
  - **`typeof`.** Return a new `String` holding `scriptTypeName(value)`.
- **Stub and unit-test work:**
  - **Stubs.** `EchoCall`, `TypeofCall`, and the `NodeKind` entries, plus placeholder evaluate,
    parse, and validate branches, following the plan 0001 stub convention.
  - **Unit tests.**
    - **T2-ECHO-8**, using the capturing writer:
      - a `String` is written once, and the result is the context's `null`;
      - a non-`String` raises the argument-type error at the argument, and the writer is not
        called;
      - a failing writer raises `OutputWrite` at the keyword;
      - the call depth seen by the writer is unchanged;
      - at `kMaxEvalDepth`, an `EchoCall` or `TypeofCall` raises the evaluation-depth error at
        the keyword, and the writer is not called.
    - **T2-TYPEOF-8:** each evaluation returns a new `String`, and a recording argument is
      evaluated exactly once.
    - **T2-OWN-2:** nothrow-destructible `static_assert`s and a chain of at least 200,000
      `TypeofCall` nodes.
  - **CLI cases.**
    - T2-ECHO-1 to T2-ECHO-7.
    - T2-TYPEOF-1 to T2-TYPEOF-7.
    - T2-OWN-1 runs under ASan: a normal echo and exit, a pseudo-call syntax error, an `echo`
      argument-type error, and an output-write error.
    - T2-BUILD-1: two `WILL_FAIL TRUE` self-checks, one with a one-byte `EXPECT_STDOUT_HEX`
      mismatch and one with no expected stdout for a program that echoes `x`.
    - The T2-TYPE-9 `nullnull` program, exit 0, stdout hex `6e756c6c6e756c6c`.
- **Acceptance criteria:**
  - All tests listed above pass with the exact statuses, classes, lines, columns, and stdout bytes
    in plan §8, including:
    - Hello World: stdout hex `48656c6c6f20576f726c64`, exit 1, empty stderr.
    - T2-ECHO-6: `dev_full` (or a recorded skip), `closed`, and `broken_pipe` each exit 70 at line
      2, column 5, with `standard output` in the message.
    - T2-TYPEOF-6: nesting 255 and 256 exit 0 with stdout `String`; 257 is a syntax error at line
      2, column 1794.
  - A syntax error or declaration error leaves stdout empty.
  - Plan 0001 T-BUILD-6 still passes.
  - The gates for every task are met.
- **Stub and tests:** Done
- **Implementation:** Done
- **Review:** GO
- **Revision:** Done

## Task 6: Documentation
- **Plan references:**
  - §7 task 6
  - §2 (in-scope documentation)
  - §6 (`directory-structure.md`)
  - §8 T2-DOC-1 and T2-DOC-2
  - Decision D27; sources of truth §3.2–§3.7 and §4, with plan 0001 for unchanged rules
- **Dependencies:** 1, 2, 3, 4, 5
- **Scope:**
  - **End-user pages and their required content.**
    - `docs/end-users/src/index.md`: link `echo` and `typeof` under the implemented reference
      pages. Remove `echo` and "Hello World" from "Planned Topics" once they are documented, and
      keep the other planned topics.
    - `docs/end-users/src/getting-started/first-program.md`:
      - replace "Philios has no output functions yet" and "A successful program writes nothing to
        the terminal" with the D13/D24 rules;
      - add the Hello World program of T2-ECHO-1, with its exact output (no line break) and exit
        status 1;
      - state that a negative `main` result maps modulo 256 (`return -1` exits 255).
    - `docs/end-users/src/getting-started/installation.md`: correct only what no longer matches the
      build and run steps. If nothing is affected, record that in the notes.
    - `docs/end-users/src/reference/source-files.md`:
      - the 28 reserved words and which of them have a meaning;
      - negative literals: their contexts, range, and the `-0` rule;
      - that any other `-` is an error;
      - `?` only in `?Type`.
    - `docs/end-users/src/reference/types.md`:
      - `Object` and the nullable forms;
      - the §3.4 acceptance table, replacing "Exact Type Matching";
      - `null` acceptance;
      - the negative `Integer` literal range;
      - the reserved, non-declarable type names.
    - `docs/end-users/src/reference/functions.md`:
      - the §3.4 return rules, including D29;
      - the available types;
      - `echo(String str): null` and `typeof(expr)`, with their rules, script-level names, and
        errors.
    - `docs/end-users/src/reference/errors-and-limits.md`:
      - replace "It never writes to standard output";
      - add every new §4 error under its class with its exit status, including output-write
        failures and broken pipes;
      - state that pseudo-call argument lists count toward the 256 nesting limit.
  - **Developer pages.**
    - `docs/developers/src/index.md`
    - `docs/developers/src/getting-started/index.md`
    - `docs/developers/src/getting-started/ai-usage.md`
    - `docs/developers/src/getting-started/compiling-philios.md`
    - `docs/developers/src/getting-started/directory-structure.md`
    - `docs/developers/src/getting-started/required-tools.md`
    - `docs/developers/src/cpp-guidelines/index.md`

    Correct every statement that no longer matches the implementation. This covers the new files
    where a page lists files, the output seam where a page describes it, and
    `EXPECT_STDOUT_HEX`/`STDOUT_MODE` where a page describes tests. Leave a page with nothing
    affected unchanged.
  - **Example.** `docs/examples/hello_world/main.phil` changes only if it does not already write
    exactly `Hello World` and exit 1. `docs/examples/hello_world/philios.json` is unchanged.
  - **T2-DOC-1 case.** `tests/cli/cli_cases.cmake` gains one CLI golden case for T2-DOC-1.
  - **Copy-folder driver mode (developer decision, 2026-11-19; architecture decisions §6 item
    7, option (a)).**
    - `tests/cli/run_case.cmake`: add one mode that copies every file of a fixture (sample)
      directory into the fresh `WORK_DIR`, and document it in the header comment. This file is
      in scope for this mode and its header documentation only. No other driver behavior
      changes.
    - `tests/cli/cli_cases.cmake`: add the matching option to `philios_cli_stdout_test` that
      passes that directory to the driver.
    - `CMakeLists.txt` does not change.
  - **Files not edited.** `docs/language_rules.md`, `AGENTS.md`, and every plan file.
- **Implementation objective:** The linked documentation describes exactly what tasks 1–5
  implemented:
  - Planned behavior is omitted or listed as planned.
  - Every Philios example in a changed page produces the result the page states when saved as
    `main.phil`.
  - Every relative link in a changed page resolves.
- **Stub and unit-test work:**
  - No C++ stubs and no unit tests.
  - The stub stage adds the T2-DOC-1 CLI case: exit 1, stdout hex `48656c6c6f20576f726c64`,
    empty stderr.
  - **Precondition (resolved 2026-11-18).** Architecture decisions §6, item 3, is resolved: the
    developer chose option (b).
    - The T2-DOC-1 case copies `docs/examples/hello_world/main.phil` and
      `docs/examples/hello_world/philios.json` into a fresh `WORK_DIR` and runs the program
      there.
    - The current driver deletes and recreates `WORK_DIR`, so `docs/examples/hello_world/` must
      never be passed as `WORK_DIR`.
    - No step may modify or delete that directory.
    - The case stays within `tests/cli/cli_cases.cmake`. If the copy mechanism genuinely needs a
      change to `tests/cli/run_case.cmake`, stop and ask the developer.
  - **Precondition: resolved again (2026-11-19; architecture decisions §6 item 7).** The stub
    stage may start. The developer chose option (a) below.
    - **Fixture source (decided).** T2-DOC-1 uses `tests/unit/sample-programs/hello-world/`
      (`main.phil` and `philios.json`) instead of `docs/examples/hello_world/`.
    - **Hard constraint.** Neither `docs/examples/hello_world/` nor
      `tests/unit/sample-programs/hello-world/` is ever passed as `WORK_DIR`, modified, or
      deleted.
    - **Question (as first recorded; now answered).** The existing driver copies only one
      `.phil` file into a fresh `WORK_DIR` (`MODE file`). Copying both files needs a
      `tests/cli/run_case.cmake` change, which was outside this task's scope. The developer had
      to choose:
      - (a) authorize a minimal `run_case.cmake` mode that copies every file of a fixture
        directory into the fresh `WORK_DIR`, plus the matching option in
        `philios_cli_stdout_test`, which adds `run_case.cmake` to this task's scope;
      - (b) copy only `main.phil` from the new fixture with `MODE file`, with no driver change,
        because the binary never opens `philios.json` (D35);
      - (c) something else.
    - **Developer answer (2026-11-19), verbatim:** "(a) Add a run_case.cmake mode that copies
      every file in a sample folder, and add run_case.cmake to Task 6 scope".
    - **Resolution.**
      - The T2-DOC-1 case uses the new copy-folder mode (see Scope) with
        `tests/unit/sample-programs/hello-world/` as its source directory, so `main.phil` and
        `philios.json` are copied into a fresh `WORK_DIR`.
      - Expected result unchanged: exit 1, stdout hex `48656c6c6f20576f726c64`, empty stderr.
      - The new mode only reads from its source directory. The hard constraint above still
        applies to both `tests/unit/sample-programs/hello-world/` and
        `docs/examples/hello_world/`.
      - The 2026-11-18 limit "stays within `tests/cli/cli_cases.cmake`" is superseded only by
        this authorized `run_case.cmake` mode. Any other `run_case.cmake` or `CMakeLists.txt`
        change still requires stopping to ask the developer.
- **Acceptance criteria:**
  - T2-DOC-1 passes.
  - **T2-DOC-2 review.** The review is recorded in the task 6 section of
    `plans/0002-implementation_notes.md` and checked by the reviewer:
    - every listed file was read, and each was updated or recorded as needing no change;
    - no linked page says Philios cannot write output, that literals cannot be negative, that
      types must match exactly, that there are 23 reserved words, or that `-` is always an error;
    - the pages state the 28 words, the declarable types and the acceptance table, the D29 return
      rules, `echo` and `typeof` per §3.5 and §3.6, negative literals per §3.2, and every new §4
      error with its class and exit status;
    - the examples run as stated;
    - the relative links resolve;
    - `docs/language_rules.md` is unchanged.
  - The gates for every task are met.
- **Stub and tests:** Done
- **Implementation:** Done
- **Review:** GO
- **Revision:** Done

## Task 7: End-to-end verification
- **Plan references:**
  - §7 task 7
  - §8 preamble (Debug, ASan, Release, no warnings, no `clang-tidy` diagnostics)
  - T-UPD-4
  - Plan 0001 D48 (T-CLI-10 pass or skip reporting)
- **Dependencies:** 1, 2, 3, 4, 5, 6
- **Scope:**
  - No source, test, or documentation changes.
  - Run the full suite in the Debug, ASan, and Release builds (plan 0001 §7) on the default 8 MiB
    stack.
  - Write the acceptance report in the task 7 section of `plans/0002-implementation_notes.md`.
  - If a failure needs a code change, do not fix it here. Report the affected task numbers for
    the reviser.
- **Implementation objective:** The acceptance report states:
  - the results of the developer's four example programs (T2-ECHO-1, both T2-TYPEOF-1 programs,
    and T2-LEX-1's `return -1`);
  - whether the `dev_full` case ran or was skipped in each build;
  - the result of T2-DOC-1 and the T2-DOC-2 review;
  - whether T-CLI-10 passed or was skipped in each build (plan 0001 D48);
  - that the builds are warning-free and `clang-tidy` is clean.
- **Stub and unit-test work:** None. No stubs or tests are created. The stub stage records
  `Done` with "no stubs required".
- **Acceptance criteria:**
  - Every §8 test exists and passes in the Debug and ASan builds.
  - The full suite passes in the Release build.
  - There are no compiler warnings and no `clang-tidy` diagnostics.
  - The acceptance report contains every item listed under Implementation objective.
- **Stub and tests:** Done
- **Implementation:** Done
- **Review:** GO
- **Revision:** Not required

## Traceability: plan §8 test IDs to tasks
| §8 test | Task |
|---|---|
| T-UPD-1, T-UPD-2, T-UPD-3 | 2 |
| T-UPD-4 | 3 (T-NULL-3, T-FN-5); every task (suite gate); 7 (final) |
| T2-LEX-1 to T2-LEX-7 | 2 |
| T2-TYPE-1 to T2-TYPE-8 | 3 |
| T2-TYPE-9 | 3 (all cases except the `nullnull` program); 5 (the `nullnull` program) |
| T2-ECHO-1 to T2-ECHO-8 | 5 |
| T2-ECHO-9 | 4 |
| T2-TYPEOF-1 to T2-TYPEOF-8 | 5 |
| T2-OWN-1, T2-OWN-2 | 5 |
| T2-BUILD-1 | 5 (plan 0001 T-BUILD-6 re-checked in 4 and 5) |
| T2-DOC-1, T2-DOC-2 | 6 |
| Full-suite Debug/ASan/Release report | 7 |

## Open items affecting tasks
Full text is in `plans/0002-architecture_decisions.md` §6.
1. **T2-TYPE-9 `nullnull` program (tasks 3 and 5).** It is registered in task 5 because it needs
   `echo` and `typeof`. **Resolved 2026-11-18: the developer confirmed this assignment.**
2. **The CMake wrapper (task 1).** `philios_cli_test` lives in `CMakeLists.txt`, which the plan
   does not list. Task 1 must stay within `run_case.cmake` and `cli_cases.cmake`, or stop and ask.
   **Status 2026-11-18: no longer blocking.** Task 1 completed with `GO` within those two files,
   and `CMakeLists.txt` is unchanged. The developer gave no answer about changing
   `CMakeLists.txt`, so no such change is authorized.
3. **T2-DOC-1 working directory (task 6).** The developer must choose a non-destructive mechanism
   before task 6's stub stage. **Resolved 2026-11-18: option (b).** Copy `main.phil` and
   `philios.json` from `docs/examples/hello_world/` into a fresh `WORK_DIR`. Task 6 is no longer
   blocked by this item. See task 6's precondition. **Update 2026-11-19: superseded in part by
   open item 6; task 6 is blocked again.** **Update 2026-11-19 (later): open item 6 resolved;
   task 6 is unblocked.**
4. **How `echo` reads a `String`'s bytes (task 5).** **Resolved 2026-11-18.** The developer
   authorized one read-only accessor, `[[nodiscard]] std::string_view String::bytes() const
   noexcept`, in `src/runtime/object_model/string_object.hpp/.cpp`, as the only exception to plan
   §3.4. Those two files are added to task 5's scope for the accessor only.
5. **Extra `)` in the `pseudo_nesting` generator (tasks 1 and 5).** **Resolved 2026-11-18.** The
   developer decided: "Fix the extra parenthesis in run_case.cmake as part of Task 5, record it
   as a correction to Task 1, then continue". `tests/cli/run_case.cmake` is added to task 5's
   scope, limited to the `MODE pseudo_nesting` branch. Task 1 stays `GO`.
6. **T2-DOC-1 fixture source and copy mechanism (task 6).** **Resolved 2026-11-19: option (a).
   Task 6 is unblocked.** The source fixture is `tests/unit/sample-programs/hello-world/`. Like
   `docs/examples/hello_world/`, it is never `WORK_DIR` and is never modified or deleted. Task 6
   adds a copy-folder mode to `tests/cli/run_case.cmake` that copies every file of a fixture
   directory into the fresh `WORK_DIR` (read-only on the source), with its header documentation,
   plus the matching `philios_cli_stdout_test` option in `tests/cli/cli_cases.cmake`.
   `tests/cli/run_case.cmake` is added to task 6's scope for that mode only; `CMakeLists.txt`
   does not change. T2-DOC-1's expected result is unchanged. See architecture decisions §6
   item 7 and task 6's precondition.
7. **`t_parse_5_long_line` run-time limit (task 1).** **Resolved 2026-11-19.** Raise this case's
   limit from 10 to 20 seconds. Every other case keeps 10 seconds, and the CTest `TIMEOUT`
   (120) does not change. The implementation-reviser applies this as a correction to task 1,
   limited to `tests/cli/run_case.cmake` (and `tests/cli/cli_cases.cmake` only if strictly
   needed). Task 1 gets a new review round. If the case still exceeds 20 seconds, write a deeper
   explanation in `plans/0002-implementation_notes.md`. See architecture decisions §6 item 8.

## Change log
- 2026-11-18: Initial breakdown from plan 0002 revision 5 (Approved). Seven tasks follow plan §7.
  No earlier task record existed.
- 2026-11-18: Recorded developer decisions given through a structured form (architecture
  decisions §6). The plan did not change, no task was renumbered, and no progress field changed.
  - **Task 5 scope.** Added `src/runtime/object_model/string_object.hpp/.cpp`, limited to the
    read-only accessor `[[nodiscard]] std::string_view String::bytes() const noexcept`. This is
    the only authorized exception to plan §3.4. The hierarchy, `ObjectType`, and behavior are
    unchanged.
  - **Task 6 precondition.** Resolved with option (b): the T2-DOC-1 case copies `main.phil` and
    `philios.json` from `docs/examples/hello_world/` into a fresh `WORK_DIR`. Task 6 stays within
    `tests/cli/cli_cases.cmake`. If a `tests/cli/run_case.cmake` change is genuinely needed, stop
    and ask.
  - **Open item 1.** Confirmed: T2-TYPE-9's `nullnull` program stays in task 5.
  - **Open item 2.** Status recorded as non-blocking because task 1 completed with `GO` within
    its two files. The developer gave no answer about `CMakeLists.txt`.
- 2026-11-18: Recorded a developer decision on the `pseudo_nesting` generator bug (architecture
  decisions §6 item 6; open item 5). The plan did not change, no task was renumbered, and no
  progress field changed.
  - **Task 5 scope.** Added `tests/cli/run_case.cmake`, limited to the `MODE pseudo_nesting`
    branch, as a correction to task 1: `closes` repeats `)` exactly `NESTING - 2` times.
  - **Task 1.** Added a correction note. Task 1 stays `GO`.
- 2026-11-19: Recorded two developer decisions given through a structured form at the start of
  task 6 (architecture decisions §6 items 7 and 8; open items 6 and 7). The plan did not change,
  no task was renumbered, and no progress field changed.
  - **Task 6 precondition.** The T2-DOC-1 source fixture is now
    `tests/unit/sample-programs/hello-world/`. It must never be `WORK_DIR` and must never be
    modified or deleted. The precondition is **blocked again** because no copy mechanism for both
    files fits task 6's scope. The open question for the developer offers options (a), (b), and
    (c). Task 6's scope is unchanged.
  - **Task 1.** Added a correction note: raise `t_parse_5_long_line`'s run-time limit from 10 to
    20 seconds in `tests/cli/run_case.cmake`. The implementation-reviser applies it, and task 1
    gets a new review round. Task 1's progress fields are unchanged. Its `Revision` field was
    already `Pending`.
- 2026-11-19: Recorded the developer's answer to the T2-DOC-1 copy mechanism through a second
  structured form (architecture decisions §6 item 7; open item 6): "(a) Add a run_case.cmake mode
  that copies every file in a sample folder, and add run_case.cmake to Task 6 scope". The plan
  did not change, no task was renumbered, and no progress field changed.
  - **Task 6 scope.** Added `tests/cli/run_case.cmake`, limited to one new mode that copies
    every file of a fixture directory into the fresh `WORK_DIR` and its header documentation,
    plus the matching `philios_cli_stdout_test` option in `tests/cli/cli_cases.cmake`. No other
    driver behavior changes, and `CMakeLists.txt` does not change.
  - **Task 6 precondition.** Resolved; task 6 is unblocked. T2-DOC-1 copies `main.phil` and
    `philios.json` from `tests/unit/sample-programs/hello-world/`; expected result unchanged
    (exit 1, stdout hex `48656c6c6f20576f726c64`, empty stderr). The hard constraint on
    `tests/unit/sample-programs/hello-world/` and `docs/examples/hello_world/` is retained.
  - **Open item 6.** Marked resolved.
