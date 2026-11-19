# Plan 0002 — Implementation Notes

## Identity
- Plan ID: `0002`
- Plan path: `plans/0002.md`


## Task 1 — CLI driver support for stdout
- Added `philios_cli_stdout_test(...)` in `tests/cli/cli_cases.cmake` with the same argument surface as `philios_cli_test`, plus `STDOUT_HEX` and `STDOUT_MODE`; it forwards through `-DEXPECT_STDOUT_HEX` and `-DSTDOUT_MODE` to `run_case.cmake` and preserves `TIMEOUT`/`SKIP_REGULAR_EXPRESSION` behavior.
- Implemented `EXPECT_STDOUT_HEX` handling in `tests/cli/run_case.cmake` by capturing stdout via `OUTPUT_FILE`, reading with `file(READ ... HEX)`, and exact-comparing against the expected lowercase hex bytes.
- Implemented `STDOUT_MODE` variants:
  - `capture` (default) with stdout checking;
  - `dev_full` writing stdout to `/dev/full` with `<SKIP_LABEL> SKIPPED: /dev/full not available` when absent;
  - `closed` via `sh -c 'exec "$0" "$@" >&-' <binary> [args...]`;
  - `broken_pipe` via two `COMMAND`s in one `execute_process`, using the first entry of `RESULTS_VARIABLE` as the binary status.
- Implemented generated modes:
  - `pseudo_nesting`: `echo(` + `NESTING - 2` nested `typeof(` + `"x"` + closing parentheses.
  - `broken_pipe_program`: recursive `spam(s)` with `echo(s)` at line 2, column 5 and a 65,536-byte `x` string in `main`.
- Manual driver exercises completed (Debug binary): `STDOUT_MODE=dev_full`, `closed`, and `broken_pipe` on `cli1_valid_program.phil`, plus direct runs of `MODE=pseudo_nesting` and `MODE=broken_pipe_program` with `EXPECT_NO_CRASH=TRUE` to confirm the generated-mode paths execute.

## Task 2 — Lexer stubs and preliminary tests
- Kept the earlier run's partial work after checking it against plan §3.2: the 7 new `TokenKind`s in `token.hpp`, the `parseNegativeDecimalInteger` declaration and docs, a placeholder body that returns `std::nullopt`, the updated `Lexer::tokenize` docs, and the T2-LEX-4 tests in `integer_literal_test.cpp`.
- No private lexer helpers were stubbed. How the lexer tracks expression-start context is left to the implementation (architecture decisions, "left to implementation"). The keyword table in `lexer.cpp` (`std::array<Pair, 23>`) still needs the five new words.
- Unit tests added in `lexer_test.cpp`: T2-LEX-5 (literal and `Minus` streams, `?`, the six keywords, plus the §3.2 context examples: a comment, a `;` continuation, `-1/*c*/abc`, `pass -1`, `f(1)-0`, and `-` followed by a tab, `(`, or end of line) and T2-LEX-6 (exact-size storage). The helper `lineTokens` requires a source with static storage, because tokens view the source.
- T-UPD-1: the `-` in the 7 fixtures is now `@`. The T-LEX-2 unit test and the comments in `cli_cases.cmake` were updated to match. T-UPD-2: `t_lex_8_negative_literal` now expects 255 with KIND empty. In `module_test.cpp`, the lexical-error section now uses `return -0`, and a new case checks that `return -1` loads.
- New CLI fixtures/cases: `lex_10_{not,goto,isa,echo,typeof}_as_{function,parameter}_name` (T-UPD-3), `t2_lex_1_*`, `t2_lex_2_*`, `t2_lex_3_*`, `t2_lex_7_question_outside_type`.
- Debug build: compiles with no warnings. Before implementation, these fail as expected: 7 unit test cases (T2-LEX-4/5/6, T-UPD-2), `t_lex_8_negative_literal`, the 10 T-UPD-3 cases, the 7 T2-LEX-1 cases, the 11 T2-LEX-3 cases, and T2-LEX-7. T-UPD-1 cases and T2-LEX-2 cases already pass.

## Task 2 — Lexer implementation
- Implemented `parseNegativeDecimalInteger` in `src/frontend/lexer/integer_literal.cpp` with `std::from_chars` into `std::uint64_t`, a hard cap of `2^63`, and unsigned negation (`0 - magnitude`) before conversion to `std::int64_t` to avoid signed overflow while still accepting `-9223372036854775808`.
- Extended `keywordKind` in `src/frontend/lexer/lexer.cpp` from 23 to 28 entries by adding `not`, `goto`, `isa`, `echo`, and `typeof`, so they lex as reserved keywords instead of identifiers.
- Added lexing for `?` as `TokenKind::Question`.
- Implemented contextual `-` handling in the lexer: when the previously emitted token is `(`, `,`, or `return`, and `-` is immediately followed by `1`–`9`, the lexer emits one negative `IntegerLiteral` token located at the `-`; otherwise it emits `TokenKind::Minus`.
- Preserved plan §3.2 lexical errors at the `-`: `-0*` leading-zero forms, out-of-range negative magnitudes, and malformed `-<digits><identifier-part>` sequences all raise located lexical errors at the minus location.
- Fixed the five new T-UPD-3 parameter-name fixtures to keep the expected syntax-error column (`3:13`) by matching the existing `lex_10_*_as_parameter_name` spacing style (`function f (name: Type)`).

## Task 3 — Types stubs and preliminary tests
- No earlier Task 3 artifacts existed; this is the first run.
- **AST (`ast.hpp/.cpp`).** Added `TypeName::Object` and the plain aggregate `DeclaredType { TypeName base; bool nullable; }` with a defaulted, `noexcept` `operator==` (still an aggregate; tests compare values). `Param` takes and returns `DeclaredType`; `FunctionDecl` takes and returns `std::optional<DeclaredType>`. The parser-enforced invariant (never `nullable` with `Null`) is documented.
- **Type check (`type_check.hpp/.cpp`).** New signature `matchesDeclaredType(const Object&, DeclaredType) noexcept`, documented with the §3.4 table, and new `scriptTypeName(const Object&) noexcept`. Placeholder bodies: `matchesDeclaredType` keeps plan 0001's exact matching on `declared.base`, returns `false` for `Object`, and ignores `nullable`, so the plan 0001 suite keeps running; `scriptTypeName` returns an empty view.
- **Callers adapted to the new types only, with plan 0001 behavior kept** (each marked "Plan 0002 task 3 stub" in a comment where behavior is still pending):
  - `parser.cpp` wraps the plan 0001 type words in `DeclaredType{..., false}`. The §3.3 grammar (`Object`, adjacent `?`, D10/D11/D25 error locations) is not implemented. `Parser::parseProgram` docs describe the new errors. No private helper was stubbed; its name is left to the generator (architecture decisions §5).
  - `validator.cpp` compares with `DeclaredType`. The `main` check is already "omitted or exactly non-nullable `Integer`". The bare-`return` check is still the plan 0001 D26 rule; the D29 `?T` exemption is pending. `validator.hpp` docs describe D23/D29.
  - `call.cpp` uses `DeclaredType`; only `DeclaredType{Null, false}` returns `null` on falling off the end, so `?T`/`Object` already give the missing-return error. `call.hpp` docs updated. `evaluate.cpp` needs no change: a bare `return` already completes with the context's `null`; the D29 return acceptance comes from `matchesDeclaredType`.
