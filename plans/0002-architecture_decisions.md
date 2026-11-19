# Plan 0002 — Architecture Decisions

- **Plan ID:** `0002`
- **Plan:** `plans/0002.md` (status Approved, revision 5; grill decisions D1–D30)
- **Review:** `plans/0002_review.md` (overall verdict Go; the latest Plan, Architecture, and
  Security rounds, Round 2 of 2026-11-18, are all Go with no findings)
- **Grill record:** `plans/0002_grill.md`
- **Task breakdown:** `plans/0002-implementation-tasks.md`
- **Stage:** implementation-architect (this record and the task breakdown only; no stubs, tests,
  or source changes)
- **Date:** 2026-11-18 (§6 items 7 and 8 added 2026-11-19; item 7 fully resolved 2026-11-19)

This record lists only decisions that the approved plan, its grill record, `AGENTS.md`, or
`.github/instructions/cpp-style-guide.instructions.md` already make. It adds no decision. A bare
`Dn` is a plan 0002 grill decision; plan 0001 decisions are written "plan 0001 Dn". Every plan
0001 decision recorded in `plans/0001-architecture_decisions.md` still applies unless §1 below
says plan 0002 replaces it. §5 lists interface details the plan leaves to later stages, and §6
lists the items that need the developer's attention.

## 1. Approved decisions this plan relies on

### 1.1 Source layout and dependency direction (plan §3.8, §7; plan 0001 D41–D45)
- The layout stays as plan 0001 established it: there is no `include/` directory, headers sit
  beside their sources under `src/`, and the single include root is `src/`. Includes use full
  paths in double quotes. Unit tests mirror `src/` under `tests/unit/`. Shared test helpers live
  in `tests/unit/support/` as `.hpp`/`.cpp` pairs. CLI golden fixtures live in
  `tests/cli/cases/`, and cases are registered in `tests/cli/cli_cases.cmake` and run by
  `tests/cli/run_case.cmake`.
- `CMakeLists.txt` collects `src/*.cpp` and `tests/unit/*.cpp` with `GLOB_RECURSE`, so the new
  source and test files the plan names are built without a `CMakeLists.txt` change. The plan
  lists no `CMakeLists.txt` change (see §6, item 2).
- The new files are exactly the ones the plan names:
  - `src/runtime/evaluator/script_output.hpp` (standard headers only)
  - `src/cli/standard_output.hpp/.cpp`
  - `tests/unit/support/capturing_output.hpp/.cpp`
  - new CLI fixtures under `tests/cli/cases/`
  - new unit test sources under `tests/unit/`, mirroring the `src/` file they test
- `src/frontend/` still includes nothing from `src/runtime/` or `src/analysis/`.
  `script_output.hpp` includes only standard headers. `src/cli/` may include runtime headers.
- No new directory is added, and `AGENTS.md` does not change (plan §6).

### 1.2 Lexer (plan §3.2; D2, D4–D7, D11, D20–D22, D25)
- **Reserved words.** There are exactly 28 reserved words. The new words are `not`, `goto`,
  `isa`, `echo`, and `typeof`, with token kinds `KeywordNot`, `KeywordGoto`, `KeywordIsa`,
  `KeywordEcho`, and `KeywordTypeof`. Matching is unchanged: case-sensitive, whole-token, and
  never an identifier.
- **New punctuation tokens.** `-` is `Minus`, and `?` is `Question`. Every other character
  outside strings and comments is still an unexpected-character lexical error.
- **Negative literals are a contextual single token, not an operator (D4, D5, D21).**
  - **Expression-start context.** The previously emitted token is `(`, `,`, or `return`. This
    list is closed for this plan.
  - **Literal form.** In that context, a `-` immediately followed by a digit `1`–`9` starts one
    integer-literal token. The token includes the `-`, its location is the `-`, and it reuses
    `IntegerLiteral` in the AST.
  - **Errors at the `-`.** A leading `0` (D7), a magnitude above 2^63 (D6), and a following
    identifier character (a malformed number) are lexical errors at the `-`.
  - **Every other `-`.** Any other `-` is a separate `Minus` token, which no grammar rule accepts,
    so it is a syntax error at the `-`. The lexer never combines `--` into one token (D22).
- **Conversion.** `[[nodiscard]] std::optional<std::int64_t> parseNegativeDecimalInteger(
  std::string_view digits) noexcept` sits beside the unchanged `parseDecimalInteger` in
  `src/frontend/lexer/integer_literal.hpp/.cpp`.
  - **Parsing.** It parses the magnitude as `std::uint64_t` with `std::from_chars`, requires full
    consumption, and returns no value on `result_out_of_range` or a magnitude above 2^63.
  - **Negation.** It negates in `std::uint64_t` and converts the result to `std::int64_t`. No
    signed expression in it can overflow. This follows the style guide's Integers rules.
- **Bounds.** Every new lookahead goes through plan 0001's bounds-checked accessor (T2-LEX-6).

### 1.3 Grammar and parser (plan §3.3; D8, D10, D11, D16, D17, D20, D25)
- **Pseudo-calls are syntax.** `pseudoCall := ('echo' | 'typeof') '(' [args] ')'`. A bare `echo`
  or `typeof` is the plan 0001 D27 reserved-word syntax error at the word. Pseudo-functions are
  never values (D8).
- **Argument count.** The parser checks it after it parses the argument list. A count other than
  one is a syntax error at the keyword (D17).
- **`echo` literal arguments.** An integer literal (positive or negative) or a `null` literal as
  the argument of `echo` is a syntax error at the argument's first byte (D16, D17). Every other
  non-`String` argument is checked at runtime.
- **Nesting.** A pseudo-call's argument list is one parser nesting level, exactly like a call's
  (plan 0001 D7; the limit stays 256).
- **Nullable types.** The type grammar is `baseType | '?' nullableBase`.
  - **Adjacency (D25).** The parser checks adjacency from token locations: the next token must
    start on the same line, one column after the `?`. Otherwise it reports a syntax error at the
    `?`. The separated form `? Type` is reserved and has no meaning here.
  - **Token after the `?`.** A token outside `nullableBase`, including `null`, is a syntax error
    at that token (D10).
- **Non-declarable type words.** `ImmutableObject`, `NullObject`, `UnsignedInteger`, and `Float`
  in a type position are syntax errors at the word (D11).

### 1.4 Types and type checking (plan §3.4; D9–D12, D23, D29)
- **AST representation.**
  - `TypeName` (in `src/frontend/ast/ast.hpp`) gains `Object`.
  - The same header gains a new plain aggregate, `struct DeclaredType { TypeName base; bool
    nullable; }`. The parser never produces `nullable` together with `TypeName::Null`.
  - `Param::type()` and `FunctionDecl::returnType()` use `DeclaredType`.
- **Acceptance (D9).** Values of a subtype are accepted (`Object` accepts every non-`null` value).
  `null` is accepted only by `null` or by a `?T` type, and no value is ever converted. This
  replaces plan 0001 D14's exact matching. The timing, order, and locations of the checks are
  unchanged: argument checks run at call time, at the argument, and return checks run when the
  function returns, at the `return`.
- **Single comparison point.** `[[nodiscard]] bool matchesDeclaredType(const Object& value,
  DeclaredType declared) noexcept` in `src/runtime/evaluator/type_check.hpp/.cpp` remains the
  only place that compares a value with a declared type.
- **Script-level names (D12).** `[[nodiscard]] std::string_view scriptTypeName(const Object&
  value) noexcept` is in the same files and returns views of string literals: `"Integer"`,
  `"String"`, `"Function"`, and `"null"`. Both functions use `objectType()`, and `ObjectType` is
  unchanged.
- **Object model.** No class is added or changed. `Object` stays abstract. `Integer` and `String`
  derive from `ImmutableObject`, and `NullObject` and `Function` derive from `Object`. There is no
  `ImmutableObjectInterface` (D19). `null` is the single `NullObject` of each execution context
  (plan 0001 D18).
  - **Developer-approved exception (2026-11-18, §6 item 4).** `String` in
    `src/runtime/object_model/string_object.hpp/.cpp` gains exactly one read-only accessor,
    `[[nodiscard]] std::string_view String::bytes() const noexcept`, so that `echo` can read a
    `String`'s bytes. This is the only exception to plan §3.4 ("No class is added or changed").
    The hierarchy, `ObjectType`, and all behavior are unchanged, and no other class change is
    authorized.