- **Test helpers.** `tests/unit/frontend/ast/ast_test.cpp` (`makeFunctionReturning`) now passes `DeclaredType{TypeName::Integer, false}`. `tests/unit/support/ast_builders` builds no `Param`/`FunctionDecl`, so it is unchanged. Existing `parser_test.cpp` comparisons now use `DeclaredType`.
- **Unit tests added.**
  - `tests/unit/runtime/evaluator/type_check_test.cpp` (new): T2-TYPE-6, all 36 pairs (table-driven) plus `scriptTypeName` for the four classes, and `noexcept` checks.
  - `tests/unit/runtime/object_model/object_test.cpp` (new): T2-TYPE-7 `static_assert`s (and `STATIC_CHECK`s so it appears in the report). Passes now, because the object model is unchanged.
  - `tests/unit/frontend/parser/parser_test.cpp`: T2-TYPE-8, nine types as parameter and as return types, plus one mixed parameter list.
  - `tests/unit/analysis/semantic_analyzer/validator_test.cpp`: accepted cases of T2-TYPE-5 (never-called `?Integer` bare `return`), T2-TYPE-9 (bare `return` in each `?T`), and the new types on non-`main` functions.
- **CLI fixtures/cases (39).** `t2_type_1_*` (8), `t2_type_2_*` (2), `t2_type_3_*` (6), `t2_type_4_*` (15; `t2_type_4_parameter_question_tab_integer.phil` contains a literal tab), `t2_type_5_*` (5), `t2_type_9_*` (3), registered after T2-LEX-7 in `tests/cli/cli_cases.cmake`. The T2-TYPE-9 `nullnull` program is **not** registered (task 5, architecture decisions §6 item 1). T2-TYPE-3's "`?Integer` falls off its end" uses `return take(f())`; T2-TYPE-9's uses `f()` as a statement, so the two are distinct programs with the same expected 1:10 error.
- **Build/test status.** Debug and ASan build with no warnings; `clang-tidy` is clean on every touched file. Before implementation, these fail as expected:
  - unit (8 cases): T2-TYPE-6 (both cases with runtime checks), T2-TYPE-8 (3 parser cases, 1 validator case), T2-TYPE-5 and T2-TYPE-9 validator cases;
  - CLI (29): all of `t2_type_1_*`, `t2_type_2_*`, `t2_type_3_*`, `t2_type_5_*`, `t2_type_9_*`, and the T2-TYPE-4 cases expecting column 16 (`parameter_null_after_question`, `parameter_question_right_paren`, `parameter_question_question_integer`, `parameter_question_null_object`, `return_null_after_question`).
  - These already pass: T2-TYPE-7 and the ten T2-TYPE-4 cases expecting column 15 (the current parser already fails at the `?` or the word). Every plan 0001 test and every task 1/2 test passes, including T-NULL-3, T-FN-5, T-DECL-3, and T-DECL-4 (T-UPD-4).
- **Note.** In one parallel ASan `ctest -j` run, `t_parse_5_long_line` hit its 10 s timeout; it passes alone (6.5 s). Task 3 does not touch this case.


## Task 3 — Types implementation
- Implemented `matchesDeclaredType(const Object&, DeclaredType)` in `src/runtime/evaluator/type_check.cpp` to match plan §3.4's acceptance table: exact class checks for `Integer`/`String`/`Function`/`null`, `Object` accepting every non-`null` runtime class, and nullable (`?T`) acceptance by allowing `NullObject` in addition to the base acceptance.
- Implemented `scriptTypeName(const Object&)` in `src/runtime/evaluator/type_check.cpp` using only `Object::objectType()`: `Integer`, `String`, `Function`, and `null`.
- Implemented parser type grammar in `src/frontend/parser/parser.cpp`:
  - added `Object` to base declarable types;
  - added nullable parsing for `?T` via `DeclaredType{base, true}`;
  - enforced D25 adjacency (`?` must be same-line, exactly one column before the next token), reporting non-adjacent cases at the `?` token;
  - for adjacent `?`, enforced nullable-base-only (`Integer`/`String`/`Function`/`Object`) so `?null`, `??`, `?)`, and `?NullObject` fail at the token after `?`;
  - rejected non-declarable words (`ImmutableObject`, `NullObject`, `UnsignedInteger`, `Float`) at the word location.
- Updated function-header parsing so `function f():\n` remains the no-annotation form, while `function f(): <type>:` parses `<type>` with the same declared-type rules used for parameters.
- Implemented validator D29 in `src/analysis/semantic_analyzer/validator.cpp`: a bare `return` is accepted for `null` and any nullable declared return type (`?T`), still rejected for `main` and non-nullable `Integer`/`String`/`Function`/`Object`.
- ASan note: `t_parse_5_long_line` failed once in a full-suite ASan run (known flaky timeout from the stub notes), passed immediately when re-run alone, and a second full ASan suite run passed completely.

## Task 4 — Output seam stubs and preliminary tests
- No earlier Task 4 artifacts existed; this is the first run.
- **Writer type.** New `src/runtime/evaluator/script_output.hpp` declares `using OutputWriter = std::function<std::error_code(std::string_view bytes)>` with the §3.6 contract in its docblock (standard headers only).
- **Execution context.** `ExecutionContext()` was replaced by `explicit ExecutionContext(OutputWriter output)`, which stores the writer by value in the new member `output_` (declared before `null_`), and `[[nodiscard]] std::error_code writeOutput(std::string_view bytes)` was added. Placeholder bodies, marked "Plan 0002 task 4 stub":
  - the constructor stores the writer but does **not** yet throw `InternalError` when it is empty;
  - `writeOutput` does not call the writer and returns `std::errc::function_not_supported`.