- **Return rules (D29).** This replaces plan 0001 D26 for `?T` functions only.
  - In a `?T` function, a bare `return` is accepted and returns the context's `null`. Falling off
    the end of a `?T` function is still the plan 0001 missing-return runtime error at the
    function name.
  - `null` functions and `main` are unchanged. In `Integer`, `String`, `Function`, and `Object`
    functions, a bare `return` is still the declaration error at the `return`.
- **`main` (D23).** Its return annotation is omitted or `Integer`. Any other annotation, including
  `?Integer`, `Object`, and `?Object`, is the declaration error at the `main` name.

### 1.5 Pseudo-functions in the AST and evaluator (plan §3.5, §3.8; D3, D8, D12–D18, D24)
- **AST nodes.** `EchoCall` and `TypeofCall` are `final` AST node classes. Each holds the
  location of its keyword and one owning `std::unique_ptr<ASTNode>` argument. `NodeKind` gains
  both kinds. Each node implements `detachChildren` and plan 0001's iterative, `noexcept`,
  non-allocating destruction.
- **Validator.** It handles both kinds explicitly by validating the argument. It keeps no casting
  `default`. Undefined identifiers and any use of `main` in the argument are declaration errors.
- **Not a Philios call.** A pseudo-call is not a `Function` and does not count toward the call
  depth. It creates no `Environment` and runs no call-depth guard.
- **Evaluation-depth guard.** Its `evaluate` starts with the evaluation-depth guard, like every
  node (plan 0001 D24), and the guard is located at the keyword (T2-ECHO-8).
- **Argument sequencing (style guide, Reentrancy; plan 0001 D6).** The argument is evaluated once,
  fully, before any check. The result goes into a named local that holds an owning
  `std::shared_ptr` for the rest of the pseudo-call.
- **`echo`.**
  1. Evaluate the argument.
  2. If the value is not a `String`, raise `RuntimeErrorKind::ArgumentType` at the argument's
     first byte. Nothing is written.
  3. Write the bytes through `ExecutionContext::writeOutput`. If that fails, raise
     `RuntimeErrorKind::OutputWrite` at the keyword.
  4. Return the context's `null`.
- **`typeof`.** It accepts any value. It returns a new `String` that holds
  `scriptTypeName(value)`. It reports the runtime type, not the declared type.
- **Errors.** `RuntimeErrorKind` in `src/common/errors.hpp` gains `OutputWrite`. The output-write
  error's message contains the phrase `standard output`, followed by the error code's reason text,
  escaped like any diagnostic text.

### 1.6 Output seam (plan §3.6; D13, D18, D24, D26)
- **Writer type.** `src/runtime/evaluator/script_output.hpp` declares `using OutputWriter =
  std::function<std::error_code(std::string_view bytes)>`. A writer must:
  - write every byte and flush;
  - return an empty `std::error_code` on success and a non-empty one on failure;
  - never report through `errno`;
  - run no Philios code;
  - throw nothing except `std::bad_alloc`.
- **Execution context.** `ExecutionContext` takes the writer in `explicit
  ExecutionContext(OutputWriter output)` and stores it by value. It throws `InternalError` if the
  writer is empty. It exposes `[[nodiscard]] std::error_code writeOutput(std::string_view
  bytes)`, which only `echo` evaluation uses. The context remains non-global and is passed
  explicitly (`AGENTS.md`, constraint 1).
- **Shipped writer.** `src/cli/standard_output.hpp/.cpp` declares `[[nodiscard]]
  std::error_code writeStandardOutput(std::string_view bytes) noexcept`.
  - It calls `std::fwrite` on `stdout` (skipped for zero bytes), then `std::fflush(stdout)`.
  - On a short write or a failed flush, it converts `errno` immediately to a
    `std::generic_category()` code. It uses `std::errc::io_error` when `errno` is 0, then calls
    `std::clearerr(stdout)`.