- **Errors.** `RuntimeErrorKind::OutputWrite` was added to `src/common/errors.hpp`. `errors.cpp` has no switch over `RuntimeErrorKind`, so it is unchanged. The message text (`standard output` + reason) is built by `echo` evaluation in task 5.
- **Shipped writer.** New `src/cli/standard_output.hpp/.cpp`: `writeStandardOutput(std::string_view) noexcept` (placeholder returns `std::errc::function_not_supported` and writes nothing) and `ignoreBrokenPipeSignal()` (placeholder does nothing). Both are documented with the full §3.6/D26 behavior. Only standard headers are included; T-BUILD-6 passes.
- **CLI wiring (done, because the default constructor no longer exists).** `runProject` now calls `ignoreBrokenPipeSignal()` and builds `ExecutionContext context{OutputWriter{writeStandardOutput}}` before `loadMainSource`. In `cli.hpp`, the `runTopLevel` comment "Nothing is written to stdout" was replaced, and the `runProject` docs describe the new setup. No CLI case's stdout changes, because nothing calls `writeOutput` yet.
- **Fake writer.** New `tests/unit/support/capturing_output.hpp/.cpp` define `philios::test::CapturingOutput`, a complete test helper rather than a stub. It keeps shared state in a `std::shared_ptr`, so a writer stays valid even if it outlives the capture. `writer()` returns a non-empty `OutputWriter` that records each call's bytes and returns the configured result. The capture also has `setResult(std::error_code)`, `bytes()` (all bytes, concatenated), `writes()` (one entry per call), and `callCount()`. Bytes are recorded even when the writer is set to fail. Task 5 (T2-ECHO-8) can reuse it, or extend it if it needs to observe depth inside the writer.
- **Existing tests updated.** Every `ExecutionContext` construction in `evaluate_test.cpp` (8), `module_test.cpp` (5), and `execution_context_test.cpp` (6) now passes `CapturingOutput::writer()`. No other test or source constructs an `ExecutionContext`.
- **Unit tests added (T2-ECHO-9)** in `tests/unit/runtime/evaluator/execution_context_test.cpp`:
  - "T2-ECHO-9 an empty OutputWriter is rejected with InternalError": checks both a temporary empty writer and a named empty writer.
  - "T2-ECHO-9 writeOutput passes the bytes to the writer and returns its result": sections cover exact bytes (including NUL, tab, and UTF-8, one call, empty code), call order including an empty write, a failing writer's `broken_pipe` code returned unchanged, and unchanged depth counters.
- **Build/test status.** Debug and ASan builds produce no warnings, and `clang-tidy` (run in the build) reports no diagnostics. Before implementation, exactly these 2 unit test cases fail as expected: the two T2-ECHO-9 cases. Every other unit case and every CLI/build test passes in Debug, including T-BUILD-6. In ASan, `t_parse_5_long_line` timed out once in a parallel run (known issue) and passed when re-run alone. There were no sanitizer reports.


## Task 4 — Output seam implementation
- `ExecutionContext(OutputWriter)` now rejects an empty writer before constructing runtime state by validating it in a local helper and throwing `InternalError`; `writeOutput(std::string_view)` now forwards directly to the stored writer and returns its exact `std::error_code` unchanged.
- Implemented `writeStandardOutput(std::string_view) noexcept` in `src/cli/standard_output.cpp` to match plan §3.6: optional `fwrite` (skipped for empty input), then `fflush(stdout)`, with failure handling that captures `errno` immediately into a `std::generic_category()` `std::error_code` (falling back to `std::errc::io_error` when `errno == 0`) and calls `std::clearerr(stdout)` before returning.
- Implemented `ignoreBrokenPipeSignal()` with `std::signal(SIGPIPE, SIG_IGN)` and `InternalError` on `SIG_ERR` (D26), with no handler/flag/global state additions and only standard headers in the CLI output module, preserving T-BUILD-6 confinement.
- Verified no Task 4 test edits were needed: existing T2-ECHO-9 unit tests now pass as written.

## Task 4 — Revision for review round 1
- **Low (include ordering), fixed.** In `src/runtime/evaluator/execution_context.cpp`, the project includes now form one alphabetized group: `common/errors.hpp`, `runtime/evaluator/script_output.hpp`, `runtime/object_model/null_object.hpp`. The direct `script_output.hpp` include stays, because the file uses `OutputWriter` (include what you use). There are no other code or test changes.
- **Verification.** Clean rebuilds of `build/` (Debug) and `build-tidy/` produced no warnings and no clang-tidy diagnostics. The full Debug suite passed 337/337.

## Task 5 — stubs and preliminary tests
- No earlier Task 5 artifacts existed (no `EchoCall`/`TypeofCall`, no `t2_echo_*`/`t2_typeof_*` fixtures or cases); this is the first run.
- **AST (`ast.hpp/.cpp`).**
  - `NodeKind` gains `EchoCall` and `TypeofCall`.
  - New `final` classes `EchoCall` and `TypeofCall` with the same shape: `(SourceLocation keyword, std::unique_ptr<ASTNode> argument)`, `argument()` returning `const ASTNode&`, and deleted copy and move. The full §3.5 behavior is documented on each `evaluate`.
  - The ownership plumbing is complete, not stubbed: the null-argument `InternalError`, `kind()`, `detachChildren`, and the iterative destructor that follows the `ReturnStmt` pattern. A placeholder recursive destructor would overflow the stack in T2-OWN-2 and take the whole unit binary down with it.
  - **`ExprStmt` widened (interface change).** It needs this to hold `exprStmt := ( callExpr | pseudoCall )`. It now takes `std::unique_ptr<ASTNode> expression`, which must be a `CallExpr`, `EchoCall`, or `TypeofCall`; anything else, or null, throws `InternalError`. The accessor is `expression()` returning `const ASTNode&`, which replaces `call()`.
  - Callers changed only to fit the new interface:
    - `ExprStmt::evaluate` evaluates `expression()`.
    - The validator's `ExprStmt` branch calls `validateExpression(expression())`; for a `CallExpr` this is the same as before.
    - `parser.cpp` still passes a `std::unique_ptr<CallExpr>`, which converts implicitly.