- **Broken pipes (D26).** `void ignoreBrokenPipeSignal()` calls `std::signal(SIGPIPE, SIG_IGN)`
  and throws `InternalError` on `SIG_ERR`. This changes a signal disposition only. It installs no
  handler and no flag, so the style guide's single-interrupt-flag exception is not used.
- **Wiring.** Before `runProject` in `src/cli/cli.cpp` loads `main.phil`, it calls
  `ignoreBrokenPipeSignal()` and constructs the execution context with `writeStandardOutput`.
- **POSIX confinement.** These files use only `<cstdio>`, `<csignal>`, and other standard
  headers. Plan 0001's POSIX confinement to `src/cli/source_file.cpp` (T-BUILD-6) is unchanged.
- **Output and diagnostics.** Each `echo` writes its bytes exactly and flushes once. There is no
  buffering across `echo` calls and no end-of-program flush step. Every output failure is
  therefore reported at the `echo` that caused it. Diagnostics stay on standard error in plan
  0001's formats.

### 1.7 Test infrastructure (plan §7 tasks 1, 4, 6; §8)
- **Expected stdout.** The CLI driver gains `EXPECT_STDOUT_HEX`, the exact expected stdout in
  lowercase hexadecimal, captured with `OUTPUT_FILE` and compared with `file(READ ... HEX)`. When
  it is unset, stdout must be empty.
- **Stdout modes.** The driver gains `STDOUT_MODE`:
  - `capture` (the default)
  - `dev_full`, which skips through `SKIP_REGULAR_EXPRESSION` when `/dev/full` is absent
  - `closed`, through `sh -c 'exec "$0" >&-' <binary>`
  - `broken_pipe`, which runs two `COMMAND`s in one `execute_process` and takes the binary's
    status from `RESULTS_VARIABLE`
- **Generated modes.** The driver gains `pseudo_nesting` and `broken_pipe_program`.
- **Copy-folder mode (developer decision, 2026-11-19; §6 item 7).** Task 6 adds one driver mode
  that copies every file of a fixture (sample) directory into the fresh `WORK_DIR`, plus the
  matching option in `philios_cli_stdout_test`. The mode only reads its source directory.
- **Fake writer.** A shared capturing fake writer lives in
  `tests/unit/support/capturing_output.hpp/.cpp`. Every test that constructs an
  `ExecutionContext` passes a writer.
- **Framework and quality gates.** Catch2 3.16.1 stays the unit-test framework (plan 0001 D49).
  Every task ends with:
  - a warning-free build;
  - no `clang-tidy` (`cert-*`, `bugprone-*`) diagnostics;
  - a passing suite in the Debug and ASan builds.

### 1.8 Documentation (plan §7 task 6; D27)
- **Source of truth.** This plan, with plan 0001 for the rules it does not change.
- **Scope.** The documentation linked from `docs/index.md`, under `docs/end-users/src/` and
  `docs/developers/src/`, is updated to describe only implemented behavior.
- **Hello World example.** `docs/examples/hello_world/main.phil` changes only if it does not
  already write exactly `Hello World` and exit 1.
- **Files not edited.** `docs/language_rules.md` (deprecated), `AGENTS.md`, and every plan.

## 2. Ownership and lifetime model
| Item | Owner | Notes |
|---|---|---|
| `EchoCall` / `TypeofCall` argument | The node, through `std::unique_ptr<ASTNode>` | Destroyed iteratively through `detachChildren` and the plan 0001 worklist (T2-OWN-2). |
| `DeclaredType` | Stored by value in `Param` and `FunctionDecl` | A plain aggregate with no ownership. |
| Pseudo-call argument value | A named local `std::shared_ptr<Object>` | Held for the whole pseudo-call (style guide, Reentrancy). |
| `typeof` result | A new `String`, returned as `std::shared_ptr<Object>` | A new object each evaluation (T2-TYPEOF-8). |
| `echo` result | The context's single `NullObject` | Plan 0001 D18. |
| `OutputWriter` | `ExecutionContext`, by value | Never empty. The writer keeps no borrowed view of the bytes after it returns. |
| Bytes passed to the writer | Borrowed `std::string_view` of the `String` held by the owning local, obtained through `String::bytes()` (§1.4 exception) | Valid for the synchronous write only, while the owning local keeps the `String` alive. |
| `scriptTypeName` result | A view of a string literal | Static storage. No ownership. |
| `SIGPIPE` disposition | The process (set once by `runProject`) | Not script-visible state. The interpreter starts no thread or child process (plan §5). |

## 3. Trade-offs the plan records
| Decision | Trade-off |
|---|---|
| Flush per `echo` (§3.6) | Output is slower, but every failure has an `echo` location and output order follows program order. Performance remains a non-goal (plan 0001 D34). |
| `SIGPIPE` ignored process-wide (D26) | It changes a process-wide disposition. This is safe because the interpreter is single-threaded and starts no child process. In return, a broken pipe is reported instead of killing the process. |
| Unlimited output volume (§5) | It is accepted under the `AGENTS.md` interim trust model, like unbounded CPU time. |
| Contextual negative literal (D5, D22) | It keeps `a-1`, `x -1`, and `x - -1` available for the future arithmetic plan. In this plan, every non-literal `-` is a syntax error. |
| `? Type` reserved (D25) | The separated form is rejected now so that a future meaning stays open. |
| Runtime check for non-literal `echo` arguments (D17) | It is simpler now. A later plan may move typed-parameter checks before execution. |
| `"Object"` name (D12) | No current value has `Object` as its most specific declarable type, so `scriptTypeName` never returns it. |
| `typeof` allocates a new `String` each time | It matches plan 0001's literal policy and keeps the AST free of runtime objects. |

## 4. Plan 0001 decisions replaced or extended by plan 0002
- **Plan 0001 §3.3 (23 reserved words; `-` an unexpected character).** Replaced by §1.2.
- **Plan 0001 D22 (no negative literals, so `INT64_MIN` cannot be written).** Replaced by D4–D7.
- **Plan 0001 D14 (exact typing).** Replaced by D9 acceptance (§1.4).
- **Plan 0001 D26 (bare `return` in non-`null`, non-`main` functions).** Narrowed by D29 (§1.4).
- **Plan 0001 §4 (nothing written to stdout).** Replaced by D13 and §3.6. The driver's
  empty-stdout check stays the default (§1.7).
- **Plan 0001 `ExecutionContext()` default constructor.** Replaced by the writer-taking
  constructor (§1.6).
- **Everything else.** All other decisions in `plans/0001-architecture_decisions.md` still apply,
  including:
  - the limits 256, 1,000, and 10,000;
  - the completion type for control flow;
  - left-to-right argument evaluation;
  - the top-level handler and the exit statuses 64, 65, 66, and 70;
  - Catch2;
  - explicit `add_test` registration.

## 5. Details left to later stages (not architecture decisions)
The plan does not fix these details. The stub creator and generator choose them within the plan,
`AGENTS.md`, and the style guide. They must not change any behavior the plan states.
- The exact constructors and accessor names of `EchoCall` and `TypeofCall`, and the private
  parser function names for the type grammar and `pseudoCall`.
- How the lexer tracks expression-start context, as long as it follows §3.2's closed token list.
- The internal shape of the capturing fake writer, for example how it records calls, a
  configurable failure code, and observation of the call depth for T2-ECHO-8.
- The names of new CLI fixtures and `ctest` entries, which follow the existing
  `tests/cli/cases/` naming.

## 6. Items for the developer
These are recorded here and in the task breakdown. None changes a language rule. Items 2 and 3
need an answer before the stub-and-test stage of the task they affect. They do not block tasks
2, 3, or 4.

**Status (updated 2026-11-18).** The developer answered items 1, 3, and 4 through a structured
form on 2026-11-18. Item 2 received no explicit answer; its status is recorded below. Each item
keeps its original text, followed by its resolution. Item 6 was raised during task 5 and
answered through a structured form on 2026-11-18.