- **Placeholders, each marked "plan 0002 task 5 stub":**
  - `EchoCall::evaluate` and `TypeofCall::evaluate` in `evaluate.cpp` throw `InternalError`.
  - In `validator.cpp`, `validateExpression` has explicit `EchoCall`/`TypeofCall` cases that throw `InternalError`. The statement switch lists both kinds as invalid statement kinds, which is final behavior: they reach a block only inside an `ExprStmt`. There is still no casting `default`.
  - In `parser.cpp`, the private `ParserImpl::parsePseudoCall()` documents the §3.3 rules (bare word, nesting, count at the keyword, `echo` literal check). Its placeholder throws `SyntaxError` at the keyword. `parseExpression` dispatches `KeywordEcho`/`KeywordTypeof` to it. `parseStatement` parses a pseudo-call, then a `Newline`, and wraps the result in `ExprStmt`.
  - Docs updated: `Parser` (nesting counter, pseudo-call errors) in `parser.hpp`, and `validateProgram` in `validator.hpp`.
- **Unit tests.**
  - `ast_test.cpp`:
    - T2-OWN-2: `static_assert`s that `EchoCall` and `TypeofCall` are nothrow destructible. A 200,000-node `TypeofCall` chain is destroyed three ways: bare, as an `echo` argument inside an `ExprStmt`/`Block`/`FunctionDecl`, and as alternating `EchoCall`/`TypeofCall` nodes inside a `CallExpr`.
    - A node-shape case: keyword location, the argument, a null argument throwing `InternalError`, and the `ExprStmt` kind check.
  - `parser_test.cpp`: two accepted-shape cases.
    - Statement `echo("Hello World")` and statement `typeof(1)` become `ExprStmt` nodes at 2:5 and 3:5.
    - `return typeof(x)`, `echo(typeof(echo("x")))` with keyword columns 10 and 17, `f(typeof(-1))`, `typeof(null)`, and `typeof(f)`.
  - `evaluate_test.cpp`:
    - T2-ECHO-8:
      - exact bytes (line break, tab, `\`, `"`, NUL, UTF-8): one write, and the result is the context's `null`;
      - an empty string writes no bytes (the test does not check whether the writer is called);
      - two echoes write in order;
      - a non-`String` argument (`Integer`, `null`, a `Function`, the inner `echo`'s `null`) raises `ArgumentType` at the argument. A recording argument is evaluated once before the check, and nothing is written;
      - a writer failing with `broken_pipe` raises `OutputWrite` at the keyword, and the message contains `standard output` followed by the code's `message()`;
      - the writer sees `callDepth()` equal to the depth before the echo. It observes the context through a pointer set after construction, inside one `CallDepthGuard`;
      - at `kMaxEvalDepth`, both pseudo-calls fail at the keyword, the argument is not evaluated, and nothing is written.
    - T2-TYPEOF-8: every evaluation returns a new `String` (`Integer` from `-1`, `String`, `null`, `Function`, a call result, `typeof(typeof(1))`), and nothing is written. A recording argument is evaluated exactly once, and both depth counters are 0 afterwards.
  - `CapturingOutput` was reused unchanged. Only the depth case needed its own writer lambda.
- **CLI fixtures and cases (37 fixtures, 44 cases),** registered after T2-TYPE-9 in `tests/cli/cli_cases.cmake`.
  - Stdout checks use `philios_cli_stdout_test ... STDOUT_HEX`. Every case without a hex value still requires empty stdout, which covers "errors before execution leave stdout empty".
  - Coverage by test ID:
    - T2-ECHO-1 to T2-ECHO-7: 26 cases. All three T2-ECHO-6 cases match the message `standard output`:
      - `dev_full` uses `SKIP_LABEL T2-ECHO-6`;
      - `closed`;
      - `broken_pipe` uses `MODE broken_pipe_program` and `FORBID "call depth exceeded"`.
    - T2-TYPEOF-1 to T2-TYPEOF-7: 15 cases. T2-TYPEOF-6 uses `MODE pseudo_nesting` with `NESTING` 255, 256, and 257.
    - T2-TYPE-9 `nullnull`: `t2_type_9_nullnull`, stdout `6e756c6c6e756c6c`.
    - T2-BUILD-1: `t2_build_1_stdout_hex_mismatch` (expects `79`, the program writes `78`) and `t2_build_1_unexpected_stdout`, both `WILL_FAIL TRUE`. They share `t2_echo_6_echo_x.phil`.
  - T2-OWN-1 follows the plan 0001 T-OWN-3 precedent: the named T2-ECHO-1/-4/-5/-6 and T2-TYPEOF-4 cases run in `build-asan`. A comment in `cli_cases.cmake` lists them; no duplicate cases were registered.
  - T2-TYPEOF-3 (`t2_typeof_3_string_result.phil`, exit 4) passes `typeof(1)` to a `String` parameter and returns `typeof(x)` from a `String` function.
- **Build and test status.**
  - Debug, ASan, tidy, and Release build with no warnings and no clang-tidy diagnostics. The `t_build_1`/`t_build_5` clang-tidy tests pass.
  - Debug and ASan: 343/381 pass, with no sanitizer reports. Every plan 0001 test and every task 1–4 test passes.
  - **Expected failures before implementation:**
    - Unit (9 cases in `philios_tests`):
      - the 5 T2-ECHO-8 cases;
      - the 2 T2-TYPEOF-8 cases;
      - the 2 parser pseudo-call shape cases.
    - CLI (37):
      - `t2_echo_1_hello_world`;
      - all `t2_echo_2_*` and `t2_echo_3_*`;
      - `t2_echo_4_{integer,negative,null,max_integer}_literal`;
      - all `t2_echo_5_*` and `t2_echo_6_*`;
      - `t2_echo_7_*`;
      - all `t2_typeof_1/2/3/5/6/7_*`;
      - `t2_typeof_4_{no_arguments,two_arguments,bare_echo_argument,bare_typeof_argument}`;
      - `t2_type_9_nullnull`.
  - **Already pass at the stub stage:**
    - T2-OWN-2 and the node-shape unit case, because the ownership plumbing is structural.
    - `t2_echo_4_{no_arguments,two_arguments,bare_statement,return_bare_echo}` and `t2_typeof_4_return_bare_typeof`, because the placeholder reports at the keyword, which is the expected location.
    - Both T2-BUILD-1 cases, vacuously: the program currently fails, so the cases fail as `WILL_FAIL` requires. They become meaningful once `echo` works.
- **Open question for the developer (blocks Task 5 implementation; does not affect these stubs or tests).** `echo` must pass a `String`'s bytes to `ExecutionContext::writeOutput` as a borrowed `std::string_view` (architecture decisions §2). `String` (`src/runtime/object_model/string_object.hpp`) exposes only `contentEquals`, so its bytes cannot be read. Plan §3.4 and architecture decisions §1.4 say "No class is added or changed", and Task 5 does not list `string_object.hpp/.cpp`.
  - **Question.** May the generator add a read-only accessor, for example `[[nodiscard]] std::string_view String::bytes() const noexcept`, to `string_object.hpp/.cpp`? The hierarchy, `ObjectType`, and behavior would not change. If not, what mechanism should `echo` use?
  - **Until then.** The generator should stop rather than choose.


## Task 5 — implementation
- Implemented pseudo-call parsing in `src/frontend/parser/parser.cpp` (`parsePseudoCall`):
  - `echo(...)`/`typeof(...)` now require `(` immediately after the keyword (bare names error at the keyword);
  - argument lists share call-style parsing and nesting accounting;
  - non-singleton argument counts now report a syntax error at the pseudo-call keyword;
  - `echo` rejects `IntegerLiteral` and `NullLiteral` arguments as syntax errors at the argument location;
  - successful parses build `EchoCall`/`TypeofCall` AST nodes.
- Implemented pseudo-call semantic validation in `src/analysis/semantic_analyzer/validator.cpp` by validating each pseudo-call argument expression recursively (so `main` usage and undefined identifiers in arguments report as declaration errors at the identifier).
- Implemented pseudo-call evaluation in `src/runtime/evaluator/evaluate.cpp`:
  - `EchoCall::evaluate` now applies the evaluation-depth guard at the keyword, evaluates the argument once into an owning `std::shared_ptr<Object>` local, enforces runtime `String` type (`ArgumentType` at the argument), writes exact bytes through `ExecutionContext::writeOutput`, raises `OutputWrite` at the keyword with `standard output` + reason text, and returns the context null object;
  - `TypeofCall::evaluate` now applies the evaluation-depth guard at the keyword, evaluates once into an owning local, and returns a new `String` each time from `scriptTypeName`.
- Implemented the developer-approved Task 5 exception in `src/runtime/object_model/string_object.hpp/.cpp`: added the single read-only accessor `[[nodiscard]] std::string_view String::bytes() const noexcept` and used it from `echo` evaluation.
- Verification results:
  - `cmake --build build` ✅
  - `cmake --build build-asan` ✅
  - `cmake --build build-tidy` ✅ (no clang-tidy diagnostics emitted during the tidy build)
  - `ctest --test-dir build --output-on-failure` ❌ (2/381 failed: `t2_typeof_6_nesting_255`, `t2_typeof_6_nesting_256`)
  - `ctest --test-dir build-asan --output-on-failure` ❌ (same 2 failures; no sanitizer reports)
- Blocking inconsistency found versus plan/task expectations: `MODE=pseudo_nesting` in `tests/cli/run_case.cmake` generates one extra closing `)` (`echo(${opens}"x"${closes})` while `closes` already includes echo's close), so generated 255/256 inputs are syntactically invalid and fail with `expected newline after expression statement` at the extra `)`.
  - Example generated file: `build/cli_cases/t2_typeof_6_nesting_255/main.phil` line 2 has 254 `(` but 255 `)`.
  - This file belongs to Task 1 infrastructure, not Task 5 scope, so Task 5 `Implementation` remains **In progress** pending developer direction.

- **2026-11-18 (Task 5 continuation; correction to Task 1).** Verified the developer-authorized MODE pseudo_nesting correction in tests/cli/run_case.cmake (architecture decisions §6 item 6): typeof_count is NESTING - 2, closes repeats ) exactly typeof_count times, and the template literal ) closes echo(. The explanatory comment in that branch matches this behavior, and no further edits were needed.
- **2026-11-18 verification rerun.**
  - cmake --build build ✅
  - cmake --build build-asan ✅
  - cmake --build build-tidy ✅
  - ctest --test-dir build -j8 ✅ (381/381)
  - ctest --test-dir build-asan --timeout 300 </dev/null ⚠️ pre-existing intermittent t_parse_5_long_line timeout in full-suite ASan runs on this machine; isolated rerun ctest --test-dir build-asan -R t_parse_5_long_line --output-on-failure passed in 8.04 s. This matches the documented long_line timing sensitivity from plan 0001 context; no behavior change made.

## Task 5 — Revision (review round 1)
- **Finding (Low, actionable): Task 5 test lines over 100 columns.** Valid under the C++ style guide, and within Task 5 test scope. Fixed every cited line without changing test behavior:
  - `tests/unit/frontend/ast/ast_test.cpp:143,178`: wrapped the `EchoCall` construction and the `TypeofCall` constructor arguments.
  - `tests/unit/frontend/parser/parser_test.cpp:156`: shortened the `TEST_CASE` name to "Plan 0002 pseudo-call statements parse to ExprStmt holding EchoCall or TypeofCall".
  - `tests/unit/runtime/evaluator/evaluate_test.cpp:304`: shortened the `TEST_CASE` name to "T2-ECHO-8 a String argument is written once with exact bytes and echo returns null".
  - `tests/unit/runtime/evaluator/evaluate_test.cpp:490,495`: reflowed the `checkTypeName(...)` calls.
- **Not changed (outside Task 5 scope):** Other lines over 100 columns remain in the changed files. All of them are in `HEAD` from plan 0001 (for example, `evaluate_test.cpp:132–142,176,185,232`), except `src/frontend/parser/parser.cpp:157` (`parseDeclaredType("expected parameter type")`), which Task 3 added (type annotations), not Task 5. Long CMake case lines follow the existing `cli_cases.cmake` convention. The reviewer did not cite any of these.
- **Verification (2026-11-19):** `cmake --build build`, `build-asan`, and `build-tidy` ✅ with no diagnostics. `ctest --test-dir build -j8` ✅ 381/381. `ctest --test-dir build-asan --timeout 300 </dev/null` ✅ 381/381; the `t_parse_5_long_line` flake did not occur.

## Task 5 — Revision (review round 2)
- **Finding (Low, actionable): missing direct `<system_error>` include.** Valid under the style guide's include-what-you-use rule. Added `#include <system_error>` to the sorted standard-library include group of `src/runtime/evaluator/evaluate.cpp`, between `<string_view>` and `<utility>`. This file uses `std::error_code` at the `echo` write.
- **Audit of the other Task 5 files.** I checked every `std::` symbol that Task 5 added to `ast.hpp/.cpp`, `parser.cpp`, `validator.cpp`, `string_object.hpp/.cpp`, `ast_test.cpp`, `parser_test.cpp`, and `evaluate_test.cpp`. Each one already has its direct standard header. `std::bad_alloc` appears only in documentation comments. No other file changed.
- **Verification (2026-11-19):** `cmake --build build`, `build-asan`, and `build-tidy` ✅ with no warnings and no clang-tidy diagnostics. `ctest --test-dir build -j8` ✅ 381/381. `ctest --test-dir build-asan --timeout 300 </dev/null` ✅ 381/381; the `t_parse_5_long_line` flake did not occur.

## Task 1 — Correction (t_parse_5_long_line time limit)
- **Authority.** Architecture decisions §6 item 8 and the task 1 correction note of 2026-11-19. Developer answer: "Increase the limit to 20 seconds. If t_parse_5_long_line exceeds that limit, raise the issue with a deeper explantion in the implementation notes."
- **Change (`tests/cli/run_case.cmake` only).** Before the `execute_process` calls, the driver sets `run_timeout` to 10, or to 20 when `MODE` is `long_line`. All four `execute_process` calls (`capture`, `dev_full`, `closed`, `broken_pipe`) use `TIMEOUT ${run_timeout}`. Only `t_parse_5_long_line` uses `MODE long_line`, so every other case keeps exactly 10 seconds. The header comment now gives both limits. `tests/cli/cli_cases.cmake` and `CMakeLists.txt` did not change, and the CTest `TIMEOUT` property (120) is unchanged. No other driver behavior changed.
- **Why it predates plan 0002.** In `HEAD` (plan 0001), `run_case.cmake` already has `MODE long_line` and a hard-coded `TIMEOUT 10`, and `cli_cases.cmake` already registers `t_parse_5_long_line ... SIZE 2097152 NO_CRASH`. Plan 0002 did not change that generator or the case.
- **Where the time goes.**
  - Input generation is negligible. The whole driver run with `PHILIOS_BIN=/bin/true` took 0.02 s, including the `string(REPEAT)` generation of the 2,097,152-byte `main.phil`.
  - The time is spent in the binary. I regenerated the same input (`f(` plus 699,035 copies of `1, `, then filler and `1)`) and ran the binary directly. The ASan binary took 5.5 to 6.9 s wall time (5.1 to 6.5 s user) with about 390 MiB peak RSS. The Debug binary took 1.4 s with about 165 MiB. Both runs exit 70 with `main.phil:4:5: runtime error: wrong number of arguments`. Each run lexes, parses, and checks a single call with about 700,000 argument nodes, and ASan instrumentation multiplies the cost of those allocations about 4 to 5 times.
  - Under `ctest`, the serial ASan case took 2.8 to 3.1 s on this run. Under `-j8` load (14 cores), it took 6.3 to 9.0 s. That is why the old 10-second limit was flaky.
- **Measured timings (2026-11-19, after the change).**
  - Debug: `ctest --test-dir build -j8`, 381/381 passed. Serial run of the case: 1.51 s.
  - ASan, full suite `ctest --test-dir build-asan -j8 --timeout 300 </dev/null`, three runs: 381/381 passed each time. The case took 6.34 s, 7.60 s, and 9.03 s (maximum 9.03 s).
  - ASan, serial `ctest --test-dir build-asan -R t_parse_5_long_line --output-on-failure`, four runs: 2.97 s, 2.83 s, 2.84 s, 3.08 s.
  - **No run exceeded 20 seconds.** The worst parallel run used about 45% of the new limit.
- **Builds.** `cmake --build build`, `build-asan`, and `build-tidy` ✅ with no diagnostics.

## Task 6 — stubs and preliminary tests
- **Authority.** Task 6 record (Scope "Copy-folder driver mode", resolved precondition of 2026-11-19), architecture decisions §1.7 and §6 items 3 and 7 (developer option (a)).
- **Earlier runs.** None found: no `copy_folder`/`FIXTURE_DIR` in the driver or wrapper, no T2-DOC-1 case, no Task 6 notes. Task 6 `Stub and tests` was `Pending`. This is the first run.
- **No C++ stubs and no unit tests** (per the task record).
- **`tests/cli/run_case.cmake` (new mode and its header documentation only).**
  - `MODE copy_folder` copies every regular file directly inside `FIXTURE_DIR` into the fresh `WORK_DIR` under the same names, byte for byte (`file(GLOB ... LIST_DIRECTORIES false)` plus `file(COPY_FILE)`). Subdirectories are not copied. An empty source directory fails.
  - `FIXTURE_DIR` (absolute path) is only read. For `MODE copy_folder` only, a check runs **before** `file(REMOVE_RECURSE "${WORK_DIR}")`: the driver fails with a clear message if `FIXTURE_DIR` is unset, missing, or not a directory, or if `WORK_DIR` is the same as, inside, or contains `FIXTURE_DIR` (real paths, component-wise `cmake_path(IS_PREFIX)`). A wrong path therefore can never delete or write into the source.
  - Header comment documents the mode and `FIXTURE_DIR`. No other driver behavior changed (other modes, timeouts, stdout handling, and the final cleanup list are untouched).
- **`tests/cli/cli_cases.cmake`.**
  - `philios_cli_stdout_test` gains `FIXTURE_DIR <dir relative to the source root>`, forwarded as `-DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/<dir>`; documented in the header comment. Configure-time errors: `FIXTURE_DIR` without `MODE copy_folder`, or `MODE copy_folder` without `FIXTURE_DIR`. `WORK_DIR` stays `${CMAKE_CURRENT_BINARY_DIR}/cli_cases/<name>`.
  - T2-DOC-1 case `t2_doc_1_hello_world_example`: `MODE copy_folder FIXTURE_DIR tests/unit/sample-programs/hello-world STATUS 1 KIND empty STDOUT_HEX 48656c6c6f20576f726c64`, placed after the T2-BUILD-1 cases.
- `CMakeLists.txt` and all docs unchanged.
- **T2-DOC-1 already passes** (expected: the sample, byte-identical to `docs/examples/hello_world/`, already writes `Hello World` and exits 1). Its `WORK_DIR` held `main.phil`, `philios.json`, and `stdout.bin` (11 bytes) after the run.
- **Guard checks (manual, against a temporary copy of the sample in `mktemp -d`, never the real directories).** Missing `FIXTURE_DIR`, unset `FIXTURE_DIR`, `WORK_DIR` = source, `WORK_DIR` inside source, and source inside `WORK_DIR` each fail with exit 1 and a `run_case.cmake:` message; a valid run exits 0. The temporary source was unchanged afterwards (`diff -r`).
- **Verification (2026-11-19).**
  - `cmake --build build` and `cmake --build build-asan` ✅ (CMake reconfigured on the `cli_cases.cmake` change; no warnings).
  - `ctest -R t2_doc_1` ✅ in Debug (0.01 s) and ASan (0.04 s).
  - `ctest --test-dir build -j8 </dev/null` ✅ 382/382 (381 before plus T2-DOC-1).
  - `ctest --test-dir build-asan -j8 --timeout 300 </dev/null` ✅ 382/382 (`t_parse_5_long_line` 9.83 s, under its 20 s limit).
  - **Sources unchanged.** SHA-256, file listings, and mtimes of `tests/unit/sample-programs/hello-world/` and `docs/examples/hello_world/` are identical before and after all runs; the two directories are still byte-identical (`cmp`). `git status` shows no change under `docs/` or to `CMakeLists.txt`. (`tests/unit/sample-programs/` is untracked, so it was checked by hash rather than `git status`.)


## Task 6 — implementation
- **Authority.** Task 6 record (Documentation implementation objective, T2-DOC-2 checklist), plan 0002 §2, §3.2–§3.7, §4, §6, and architecture decisions §1.8 and §6.
- **Earlier-run state checked.** Task 6 stub work already existed (`copy_folder` mode, `FIXTURE_DIR`, and `t2_doc_1_hello_world_example`) and remained in place. This run implemented only documentation scope and left stubs/tests behavior unchanged.

### T2-DOC-2 review checklist
- **Every listed file was read; each was updated or recorded as no-change.**
  - `docs/end-users/src/index.md` — **updated**: references now reflect implemented docs; removed planned `echo` and planned "Hello World" entries.
  - `docs/end-users/src/getting-started/first-program.md` — **updated**: replaced old no-output statements, added Hello World (`echo("Hello World")`, exit 1, no trailing newline), and documented modulo-256 negative return mapping (`return -1` -> 255).
  - `docs/end-users/src/getting-started/installation.md` — **no change needed**: task 1–5 behavior did not change these build/install steps.
  - `docs/end-users/src/reference/source-files.md` — **updated**: now states 28 reserved words and which are currently meaningful, negative literal contexts/range/`-0` rule, "every other `-` is syntax error" in this version, and `?` only for nullable type names.
  - `docs/end-users/src/reference/types.md` — **updated**: now includes `Object` and nullable forms, acceptance table from §3.4, `null` acceptance, negative literal range, and reserved non-declarable type names.
  - `docs/end-users/src/reference/functions.md` — **updated**: now covers §3.4/D29 return rules, available types, and `echo`/`typeof` behavior, script-level names, and error behavior.
  - `docs/end-users/src/reference/errors-and-limits.md` — **updated**: removed old "never writes stdout" claim; added plan 0002 §4 conditions by class/exit status including output-write failures and broken-pipe handling; states pseudo-call argument lists count toward the 256 nesting limit.
  - `docs/developers/src/index.md` — **no change needed**: no task 1–5 mismatch in this page's high-level navigation text.
  - `docs/developers/src/getting-started/index.md` — **no change needed**: no task 1–5 mismatch in this page's high-level navigation text.
  - `docs/developers/src/getting-started/ai-usage.md` — **no change needed**: process guidance remained accurate; no task 1–5 behavior drift requiring edits.
  - `docs/developers/src/getting-started/compiling-philios.md` — **updated**: added CLI golden-test stdout options (`EXPECT_STDOUT_HEX`, `STDOUT_MODE`) and task-6 `copy_folder`/`FIXTURE_DIR` driver notes.
  - `docs/developers/src/getting-started/directory-structure.md` — **updated**: added new files/areas introduced by tasks 1–5 (`src/cli/standard_output.*`, `src/runtime/evaluator/script_output.hpp`, `tests/unit/support/capturing_output.*`, `tests/unit/sample-programs/hello-world/`) and output-seam responsibilities.
  - `docs/developers/src/getting-started/required-tools.md` — **updated**: aligned tool requirements with recorded baselines (GCC 13+, clang-tidy 18+, POSIX Bash, Catch2 3.16.1 test-only).
  - `docs/developers/src/cpp-guidelines/index.md` — **updated**: added testing notes for output seam and CLI stdout options (`EXPECT_STDOUT_HEX`, `STDOUT_MODE`).
  - `docs/examples/hello_world/main.phil` — **no change needed**: already writes `Hello World` and exits `1`.

- **Forbidden statements absent (checked in linked end-user pages).** No page now states:
  - Philios cannot write output;
  - literals cannot be negative;
  - types must match exactly;
  - there are 23 reserved words;
  - `-` is always an error.

- **Required statements present (checked against plan/task text).**
  - 28 reserved words and their meaning status.
  - Declarable types and §3.4 acceptance table semantics.
  - D29 return rules (including `?T` bare `return` behavior).
  - `echo` and `typeof` rules per §3.5/§3.6.
  - Negative literal contexts/range per §3.2.
  - New §4 errors with class and exit status, including output-write/broken-pipe outcomes.

- **Examples run as stated (temporary dirs under `/tmp`).**
  - `docs/end-users/src/getting-started/first-program.md` example run with `build/philios`:
    - status `1`
    - stdout hex `48656c6c6f20576f726c64`
    - stderr empty

- **Relative links resolved.** All relative links in changed pages resolved by script (`exists()` checks after path resolution from each page's directory).

- **`docs/language_rules.md` unchanged.** Verified with `git --no-pager diff --quiet -- docs/language_rules.md` (exit `0`).

### Verification
- `cmake --build build` ✅
- `ctest --test-dir build -j8 </dev/null` ✅ `382/382` passed

### Revision — code review round 1 (2026-11-19)
Plan ID `0002`, plan `plans/0002.md`. Findings from `plans/0002-code_review.md`, "Task 6 — Review
round 1 (2026-11-19)". All three are fixed; none conflicts with the plan or needs a decision.

- **High — `copy_folder` symlinked-parent write. Fixed.**
  - `tests/cli/run_case.cmake`, copy_folder check and header docs only (§6 item 7). Before any
    delete or create, the driver resolves `FIXTURE_DIR` with `file(REAL_PATH)`. It walks
    `WORK_DIR` up to its deepest existing ancestor, resolves that with `file(REAL_PATH)`, and
    re-appends the missing components. A missing component that is `.`, `..`, or a dangling
    symlink is rejected. Both `IS_PREFIX` directions are checked, so equal, inside, and
    containing paths are all rejected. (An earlier, stalled revision applied this part; it was
    checked against the reproduction and kept.)
  - `docs/examples/hello_world/` has no separate guard in the driver, so none was extended. No
    case passes it as `FIXTURE_DIR` or `WORK_DIR`.
  - Regression `t2_doc_1_copy_folder_rejects_alias` in `tests/cli/cli_cases.cmake`. At
    configure time it writes `copy_folder_alias_check.cmake` into the build tree; `CMakeLists.txt`
    is unchanged. The check copies the sample to a temporary build-tree directory and makes the
    symlinks `alias -> outer/source` and `outer_alias -> outer`. It then runs `run_case.cmake` for
    seven aliasing cases: a missing `WORK_DIR` (one or two levels) under a symlinked parent,
    `WORK_DIR` equal to the fixture through a symlink, `WORK_DIR` containing the fixture through a
    symlink, a symlinked `FIXTURE_DIR` with a `WORK_DIR` inside its real path, and the inverse
    containment case. Each case must fail with the overlap message and leave the copy's SHA-256
    listing and the symlinks unchanged. A non-overlapping control run through a symlinked
    `FIXTURE_DIR` must pass with `Hello World` and exit 1. The real sample and shipped example are
    never used as either path.
- **Medium — escape list in `source-files.md`. Fixed.** The list is now a table of the source
  spellings `\\`, `\"`, `\n`, and `\t` in inline code, with their meanings. An earlier, stalled
  revision made this change; it was checked and kept.
- **Medium — pseudo-function errors in `functions.md`. Fixed.** The rules section states the
  callable-only rule, the one-argument rule, the `echo` literal-argument rule, and declaration
  errors in arguments (`typeof(main)`, undefined names). The "Pseudo-function Errors" table now
  lists every pseudo-function row of plan §4, including nesting above 256 (syntax, the `(` that
  opens level 257, 65). A new "Detected" column separates the parsing and validation errors
  (exit 65) from `echo`'s runtime argument-type and output-write errors (exit 70).

#### Revision verification
- `cmake build && cmake --build build -j8` ✅
- `ctest --test-dir build -R t2_doc_1 --output-on-failure </dev/null` ✅ 2/2
  (`t2_doc_1_hello_world_example`, `t2_doc_1_copy_folder_rejects_alias`)
- `ctest --test-dir build -j8 </dev/null` ✅ 383/383
- `CMakeLists.txt`, `docs/language_rules.md`, `docs/examples/`, and `AGENTS.md` have no diff. Both
  sample files match `docs/examples/hello_world/` (`cmp`).

### Revision — code review round 2 (2026-11-19)
Plan ID `0002`, plan `plans/0002.md`. Finding from `plans/0002-code_review.md`, "Task 6 — Review
round 2 (2026-11-19)". It is fixed and does not conflict with the plan.

- **Medium — index does not link `echo` or `typeof`. Fixed.** `docs/end-users/src/index.md` now
  lists `echo` and `typeof` under the implemented Functions entry. They link to
  `reference/functions.md#echostring-str-null` and `reference/functions.md#typeofexpr-string`,
  the GitHub-style anchors of the `echo(String str): null` and `typeof(expr): String` headings.
  A script checked that every relative link in the index resolves to an existing file and that
  both anchors match a heading slug. "Planned Topics" is unchanged: `echo` and Hello World are
  still absent, and the other planned topics are kept.

#### Revision verification
- `cmake --build build -j8` ✅
- `ctest --test-dir build -R t2_doc --output-on-failure </dev/null` ✅ 2/2

## Task 7 — End-to-end verification
- Stub stage: no stubs required.
- Earlier-run state checked: Task 7 section existed with only the stub note; no prior Task 7 acceptance report was present.

### Verification run (default stack)
- `ulimit -s` => `8192` (8 MiB default stack).
- Debug (plan 0001 §7 commands):
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` ✅
  - `cmake --build build --clean-first` ✅ (full rebuild; no compiler warnings emitted)
  - `ctest --test-dir build --output-on-failure </dev/null` ✅ `383/383` passed
- ASan (plan 0001 §7 commands):
  - `cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DPHILIOS_SANITIZE=ON` ✅
  - `cmake --build build-asan --clean-first` ✅ (full rebuild; no compiler warnings emitted)
  - `ctest --test-dir build-asan --output-on-failure </dev/null` ✅ `383/383` passed
- Release (plan 0001 §7 commands):
  - `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release` ✅
  - `cmake --build build-release --clean-first` ✅ (full rebuild; no compiler warnings emitted)
  - `ctest --test-dir build-release --output-on-failure </dev/null` ✅ `383/383` passed

### Required acceptance report
- Developer example programs (run through `build/philios`, each in a temp working directory as `main.phil`):
  - `T2-ECHO-1` (`t2_echo_1_hello_world.phil`): stdout `Hello World` (hex `48656c6c6f20576f726c64`), exit `1`.
  - `T2-TYPEOF-1` (`t2_typeof_1_string.phil`): stdout `String` (hex `537472696e67`), exit `1`.
  - `T2-TYPEOF-1` (`t2_typeof_1_integer.phil`): stdout `Integer` (hex `496e7465676572`), exit `1`.
  - `T2-LEX-1` (`t2_lex_1_return_minus_1.phil`): stdout empty, exit `255`.
- `dev_full` case status (`t2_echo_6_dev_full`):
  - Debug: **ran and passed** (not skipped).
  - ASan: **ran and passed** (not skipped).
  - Release: **ran and passed** (not skipped).
- T2-DOC status:
  - `T2-DOC-1` (`t2_doc_1_hello_world_example` and `t2_doc_1_copy_folder_rejects_alias`) passed in Debug, ASan, and Release runs.
  - `T2-DOC-2` review status: **GO** from Task 6 review round 3 (`plans/0002-code_review.md`, "Task 6 — Review round 3 (2026-11-19)").
- T-CLI-10 status (plan 0001 D48):
  - Debug: `t_cli_10_terminal_device_main_phil` **passed** (not skipped).
  - ASan: `t_cli_10_terminal_device_main_phil` **passed** (not skipped).
  - Release: `t_cli_10_terminal_device_main_phil` **passed** (not skipped).
- Warning and clang-tidy status:
  - All three builds were rebuilt cleanly (`--clean-first`) and produced no compiler warnings.
  - `clang-tidy` clean in all three builds: `t_build_1_clang_tidy_all_targets` and `t_build_5_clang_tidy_scope` passed in Debug, ASan, and Release.
- Test counts per build:
  - Debug: `383/383` passed.
  - ASan: `383/383` passed.
  - Release: `383/383` passed.