**Status (updated 2026-11-19).** Items 7 and 8 were raised at the start of task 6 and answered
through a structured form on 2026-11-19. Item 8 is resolved. Item 7 was first partly resolved
(fixture source only); its copy mechanism was answered through a second structured form on
2026-11-19 (option (a)), so item 7 is now **fully resolved** and task 6 is unblocked.
1. **T2-TYPE-9 is listed under task 3 but needs task 5.** Its first program uses `echo` and
   `typeof` and checks stdout `nullnull`. Those exist only after task 5 (and task 1 for
   `EXPECT_STDOUT_HEX`). Task 5 depends on task 3, because T2-ECHO-5 and T2-TYPEOF-2 use `Object`
   and `?String` parameters, so the order cannot be reversed.
   - **What the task breakdown does.** Task 3 implements the D29 behavior and covers every other
     T2-TYPE-9 case. The `nullnull` CLI case is registered in task 5. No test or behavior is
     dropped.
   - **Developer action.** Please confirm this assignment.
   - **Resolved (2026-11-18): confirmed.** Q: "May T2-TYPE-9 stay registered under Task 5?" A:
     Yes. The `nullnull` program stays registered in task 5.
2. **The `philios_cli_test` wrapper is defined in `CMakeLists.txt`.** It parses a fixed keyword
   list and does not forward `EXPECT_STDOUT_HEX` or `STDOUT_MODE`. Plan §7 task 1 names only
   `tests/cli/run_case.cmake` and `tests/cli/cli_cases.cmake`.
   - **What the task breakdown does.** Task 1 must stay within those two files, for example
     through a helper defined in `cli_cases.cmake`.
   - **Developer action.** If that is not workable, the generator must stop and ask whether
     `CMakeLists.txt` may change. Please say now whether a `CMakeLists.txt` change to
     `philios_cli_test` is acceptable.
   - **Status (2026-11-18): no longer blocking; no developer answer recorded.** Task 1 was
     completed within `tests/cli/run_case.cmake` and `tests/cli/cli_cases.cmake` (Stub and
     tests `Done`, Implementation `Done`, Review `GO`), and `CMakeLists.txt` is unchanged. The
     developer has not said whether a `CMakeLists.txt` change would be acceptable, so none is
     authorized. Any later need for one must stop and ask.
3. **T2-DOC-1 working directory.** The plan says the case "runs the shipped example in
   `docs/examples/hello_world/` as its working directory". The current driver deletes and
   recreates `WORK_DIR` before every run (`file(REMOVE_RECURSE "${WORK_DIR}")`). Passing the
   example directory as `WORK_DIR` would therefore delete the shipped example. Task 1 adds no
   driver option for an existing working directory, and task 6 lists only
   `tests/cli/cli_cases.cmake`.
   - **Hard constraint for task 6.** It must never pass `docs/examples/hello_world/` as `WORK_DIR`
     or otherwise modify or delete that directory.
   - **Question for the developer.** Choose one before task 6:
     - (a) Add a non-destructive "run in an existing directory" option to
       `tests/cli/run_case.cmake`. This adds a file to task 6.
     - (b) Satisfy T2-DOC-1 by copying `docs/examples/hello_world/main.phil` (and
       `philios.json`) into a fresh `WORK_DIR`.
     - (c) Something else.
   - **Resolved (2026-11-18): option (b).** Q: "How should T2-DOC-1 run the shipped example?"
     A: "(b) Copy main.phil and philios.json from the example into a fresh working folder".
     - Task 6's T2-DOC-1 case copies `docs/examples/hello_world/main.phil` and
       `docs/examples/hello_world/philios.json` into a fresh `WORK_DIR` and runs there.
     - The hard constraint above still applies: `docs/examples/hello_world/` is never passed as
       `WORK_DIR`, modified, or deleted.
     - Task 6 stays within its listed file, `tests/cli/cli_cases.cmake`. If the copy mechanism
       genuinely needs a change to `tests/cli/run_case.cmake`, the generator must stop and ask
       the developer.
     - The task 6 precondition is resolved.
   - **Update (2026-11-19): see item 7.** The fixture source is now
     `tests/unit/sample-programs/hello-world/`. The existing driver cannot copy both files into a
     fresh `WORK_DIR`, so the task 6 precondition is blocked again until item 7 is answered.
   - **Update (2026-11-19, later): item 7 resolved with option (a).** The task 6 precondition is
     resolved again. Item 3's "stay within `tests/cli/cli_cases.cmake`" limit is superseded by
     item 7's authorized `tests/cli/run_case.cmake` mode; its hard constraint still applies.
4. **How `echo` reads a `String`'s bytes.** Raised during task 5 (see
   `plans/0002-implementation_notes.md`). `String` exposes only `contentEquals`, and plan §3.4
   says no object-model class is changed.
   - **Resolved (2026-11-18).** Q: "How should echo read a String's bytes?" A: "Add a read-only
     accessor: [[nodiscard]] std::string_view String::bytes() const noexcept".
     - This authorizes that single read-only accessor on `String` in
       `src/runtime/object_model/string_object.hpp/.cpp` as an exception to plan §3.4 (see
       §1.4). No other class change is authorized.
     - The hierarchy, `ObjectType`, and behavior are unchanged.
     - The two files are added to task 5's scope, for the accessor only.
5. **Expected preliminary failures.** As in plan 0001, the stub-backed tests of each task fail
   until that task's implementation is done. (Numbered 4 before the 2026-11-18 update.)
6. **The `pseudo_nesting` generator in `tests/cli/run_case.cmake` emitted one extra `)`.**
   Raised during task 5. Task 1 (`GO`) added `MODE pseudo_nesting` (§1.7). Its template
   `echo(${opens}"x"${closes})` added a literal `)` for `echo(`, but `closes` already included
   `echo`'s close. The generated programs therefore had one unbalanced `)`, and the T2-TYPEOF-6
   nesting 255/256 cases failed.
   - **Resolved (2026-11-18).** Q: "How should the run_case.cmake bug be handled?" A: "Fix the
     extra parenthesis in run_case.cmake as part of Task 5, record it as a correction to Task 1,
     then continue".
     - The fix is authorized within task 5's scope as a correction to task 1. It is limited to
       the `MODE pseudo_nesting` branch of `tests/cli/run_case.cmake`.
     - Corrected behavior: `closes` repeats `)` exactly `NESTING - 2` times, which balances the
       `typeof(`s, and the template's literal `)` closes `echo(`. This matches the plan's
       description in §1.7 and plan §7 task 1; no language rule or plan behavior changes.
     - No other part of `tests/cli/run_case.cmake` and no other task 1 behavior may change under
       this authorization. Task 1 keeps its recorded progress.
7. **T2-DOC-1 fixture source and copy mechanism.** Raised at the start of task 6. Item 3 chose
   option (b): copy `main.phil` and `philios.json` into a fresh `WORK_DIR`. The existing driver
   cannot do that:
   - `tests/cli/run_case.cmake` copies only one `.phil` source file. `MODE file` copies
     `FIXTURE` to `main.phil`. `MODE json` writes its own `philios.json` that contains `{`.
   - Neither `philios_cli_test` (`CMakeLists.txt`) nor `philios_cli_stdout_test`
     (`tests/cli/cli_cases.cmake`) nor any `run_case.cmake` mode can place two fixture files in
     the fresh `WORK_DIR`. `FIXTURE` may be a path relative to `tests/cli/cases/`, but only
     `main.phil` is copied.
   - Task 6 may edit only `tests/cli/cli_cases.cmake`, and item 3 says to stop and ask if
     `run_case.cmake` must change.
   - **Developer answer (2026-11-19), verbatim:** "1. I created the `tests\unit\sample-programs`
     directory with `hello-world` as its first program. This is a copy of the `hello_world`
     example program."
   - **Resolved part (2026-11-19): the fixture source.**
     - The answer's backslash path is `tests/unit/sample-programs/hello-world/`. That directory
       exists and holds `main.phil` and `philios.json`, byte-identical to
       `docs/examples/hello_world/` (checked 2026-11-19).
     - T2-DOC-1 uses `tests/unit/sample-programs/hello-world/` as its source fixture instead of
       `docs/examples/hello_world/`.
     - The developer created this directory. No task adds it, and it contains no `.cpp` file, so
       the `tests/unit/*.cpp` glob is unaffected.
     - **Hard constraint.** Item 3's constraint still applies: `docs/examples/hello_world/` is
       never passed as `WORK_DIR`, modified, or deleted. The same applies to
       `tests/unit/sample-programs/hello-world/`.
     - Task 6's documentation scope is unchanged. `docs/examples/hello_world/main.phil` changes
       only under task 6's existing rule.
   - **Open part (as first recorded): the copy mechanism. Was blocking task 6's stub stage;
     resolved below.** The answer does not
     say how both files reach a fresh `WORK_DIR`. That cannot be done with the existing driver
     without editing `tests/cli/run_case.cmake`, which is outside task 6's scope.
   - **Question for the developer.** Choose one before task 6's stub stage:
     - (a) Authorize a minimal change to `tests/cli/run_case.cmake`, for example a mode that
       copies every file of a fixture directory into the fresh `WORK_DIR`, plus the matching
       option in `philios_cli_stdout_test` in `tests/cli/cli_cases.cmake`. This adds
       `tests/cli/run_case.cmake` to task 6's scope.
     - (b) Copy only `main.phil` from the new fixture with the existing `MODE file`, with no
       driver change. This relaxes item 3's "both files" requirement. The case for it: the
       binary never opens `philios.json` (D35, documented in `src/cli/cli.hpp`).
     - (c) Something else.
   - **Developer answer (2026-11-19, second structured form), verbatim:** "(a) Add a
     run_case.cmake mode that copies every file in a sample folder, and add run_case.cmake to
     Task 6 scope".
   - **Resolved (2026-11-19): option (a).**
     - **Authorized driver change.** One new copy-folder mode in `tests/cli/run_case.cmake`
       that copies every file of a fixture (sample) directory into the fresh `WORK_DIR`, and the
       header comment documenting it. No other `run_case.cmake` behavior changes.
     - **Authorized wrapper change.** The matching option in `philios_cli_stdout_test` in
       `tests/cli/cli_cases.cmake` that passes that directory to the driver.
     - **Scope.** Both changes belong to task 6. `tests/cli/run_case.cmake` is added to task 6's
       scope, limited to this mode and its header documentation. `CMakeLists.txt` does not
       change.
     - **T2-DOC-1.** The case uses the new mode with `tests/unit/sample-programs/hello-world/`
       (`main.phil` and `philios.json`) as its source. Expected result unchanged: exit 1,
       stdout hex `48656c6c6f20576f726c64`, empty stderr.
     - **Hard constraint retained.** Neither `tests/unit/sample-programs/hello-world/` nor
       `docs/examples/hello_world/` is ever passed as `WORK_DIR`, modified, or deleted. The new
       mode only reads from its source directory.
     - The task 6 precondition is resolved, and task 6 is unblocked. No language rule or plan
       behavior changes.
8. **`t_parse_5_long_line` run-time limit.** Raised at the start of task 6. The case takes 8 to
   9.3 seconds under ASan. The limit is the hard-coded `TIMEOUT 10` in the `execute_process`
   calls of `tests/cli/run_case.cmake`, so the case is flaky when tests run in parallel. This
   predates plan 0002.
   - **Developer answer (2026-11-19), verbatim:** "2. Increase the limit to 20 seconds. If
     t_parse_5_long_line exceeds that limit, raise the issue with a deeper explantion in the
     implementation notes."
   - **Resolved (2026-11-19).**
     - Raise the run-time limit for `t_parse_5_long_line` from 10 to 20 seconds. Every other CLI
       case keeps 10 seconds. The CTest `TIMEOUT` property (120) does not change.
     - This is a correction to task 1, which owns the `run_case.cmake` driver. The
       implementation-reviser applies it, following item 6's precedent.
     - **Scope.** `tests/cli/run_case.cmake` only: the `TIMEOUT` used for the `long_line` mode,
       and the header comment that mentions `TIMEOUT 10`. `tests/cli/cli_cases.cmake` may change
       only if strictly needed for this. `CMakeLists.txt` does not change. No language rule or
       plan behavior changes.
     - Task 1 gets a correction note and a new review round. Its progress stays `Done`/`Done`/
       `GO` unless the re-review is not `GO`.
     - If the test still exceeds 20 seconds, a deeper explanation must be written in
       `plans/0002-implementation_notes.md` (developer instruction).
