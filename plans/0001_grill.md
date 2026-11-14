# Grill Record for Plan 0001

## Session 2026-11-04

Run by the orchestrator with the developer. All decisions below were confirmed by the developer at
the end of the session. Items marked "interpretation" were confirmed as stated.

### Decisions
| ID | Question | Decision | Rationale | Plan section affected |
|----|----------|----------|-----------|-----------------------|
| D1 | Runtime value model | Plan 0001 creates minimal `Object`, `ImmutableObject`, `Integer`, and `String` classes per AGENTS.md, plus `NullObject` (D18) and `Function` (D16). Plan 0002 extends them. The `RuntimeValue` `std::variant` is removed. | AGENTS.md: everything is an object; built-in storage is private and swappable. Avoids a later migration. | Milestone 4, Ownership & lifetime model, Additional Information 2 |
| D2 | `Integer` bit width | `Integer` is 64-bit signed. Plan adds a task to record this in AGENTS.md. `UnsignedInteger` width stays open. | Settles part of an AGENTS.md Open Decision. | Numeric & exit-status semantics, AGENTS.md impact |
| D3 | Function ownership and the closure cycle | `closureEnv` is a strong (owning) reference. The `Module` explicitly clears its environment at teardown, breaking the module env → function → module env cycle deterministically. | AGENTS.md forbids weakening an owning edge to avoid a cycle. MVP functions are module-level only, so the cycle lives exactly as long as the module. | Ownership & lifetime model |
| D4 | Plan size | Keep one bootstrap plan. Each milestone must be independently buildable and testable. | Bootstrapping the pipeline; plans/AGENTS.md says "generally". | Implementation Milestones |
| D5 | Evaluator control flow for `return` | `evaluate()` returns a small completion/result type. No exceptions for control flow. | Industry standard for tree-walkers; explicit exit paths; faster. Settles an AGENTS.md Open Decision. | Milestone 6, AGENTS.md impact |
| D6 | Argument evaluation order | Left to right; each argument fully evaluated before the next. | Matches Java, JavaScript, Python, PHP. Settles an AGENTS.md Open Decision. Implementation must sequence explicitly (style guide, CERT EXP50). | Language Behavior, AGENTS.md impact |
| D7 | Recursion limits | Parser nesting limit 256 (nested call arguments and blocks); exceeding it is a syntax error. Call depth limit 1,000; exceeding it is a runtime error. Limits are named constants held by the execution context. Replaces the plan's "no call-depth limit" accepted risk. | AGENTS.md requires depth limits; no input may crash the interpreter. Settles the AGENTS.md limit-values Open Decision. | Resource limits & accepted risks, Additional Information 6, AGENTS.md impact |
| D8 | Malformed UTF-8 | Malformed UTF-8 anywhere in source is a lexical error. Columns count bytes. | Matches the AGENTS.md interim default; makes it final. | Syntax baseline |
| D9 | Minimum toolchain | GCC 13+ on Linux x86-64; `clang-tidy` 18+. Other compilers best-effort. | Matches the developer's installed toolchain; covers the C++20 library features the style guide requires. Settles an AGENTS.md Open Decision. | Build & verification, AGENTS.md impact |
| D10 | Test framework | doctest, as a single vendored header, used only by tests and driven by `ctest`. Never linked into the shipped binary. | Header-only, MIT, fast. Settles the AGENTS.md build/test Open Decision (test-only dependencies are allowed). | Build & verification, AGENTS.md impact |
| D11 | Empty blocks | New reserved keyword `pass`: a no-op statement allowed anywhere a statement is. Every block must contain at least one statement; an empty or comment-only block is a syntax error. | Developer choice; resolves the review's empty-body grammar conflict. | Grammar, Syntax baseline, Acceptance Criteria |
| D12 | `main` rules | Exactly one `main`, with zero parameters; return annotation omitted or `Integer`; anything else is rejected before execution. If no `return` executes, `main` returns 0 (the only exception to D13). A bare `return` in `main` is a declaration error. No function may call `main` (declaration error). | Developer choice. | `main()` special rules, Errors |
| D13 | Return requirements | Every non-`main` function whose return type is not `null` must execute `return <value>`; falling off the end is an error. A `null`-returning function may use bare `return`, `return null`, or fall off the end, all of which return `null`. | Developer choice. | Function declaration rules, Acceptance Criteria |
| D14 | Type checks | Exact runtime type match for arguments and return values; no implicit conversion. A `String` is never converted to a function name or callable (unlike PHP). | Developer choice. | Function declaration rules, Errors |
| D15 | Parameter names | A parameter may not share a name with a module function (declaration error). | Avoids ambiguous callee resolution. | Function declaration rules, Errors |
| D16 | Functions as values | Every function is an object of type `Function`, which extends `Object` (interpretation: the developer wrote "extends `Function`"; confirmed as `Object`, matching plan 0003). A function name used without parentheses is a reference to the function object, not a copy and not a call. Functions may be passed as arguments and returned; parameter and return types use the untyped `Function` type. A `Function`-typed parameter may be called by its name. Argument and return checks happen at call time, exactly as for direct calls. Plan 0001 absorbs plan 0003's scope. | Developer choice. | Scope, Grammar, Milestones 3–6, Acceptance Criteria |
| D17 | Function name binding | A function name, once declared, is a constant. | Developer choice (from plan 0003). | Grammar, Errors |
| D18 | `null` | `null` is the singleton instance of `NullObject`, one per execution context (no process-wide globals). `null` is a value usable anywhere an expression is. Interpretation, confirmed: under exact typing (D14), `null` matches only parameters and returns declared as `null` until plan 0002 adds `Object` and `not()`. | Developer choice; aligns with plan 0002. | Grammar (`null` literal in `expr`), Function declaration rules, Additional Information 4 |
| D19 | Command line | `philios` takes no arguments and always uses the current directory. Any argument is a usage error. | Developer choice. | Entry point, Errors |
| D20 | Exit statuses | A script's status is `main`'s `Integer` result modulo 256, computed explicitly so negative values map correctly (for example, -1 → 255). Interpreter failures use BSD sysexits codes: usage 64, lexical/syntax/declaration 65, missing or unreadable `main.phil` 66, runtime error 70, internal error 70. Collisions with script codes are accepted; stderr distinguishes them. | Developer choice. | Numeric & exit-status semantics, Errors |
| D21 | Opening `main.phil` | Open the file first, then check that the opened file is a regular file (no check-then-open race). Symlinked `main.phil` is rejected. Maximum source size is 2 MiB, enforced while reading and before allocating more; larger files are a startup error (66). Not configurable. | Developer choice. | Resource limits, Errors |
| D22 | Negative literals | Deferred with arithmetic. `INT64_MIN` cannot be written as a literal yet; documented limitation. | Developer choice. | Syntax baseline, Numeric semantics |
| D23 | Lexer details | (a) Comments are removed before the `;` continuation check, so `; // note` still continues. (b) A raw newline inside a string literal is a lexical error. (c) An unknown escape is a lexical error. (d) NUL and control characters other than tab, LF, and CR are rejected everywhere, including inside strings and comments. (e) A last line without a trailing newline is accepted; open blocks close at end of file. (f) Tabs are allowed as whitespace outside indentation and inside strings and comments; tabs in indentation remain an error. | Developer choice; resolves the review's lexer findings. | Syntax baseline, Acceptance Criteria |

### Open Decisions from AGENTS.md
| AGENTS.md entry | Decision, or "still open" | Requires an AGENTS.md update? |
|-----------------|---------------------------|-------------------------------|
| Remaining numeric semantics | `Integer` is 64-bit signed (D2). Everything else (division, shifts, mixed types, `UnsignedInteger`) still open; MVP has no arithmetic. | Yes: record the `Integer` width |
| Reclaiming reference cycles | Still open in general. For module-level functions: strong `closureEnv`, cleared at module teardown (D3). | No (consistent with the interim default) |
| Evaluator control flow | Completion/result type (D5) | Yes: mark resolved |
| Evaluation order | Left to right (D6) | Yes: mark resolved |
| Field and map iteration order | Still open; not used by the MVP | No |
| Source encoding and column units | UTF-8, malformed input rejected, columns in bytes (D8) | Yes: mark resolved |
| Import path resolution and import cycles | Still open; imports out of scope | No |
| Concurrency model | Still open; single-threaded default applies | No |
| Trust model, script capabilities, and resource limits | Trust model still open (interim default applies). Source cap 2 MiB (D21). | No |
| Minimum compiler and standard library versions | GCC 13+, `clang-tidy` 18+ (D9) | Yes: mark resolved |
| Build system and test framework | CMake (already in plan), doctest test-only (D10) | Yes: mark resolved; test-only header dependencies allowed |
| Recursion depth limit values | Parser nesting 256, call depth 1,000 (D7) | Yes: record values |

### Contradictions Found
- Plan used the `RuntimeValue` variant removed from AGENTS.md. Resolved by D1.
- Plan's weak `closureEnv` contradicted AGENTS.md's ban on weakening owning edges. Resolved by D3.
- Plan accepted unbounded recursion; AGENTS.md requires depth limits. Resolved by D7.
- Plan's grammar required at least one statement per block while acceptance criteria allowed an empty `main`. Resolved by D11.
- Plan 0001 review recommended rejecting bare function identifiers; the developer instead brought functions as values into scope (D16). Plan 0003 is absorbed.
- Plan's "nullable variable" note conflicted with variables being out of scope. Resolved by D18 (`null` as a value; variables remain out of scope).
- Additional Information item 3 says `main` can take a `List` argument; D12 says `main` takes zero parameters in this plan.

### Open Questions
- Performance and binary-size goals: not blocking. Recommendation: mark as MVP non-goals and measure in a later plan.
- `philios.json` fallback behavior (Additional Information 1): not blocking. Recommendation: keep as a non-normative note about future work.
- Exact diagnostic message wording: not blocking; the composer should define message classes and source-location formats, not exact prose.

### Deferred / Out of Scope
- Anonymous functions and closures over local scopes: v0.1.0 per AGENTS.md.
- Nested function declarations: remain rejected.
- Function signature types (for example `Function(String): String`): later plan.
- `Object` as a declarable type, nullable types, and `not()`: plan 0002.
- Negative integer literals and arithmetic: later plan (D22).
- Command-line project-directory argument and `philios.json`: later plan (D19).
- Fuzzing the lexer and parser: its own plan, per AGENTS.md.

### Answered From the Repository
- C++ standard: strict ISO C++20 with `CMAKE_CXX_STANDARD_REQUIRED ON` and `CMAKE_CXX_EXTENSIONS OFF` (plans/0001.md, Build & verification).
- Static analysis: `clang-tidy` with `cert-*` and `bugprone-*` as errors via the `PHILIOS_CLANG_TIDY` CMake option (plans/0001.md, Build & verification). Verified working with CMake 3.28.3, `clang-tidy` 18.1.3, and GCC 13.3 on 2026-11-04.
- No process-wide mutable globals; the `NullObject` singleton must live in the execution context (AGENTS.md Core Architectural Constraint 1; style guide, Naming & Layout).
- Integer wrapping, `from_chars` parsing, and the `main` catch-all exception chain are governed by the style guide and need no new decision.

### Recommended Changes for the plan-composer
- Restructure the plan into the plan-composer's required structure, keeping existing precise wording.
- Replace the `RuntimeValue` variant with the object model (D1, D16, D18), and add `NullObject` and `Function` to Milestone 4.
- Rewrite the Ownership & lifetime model per D3.
- Replace "no call-depth limit" with D7 everywhere, including Additional Information 6 and Resource limits.
- Update the grammar: `pass` (D11); `null` literal in `expr` (D18); a bare function name as an expression and a callee that can be a `Function`-typed parameter (D16); `Function` and `null` in `typeName` for parameters and returns as applicable (D16, D18).
- Add the `main`, return, typing, and naming rules (D12–D15, D17) and their errors with sysexits statuses (D20).
- Add the CLI and file-loading rules (D19, D21) and the lexer contract (D8, D22, D23).
- Add an AGENTS.md impact section with tasks to record D2, D5, D6, D7, D8, D9, and D10.
- Add D10 (doctest) and D9 (toolchain) to Build & verification.
- Remove or rewrite stale Additional Information items 2, 3, 4, and 6.
- Add acceptance criteria and tests for every decision, including each limit boundary (255/256/257 nesting, 999/1,000/1,001 calls, 2 MiB ± 1 byte), each error class and exit status, symlinked and non-regular `main.phil`, `pass`, function values passed, returned, and called through parameters, `null` handling, and every lexer rule in D23.
- Note in the plan that plan 0003 is absorbed by this plan and needs revision afterwards.

## Session 2026-11-13

The orchestrator relayed this session between the grill agent and the developer. It covered open
questions OQ-1 to OQ-14 in the revised plan. The developer confirmed every decision below with "confirm" and
made no corrections. Developer answers are quoted exactly. Three decisions go against the grill's
recommendation and are marked as developer-accepted risks.

### Decisions
| ID | Question | Decision | Rationale | Plan section affected |
|----|----------|----------|-----------|-----------------------|
| D24 | OQ-1: How should native stack use be bounded? Parser nesting 256 × call depth 1,000 allows about 255,000 nested `evaluate()` frames. | Answers: "A", then "a=10000 (10,000 fixed, verified by T-SEC-4 under ASan and release), b=every (every evaluate() entry), c=distinct (exit 70, call-depth wins, own message class), d=document (document 8 MiB assumption, accept low-ulimit risk). No additional notes." Add a third limit, `kMaxEvalDepth` = 10,000. It is fixed and held by the execution context, with no process-wide globals. An RAII guard counts every `ASTNode::evaluate()` entry, so expressions and call frames share one count. The check runs before any other work, and the count is restored on every exit path, including exceptions. Going over the limit is a runtime error (exit 70) at the line and column of the node being evaluated. It has its own message class, for example `runtime error: evaluation nested too deeply (limit 10000)`; only the class and the location are normative. If both limits would trip, the call-depth error (D7, 1,000) is reported. D5 and D7 do not change. The plan documents that it assumes the Linux default main-thread stack of 8 MiB, and the developer accepts the crash risk under a low `ulimit -s`. T-SEC-4 must pass under ASan and in a release build on the default stack. If it fails at 10,000, the implementer stops and asks the developer. T-SEC-1 must still pass. | Security comes first: no input may crash the interpreter. A depth counter is the industry-standard fix and costs almost nothing. | §3.7, §4 runtime-error row, §5 limits, §6 AGENTS.md impact, §9 T-SEC-1/T-SEC-4, §10 OQ-1 removed |
| D25 | OQ-2: Can `main` be used as a value? | Answer: "A". Any use of the identifier `main` other than its own declaration is a declaration error (exit 65). This covers direct calls (D12) and value references such as `apply(main)` and `return main`. The check runs during whole-file validation before execution and reports the identifier's line and column. It needs no scope analysis (D15). Tests: `main` passed as an argument, returned, and called from another function each exit 65 with no script output. | Extends D12 to function values (D16) without loopholes. | §3.5, §4, §9 |
| D26 | OQ-3: What happens with a bare `return` in a non-`main` function whose return type is not `null`? | Answer: "A". It is a declaration error (exit 65), found by whole-file validation even if the function is never called, at the `return` keyword's line and column. This matches D12. `null`-returning functions keep D13: bare `return`, `return null`, and falling off the end are all allowed. Tests: `Integer` and `String` functions with a bare `return` exit 65 even if never called; a `null` function with a bare `return` runs. | Static rejection is consistent with D12 and catches errors early. | §3.5, §4, §9 |
| D27 | OQ-4: Which words are reserved? | Answer: "B". There are 23 reserved words: `function`, `return`, `null`, `pass`, `Integer`, `String`, `Function`, `Object`, `ImmutableObject`, `NullObject`, `UnsignedInteger`, `Float`, `if`, `elseif`, `else`, `class`, `extends`, `implements`, `interface`, `mixin`, `with`, `export`, `import`. Matching is case-sensitive and whole-token, so `Integer2`, `integer`, and `_if` are legal identifiers. Using a reserved word as an identifier (function name, parameter, or identifier expression) is a syntax error (exit 65) at the word's line and column. T-LEX gets one test per word (23 tests). Each test uses the word as a function name and as a parameter name and checks exit 65, the location, and that there is no output. | Reserving planned words now prevents breaking changes later. | §3.3, §4, §9 T-LEX |
| D28 | OQ-5 (a–e): Which lexer edge cases are errors? | Answers: "a=newline, b=interim, c=interim, d=error, e=interim. No additional notes." Follow-up (Q7): "choice=A (any mix of LF, CRLF, lone CR allowed), items 1 and 2 accepted as proposed. No additional notes." **(a)** LF, CR LF, and lone CR each count as one line break. Each break adds 1 to the line number and resets the column to 1. `CR LF` is one break; `LF CR` is two breaks, so the second line is blank. A lone CR behaves exactly like LF everywhere: it ends a `//` comment, counts as a line inside `/* */` for line numbers and for (b), and ends the line for the `;` continuation check (D23a, D28c). Inside a string literal it is a raw newline and therefore a lexical error (D23b). A file may mix line-ending styles freely. **Developer-accepted risk, against the grill's recommendation (one style per file):** a lone CR can make code look different in some viewers (for example `git diff`, `less`, or `cat` in a terminal) from how the lexer reads it. This allows code hiding, in the same class as Trojan Source. Flag it for the security reviewer; do not ask again. **(b)** A multi-line `/* */` comment ends its logical line at `/*`. On the line with the closing `*/`, only whitespace and comments may follow. Code there is a lexical error at its first byte. **(c)** A `;` continuation followed by EOF, a blank line, or a comment-only line is a lexical error at the `;`. **(d)** An integer literal with a leading zero, other than `0` itself (for example `007` or `00`), is a lexical error at the literal's first byte. **(e)** The bidi formatting characters U+202A–U+202E and U+2066–U+2069 are lexical errors everywhere, including in strings and comments. All of these errors exit 65, and columns count bytes (D8). Each rule gets a T-LEX test. T-LEX-4 covers LF-only, CRLF-only, lone-CR-only, and mixed files; `LF CR` as two breaks; line and column after each style; a lone CR ending a `//` comment; and a lone CR in a string (exit 65). | (b)–(e) make the interim defaults final or reject ambiguous input. For (a), the developer chose broad compatibility and accepted the code-hiding risk. | §3.3, §4 lexical-error row (remove "lone CR (interim)"), §9 T-LEX (T-LEX-3, -4, -5, -6, -8 drop "(interim)"; T-LEX-4 rewritten), §10 OQ-5 removed |
| D29 | OQ-6: Are statements after `return` allowed? | Answer: "A". Any statement after a `return` in the same block is a declaration error (exit 65). This includes `pass` and applies after bare `return`, `return null`, and `return <value>`. Whole-file validation finds it before execution, even in functions that are never called. The error is at the first unreachable statement's line and column. Blank and comment-only lines after a `return` are fine. The rule covers the same block only; full reachability analysis is deferred to the control-flow plans. Tests: an `Integer` function with code after `return 1` exits 65 even if never called; `pass` after `return` exits 65; a comment after `return` runs normally. | Catches dead code early with a simple same-block check. | §3.5, §4 declaration-error row, §9 T-DECL, §10 OQ-6 removed |
| D30 | OQ-7: How are out-of-memory and unexpected exceptions reported? | Answer: "a=70, c=distinct" (field b left at its default, 70; no notes). `std::bad_alloc` exits 70 (EX_SOFTWARE), not 71. Any other unexpected exception (`std::exception` or `catch (...)`) that reaches the top-level handler also exits 70. They have their own message classes, `internal error: out of memory` and `internal error: unexpected exception`, with no source location; only the class is normative. The out-of-memory message is a fixed string literal written without allocating. The top-level handler catches both, never lets an exception escape `main()`, and never calls `std::terminate`. Tests: a test-only hook, compiled only into test builds and not reachable from scripts, forces a `bad_alloc` and an unknown exception. Each test checks exit 70, the message class, and that ASan reports no leaks. | Consistent with D20's internal-error status; no crash on any input. | §4 status table and internal-error row, §8 task 2 (InternalError, top-level handler), §9, §10 OQ-7 removed; §6 only if AGENTS.md lists status mappings |
| D31 | OQ-8: How are undefined identifiers handled? | Answer: "a=A, b=runtime. No notes." **(a)** During whole-file validation before execution, an identifier used as a value or callee is a declaration error (exit 65) at its line and column unless it is a parameter of its enclosing function or a module-level function. This applies to functions that are never called. `main` is handled by D25. There are no block scopes (D15). **(b) Developer-accepted choice, against the grill's recommendation (internal error):** the runtime lookup check stays a normal runtime error (exit 70) at the identifier's line and column, with the existing undefined-identifier runtime message class. There is no fallback lookup. After (a), valid input cannot reach this check, so a unit test that reaches it must build an AST directly. Do not ask again. Tests: a typo in a never-called function exits 65 with no output; an undefined callee exits 65; an undefined value in an argument exits 65; parameter and module-function uses, including forward references, are valid; a parameter that shadows a module function resolves to the parameter (D15). | Static rejection catches typos in code that never runs. | §3.5, §3.7 runtime lookup text, §4 declaration-error and runtime-error rows, §9 T-DECL plus a unit test for the runtime path, §10 OQ-8 removed |
| D32 | OQ-9: Which POSIX file APIs load `main.phil`? | Answer: "A". `main.phil` is loaded only in `src/source_file.cpp`. The code calls `open(O_RDONLY \| O_NOFOLLOW \| O_NONBLOCK \| O_CLOEXEC)`, then `fstat` on the open descriptor (open first, then check, per D21), then `read` (retried on EINTR), then `close`. An RAII class owns the descriptor and closes it on every exit path, including exceptions. No other file may call `open`, `fstat`, `read`, or `close` directly. There is no platform-abstraction layer for now (D9 and the AGENTS.md POSIX assumption). The existing symlink, FIFO, and 2 MiB tests stay. | Race-free and symlink-safe; reading an empty FIFO cannot block. | §3.1, §8 source_file task, §10 OQ-9 removed (interim wording becomes final) |
| D33 | OQ-10: What is the minimum CMake version? | Answer: "A". `cmake_minimum_required(VERSION 3.28)` is final. It matches the CMake that Ubuntu 24.04 packages (D9). There is no version range, the build uses no feature newer than 3.28, and the build commands must work on 3.28. | Matches the supported toolchain. | §7, §8 task 2 (CMakeLists.txt), §10 OQ-10 removed; review-disposition row "build and acceptance not reproducible" drops "CMake minimum Needs developer (OQ-10)" |
| D34 | OQ-11: Does Plan 0001 have performance or binary-size goals? | Answer: "A". Performance and binary-size targets are a non-goal of Plan 0001. Nothing is measured, recorded, or enforced, and no benchmark or size check is a test or acceptance criterion. A later plan sets up benchmarks and targets. | Keeps the bootstrap plan focused. | §2 Non-goals ("(OQ-11)" → "(D34)"), §10 OQ-11 removed; review-disposition row "performance and binary-footprint goals unmeasured": "Needs developer" → "Accepted" (D34) |
| D35 | OQ-12: What happens to the `philios.json` note? | Answer: "A". The `philios.json` note in §2 "Non-normative notes on future work" stays and is explicitly non-normative. Plan 0001 never opens, reads, stats, or checks for `philios.json`, and whether it exists or what it contains has no effect. The entry point is fixed to `main.phil` in the current directory (§3.1). The fallback rule and project configuration belong to a later plan. New T-CLI test: a directory with an invalid `philios.json` (for example `{`) and a valid `main.phil` that uses `pass` exits 0 with nothing on stderr. | Keeps the plan's scope clear and proves the file is ignored. | §2 (note gets "(D35)"; OQ-12 reference removed), §9 T-CLI, §10 OQ-12 removed |
| D36 | OQ-13: How does doctest's static registration fit with `cert-err58-cpp`? | Answer: "A". `tests/unit/.clang-tidy` with `InheritParentConfig: true` disables only `cert-err58-cpp`. A comment in the file cites the rule and the reason (doctest registers tests through static objects), as the AGENTS.md suppression rule requires. This is the only allowed file-level suppression, and registration needs no per-test NOLINT. `src/`, the CLI test scripts, and all other code keep the check. Plan §7 records it as a test-only exception to the style guide's ban on dynamically initialized static objects; the style guide does not change. A T-BUILD test or acceptance check confirms that a dynamically initialized static object in `src/` still fails the clang-tidy build. | Narrowest possible exception that still allows doctest (D10). | §7 (interim wording becomes final), §8 task 2, §9 T-BUILD, §10 OQ-13 removed |
| D37 | OQ-14: Should Plan 0001's language rules be recorded in `docs/language_rules.md`? | Answer: "A". There is no `docs/language_rules.md` update in Plan 0001. Plan 0001 is the only authoritative source for the rules it introduces, which AGENTS.md Document Precedence (line 10) allows. No implementation task is added. **Developer-accepted choice, against the grill's recommendation (B, add a doc-update task).** Do not ask again. | Developer choice; allowed by Document Precedence. | §10 OQ-14 removed |

### Open Decisions from AGENTS.md
| AGENTS.md entry | Decision, or "still open" | Requires an AGENTS.md update? |
|-----------------|---------------------------|-------------------------------|
| Recursion depth limit values | Parser nesting 256, call depth 1,000 (D7), evaluation depth 10,000 (D24) | Yes: record all three values |
| Source encoding and column units | Unchanged from D8. D28 adds lexer rules (line endings, bidi controls) but does not change the entry. | No further change beyond session 1 |
| Trust model, script capabilities, and resource limits | Trust model still open. D24 adds an evaluation-depth limit and documents the 8 MiB stack assumption. | Covered by the recursion-limit row |

No other Open Decisions entry is affected by this session.

### Contradictions Found
- The plan's parser nesting limit (256) times its call-depth limit (1,000) allowed about 255,000 nested `evaluate()` frames, which could overflow the native stack despite AGENTS.md requiring that no input crash the interpreter. Resolved by D24.
- Plan §4's lexical-error row listed a lone CR as an interim error. D28(a) makes a lone CR a line break. The row must change.
- D12 forbade calling `main`, but D16 allowed function values, which left `main` reachable as a value. Resolved by D25.
- The example code in the grill's OQ-6 question did not use the plan's function-declaration syntax. The developer did not comment. The plan-composer must use the plan's existing function-declaration syntax in all examples and tests.

### Open Questions
- None. No blocking items remain.

### Deferred / Out of Scope
- Startup stack-size check (`getrlimit`) or a dedicated big-stack thread: deferred by D24. The developer accepts the risk under a low `ulimit -s`. No target plan.
- Full control-flow reachability analysis: control-flow plans (D29).
- Performance and binary-size benchmarks and targets: a later plan (D34).
- `philios.json` project configuration and the fallback rule: a later plan (D35).
- Moving Plan 0001's language rules into `docs/language_rules.md`: no target plan (D37).
- A platform-abstraction layer for file I/O: not planned. POSIX only (D32).

### Answered From the Repository
- Target platform: AGENTS.md (line 33) assumes POSIX, with Windows supported only through WSL. D9 targets Linux.
- How `main.phil` is opened: by relative name from the current directory (plan §3.1). `O_NOFOLLOW` applies to the final path component, which is the whole relative path.
- CMake version: AGENTS.md gives none. Ubuntu 24.04 (D9) packages CMake 3.28.
- `philios.json`: no §8 task or §9 test reads it (plan §2, lines 65–67).
- clang-tidy: AGENTS.md (line 62) requires `cert-*` and `bugprone-*`, and every suppression needs a comment with the rule and the reason. The style guide (line 38) bans dynamically initialized static objects. CLI tests are CMake scripts, so the D36 exception applies only to C++ unit tests.
- Language rules: AGENTS.md (line 10) makes both `docs/language_rules.md` and approved plans authoritative. `docs/language_rules.md` has 17 short bullets and does not cover Plan 0001's rules.

### Recommended Changes for the plan-composer
- Remove every "(interim)" marker and all of OQ-1 to OQ-14 from §10, replacing each with its decision (D24–D37).
- Add `kMaxEvalDepth` = 10,000, the RAII guard, its error class, the call-depth-wins rule, and the 8 MiB stack assumption to §3.7, §4, and §5. Update §6 so the AGENTS.md recursion-limit task records 256 / 1,000 / 10,000. Make T-SEC-4 require ASan and release builds, plus the rule that the implementer stops and asks the developer if it fails (D24).
- Add the `main`-as-value rule (D25), the bare-`return` rule (D26), the no-statements-after-`return` rule (D29), and the static undefined-identifier rule (D31a) to §3.5 and the §4 declaration-error row, with T-DECL tests. Update the §3.7 runtime lookup text for D31(b) and add a unit test that builds an AST directly.
- Replace the reserved-word list in §3.3 with the 23 words from D27 and add 23 T-LEX tests.
- Rewrite the line-ending rules in §3.3, the §4 lexical-error row, and T-LEX-4 per D28(a). Add the D28(b)–(e) rules and tests.
- Add the out-of-memory and unexpected-exception handling to §4 and §8 task 2, with test-only hook tests (D30).
- Make the POSIX loading sequence and the RAII descriptor in `src/source_file.cpp` final (D32). Set CMake 3.28 (D33).
- Mark performance and size as a non-goal citing D34. Mark the `philios.json` note "(D35)" and add the invalid-`philios.json` T-CLI test.
- Add `tests/unit/.clang-tidy` (D36) to §7, §8 task 2, and a T-BUILD check.
- Change every review-disposition row that says "Needs developer" to "Accepted", citing the deciding decision ID.
- Flag these developer-accepted risks for the security reviewer and the plan reviewer. Do not ask about them again: D28(a) mixed line endings including lone CR (code-hiding risk); D31(b) runtime-error class for an unreachable lookup failure; D37 no `docs/language_rules.md` update; D24 crash risk under a low `ulimit -s`.
- Use the plan's existing function-declaration syntax in all new examples and tests.

## Session 2026-11-13 (follow-up 3: OQ-15, OQ-16)

The orchestrator relayed this session between the grill agent and the developer. It covered the two
non-blocking confirmations OQ-15 and OQ-16 in the revised plan. The developer confirmed the summary with
"confirm" and made no corrections. Developer answers are quoted exactly. Both answers match the grill's
recommendation.

### Decisions
| ID | Question | Decision | Rationale | Plan section affected |
|----|----------|----------|-----------|-----------------------|
| D38 | OQ-15: D15 makes a parameter with the same name as a module function a declaration error, but D31's test list says "a parameter that shadows a module function resolves to the parameter". Options: A keep D15 (recommended), B drop the D15 ban, C remove T-DECL-9. | Answer: "A". Keep D15. A parameter with the same name as a module function is still a declaration error (T-DECL-2). D31's test line only means that name lookup checks parameters first. No valid script can reach that case, so only unit test T-DECL-9 covers it. | Keeps the stricter static rule. The lookup order is still tested at unit level. | No plan change; §10 OQ-15 removed |
| D39 | OQ-16: What concrete form does D24's limit-precedence rule take? Options: A keep the plan's rule (recommended), B report whichever limit is hit first, C other. | Answer: "A" (no notes). Keep the §3.7 / D24 limit-precedence rule as written. When a call expression is entered, the evaluation-depth check fails, and the call depth already equals `kMaxCallDepth`, the call-depth error is reported at the callee identifier. Otherwise, the evaluation-depth error is reported at the node being entered. T-SEC-6 does not change. Add one sentence to §3.7: the rule predicts the call-depth overflow before callee resolution, argument evaluation, and type and arity checks, so in that rare case it can hide a non-Function callee or argument-count error. Both paths exit 70. | Deterministic, matches D24's "call-depth wins", and the side effect is now documented. | §3.7 limit-precedence bullet; §10 OQ-16 removed |

### Open Decisions from AGENTS.md
| AGENTS.md entry | Decision, or "still open" | Requires an AGENTS.md update? |
|-----------------|---------------------------|-------------------------------|
| (none) | No Open Decisions entry is affected by this session. | No |

### Contradictions Found
- D15 (a parameter may not have the same name as a module function) conflicted with the wording of D31's test "a parameter that shadows a module function resolves to the parameter" (plan §9 T-DECL-9 and OQ-15). Resolved by D38: D15 stands, and the D31 wording describes only the lookup order, which is tested at unit level.

### Open Questions
- None. No blocking items remain.

### Deferred / Out of Scope
- None added in this session.

### Answered From the Repository
- T-DECL-9 is already a unit test of local-before-module lookup order (plan §9, line 807), and §8 already lists it. No test change is needed for D38.
- T-SEC-6 already tests the precedence rule (plan §8, line 656). No test change is needed for D39.

### Recommended Changes for the plan-composer
- Add the D39 sentence to the §3.7 limit-precedence bullet: the rule predicts the call-depth overflow before callee resolution, argument evaluation, and type and arity checks, so in that rare case it can hide a non-Function callee or argument-count error. Both paths exit 70.
- Remove OQ-15 and OQ-16 from §10, citing D38 and D39, and update the OQ-15 reference in §9 test 9 to cite D38.
- Make no other changes.

## Session 2026-11-13 (follow-up 4: review H1)

The orchestrator relayed this session between the grill agent and the developer. It covered review
finding H1 from `plans/0001_review.md` and related items. One developer question was asked. The
developer confirmed the final summary with "confirm" and made no corrections. Developer answers are
quoted exactly. The answer matches the grill's recommendation.

### Decisions
| ID | Question | Decision | Rationale | Plan section affected |
|----|----------|----------|-----------|-----------------------|
| D40 | Q1 (review H1): Does an inline comment separate tokens? Options: A a comment always ends a token and works like a space (recommended), B the comment is removed with no gap, C a comment next to a token is an error. | Answer: "A" (no notes). A comment always ends the token before it and works like a space. A comment never counts as indentation, and `;` continuation does not change. `return/*c*/42` lexes as `return` `42` and runs. `fo/*c*/o()` is a syntax error (exit 65). `12/*c*/abc` is a syntax error (exit 65). Each example gets a T-LEX test that checks the exit status and, for errors, the line and column. | Industry-standard behavior (C, C++, Java, JavaScript). It prevents comments from silently joining tokens, so the code a reader sees is the code that runs. | §3.3 Comments and Indentation; §8/§9 T-LEX tests |

Derived from D40 (not a separate developer question; shown in the confirmed summary):
- A line's indentation is only the spaces before its first comment or token. Spaces after a comment are ordinary gaps between tokens.
- `/*c*/    return 1` has indentation 0. In a function body this is a dedent to module level, so `return` is a module-level statement: syntax error, exit 65, reported at the `return` token. If it is the body's only line, the body is empty, which is the D11 error.
- `    /*c*/return 1` has indentation 4 and is valid.
- Lines that hold only comments still have no indentation.
- Both lines get T-LEX tests.

### Open Decisions from AGENTS.md
| AGENTS.md entry | Decision, or "still open" | Requires an AGENTS.md update? |
|-----------------|---------------------------|-------------------------------|
| (none) | No Open Decisions entry is affected by this session. | No |

### Contradictions Found
- Review M2: the plan names "plan 0003" as the absorbed "Functions are Objects" plan, but that plan is `plans/0005.md`. "Plan 0003" is an out-of-date number. Settled from the repository (see below); no developer decision was needed.

### Open Questions
- None. No blocking items remain.

### Deferred / Out of Scope
- Functor and class sugar from plan 0005 ("Functions are Objects"): out of scope for plan 0001. D16 absorbs only functions as values.

### Answered From the Repository
- M2: "Functions are Objects" is `plans/0005.md`. "Plan 0003" in `plans/0001.md` (lines 6–7, 37, 925, 947) is an out-of-date number. D16 absorbs only functions as values; functor/class sugar is out of scope.
- M3: The order is already fixed by §3.7/D6 (a failing argument wins over a non-Function callee and over a wrong argument count) and by §3.2 (a failure after a partial read of the source file is a startup error). Only the tests are missing.
- H2, M1, L1: editing fixes that need no developer decision.

### Recommended Changes for the plan-composer
1. H1 (D40): Add D40 and the start-of-line comment rule to §3.3 (Comments and Indentation). Add T-LEX tests for `return/*c*/42` (exit 0); `fo/*c*/o()` and `12/*c*/abc` (exit 65, checking line and column); and `/*c*/    return 1` (exit 65 at `return`) versus `    /*c*/return 1` (valid).
2. H2: Add `philios_core` to the clang-tidy targets in §7.
3. M1: Move T-CLI-2 through T-CLI-7 and T-PARSE-3 to the evaluator milestone.
4. M2: Replace "plan 0003" with "plan 0005 ('Functions are Objects')" at plan lines 6–7, 37, 925, and 947. State that only functions as values come in and that functor/class sugar is out of scope.
5. M3: Add tests showing that a failing argument wins over a non-Function callee and over a wrong argument count (§3.7/D6), and that a failure after a partial read of the source file is a startup error (§3.2).
6. L1: Reword §3.4 so that it does not seem to ban statements in general.

## Session 2026-11-13 (follow-up 5: source layout and header placement)

The orchestrator relayed this session between the grill agent and the developer. It covered the
High finding of architecture review round 1 in `plans/0001_review.md` (line 270: the flat `src/`
layout contradicts `docs/developers/src/getting-started/directory-structure.md`) and the related
question of where header files go. The developer decided D41 before the questions began and noted:
"I feel like we need to figure out where header files go before we implement." Five developer
questions were then asked (Q1, Q2, Q4, Q5, Q6). Q3 was derived from D42 and shown for objection; the
developer did not object. Each answer matches the grill's recommendation. The developer confirmed the
final summary with "confirm, with item 8 corrected to cite D45 for the line-293 finding". The
corrected item 8 is recorded below. Developer answers are quoted exactly.

**Correction note:** D16 and D17 (session 2026-11-04) and the session-1 Contradictions entry say
"plan 0003"; read this as plan 0005 ("Functions are Objects", `plans/0005.md`). Earlier sessions are
left unchanged.

### Decisions
| ID | Question | Decision | Rationale | Plan section affected |
|----|----------|----------|-----------|-----------------------|
| D41 | Review High finding (line 270): the plan's flat `src/` layout contradicts `directory-structure.md`. | Answer: "Follow documented src/ structure". Plan 0001 follows the `src/` structure in `docs/developers/src/getting-started/directory-structure.md`. The flat `src/` layout in the plan is replaced. | The documented layout already exists as `.gitkeep` placeholders, and the document asks that the structure stay aligned as files are added. Following it avoids moving files in later plans. | §3.1, §3.2, §5 (D32 path), §7 Layout, §8 all tasks, §9 test paths |
| D42 | Q1: Where should project header files live? Options: A next to their source files in the documented directory (recommended), B a separate `include/` tree mirroring `src/`, C public `include/philios/` plus internal headers in `src/`, D other. | Answer: "A". Note: "We can migrate to a separate `includes` directory later. Keeping the header and `.cpp` files together is less likely to confuse the AI." Each `.hpp` file sits next to its `.cpp` file in its `src/` subdirectory. There is no `include/` tree. A separate include directory is postponed, not rejected. | Philios is one standalone binary with no external C++ API consumers (AGENTS.md). Keeping each header next to its source keeps the two together for readers and AI agents. | §7 Layout, §8 task 2 (CMake), all §8 file lists |
| D43 | Q2: How should project files `#include` each other? Options: A one include root at `src/`, full paths from that root in double quotes, no `..` (recommended), B paths relative to the including file, C one include directory per component with bare names, D other. Related rule: double quotes for project headers, angle brackets only for the standard library and third-party headers. | Answer: "A". Quotes vs. angle brackets rule: accepted. There is one project include root, `src/`. Project headers are included by their full path from that root, in double quotes (for example `#include "frontend/lexer/token.hpp"`). Relative `..` paths are not allowed. `src/` is a PRIVATE include directory on `philios_core`; `philios` and `philios_tests` add the same root explicitly. Angle brackets are only for the standard library and third-party headers (doctest, added as SYSTEM). | Full paths are unambiguous and do not change when the including file moves. One root avoids name collisions between components. Common industry practice. | §7 (CMake, HeaderFilterRegex), §8 task 2, coding conventions |
| D44 | Q4: Where does test-only code go? Options: A single-file fakes in an anonymous namespace, shared helpers in `tests/unit/support/`, `tests/unit/` as a second PRIVATE include root on `philios_tests` only, tests mirror `src/`, nothing test-only under `src/` (recommended), B the same but with a flat `tests/unit/`, C no second include root, relative includes, D other. | Answer: "A" (no notes). A fake used by only one test file stays in that file's anonymous namespace, with no header. Shared test helpers go in `tests/unit/support/` as `.hpp`/`.cpp` pairs. `tests/unit/` is a second PRIVATE include root, on `philios_tests` only. `philios_core` and `philios` never get `tests/` include paths or sources. Unit tests mirror the `src/` subdirectories. Nothing test-only lives under `src/`. A CMake check may confirm that `philios` has no `tests/` include path or sources. | Keeps test code out of the shipped binary (§7: `philios` never compiles or links test code) and keeps the two test seams (T-CLI-8, T-RUN-8) supplied only from tests. | §7 Layout, §8 task 2, §9 test paths, T-CLI-8, T-RUN-8 |
| D45 | Q5: Where do the files go that the directory document does not place? Options: A new `runtime/object_model/` and `cli/` directories, `evaluate` definitions in `runtime/evaluator/evaluate.cpp`, `main.cpp` in `src/` (recommended), B no new directories, loose files in `src/runtime/` and `src/`, C `common/` for the leftovers. | Answer: "A" (no per-placement changes). `src/frontend/lexer/`: lexer, `token.hpp`, utf8. `src/frontend/parser/`: parser. `src/frontend/ast/`: AST node classes. `src/analysis/semantic_analyzer/`: validator. `src/runtime/environment/`: environment. `src/common/`: errors. `src/runtime/object_model/` (new): object (`Object`, `ImmutableObject`), integer, string_object, null_object, function. `src/runtime/evaluator/`: `evaluate.cpp` (new; holds the `evaluate` definitions), interpreter, module, execution_context, `completion.hpp`. `evaluate` stays a virtual member of `ASTNode`, declared in `frontend/ast/ast.hpp` with forward declarations; only the definitions move. `src/cli/` (new): cli, source_file, `exit_status.hpp`. D32 now reads "only `src/cli/source_file.cpp` …". `src/main.cpp` sits directly in `src/`. `philios_core` contains every `.cpp` under `src/` except `main.cpp`. Unit tests mirror this layout. | `directory-structure.md` puts runtime values in `runtime/` but names no subdirectory, does not cover the CLI or source loading, and says `common/` is not "a general utility bucket". Moving the `evaluate` definitions to the evaluator removes the frontend's dependency on runtime types. | §3.1, §3.2, §3.9, §5 (D32), §7, §8 tasks 2–8, §9 |
| D46 | Q6: How are D42–D45 recorded in project documentation? Options: A plan 0001 adds a documentation task updating `directory-structure.md` and the style guide's Includes section (recommended), B everything in `directory-structure.md`, C a separate change before plan 0001, D no update. | Answer: "A" (no wording changes). Plan 0001 adds a documentation task, done before or alongside the CMake task. It updates: (1) `directory-structure.md`: add `runtime/object_model/`, `cli/`, and `src/main.cpp` to the tree and the responsibilities list, and add a short "Headers and tests" section with the D42 and D44 rules; (2) the Includes section of `.github/instructions/cpp-style-guide.instructions.md`: one include root `src/`, full paths in double quotes, no `..`, and angle brackets only for the standard library and third-party headers (D43). | Keeps the documentation, which is the source the implementer is told to follow, in step with the plan. | §8 (new documentation task, before or with task 2) |

Derived from D42 (Q3, not a separate developer question; shown in the confirmed summary, no objection):
- There is no separate public-header tier. All project headers are internal, `philios_core` include
  directories are PRIVATE, and tests get the same `src/` root explicitly.

### Open Decisions from AGENTS.md
| AGENTS.md entry | Decision, or "still open" | Requires an AGENTS.md update? |
|-----------------|---------------------------|-------------------------------|
| (none) | No Open Decisions entry covers source layout, header location, or include paths. None is affected by this session. | No |

### Contradictions Found
- The plan's flat `src/` layout (§7, §8 tasks 2–8, D32 path) and the scaffold's `PUBLIC include` CMake
  setup contradict `docs/developers/src/getting-started/directory-structure.md`, and the plan never
  mentions that document. D41–D45 resolve this in favor of the documented structure, extended by D45
  and documented by D46.

### Open Questions
- None. No blocking items remain.

### Deferred / Out of Scope
- A separate `include/` directory (D42): postponed, not rejected; could be revisited later. No target
  plan chosen.

### Answered From the Repository
- What the directory document covers: only directories under `src/` (`frontend/{ast,lexer,parser}`,
  `analysis/semantic_analyzer`, `runtime/{environment,evaluator,virtual_machine}`,
  `backend/bytecode_compiler`, `common`), now only `.gitkeep` placeholders. It says nothing about headers or
  `include/`, names no `runtime/` subdirectory for values or module execution, does not cover the CLI,
  source loading, exit statuses, or `main.cpp`, and says `common/` is not "a general utility bucket"
  (`docs/developers/src/getting-started/directory-structure.md`).
- What the style guide says about includes: `.hpp`/`.cpp` only, snake_case, `#pragma once`,
  include-what-you-use; order is matching header, project headers, standard library, each sorted
  alphabetically. Nothing on header location or include roots
  (`.github/instructions/cpp-style-guide.instructions.md`).
- AGENTS.md says nothing about header location or include paths. Philios is one standalone binary with
  no external C++ API consumers.
- The root `CMakeLists.txt` is a placeholder scaffold ("my-lang", `src/lexer.cpp`, `src/parser.cpp`,
  `target_include_directories(interpreter_core PUBLIC include)`) with no `include/` directory; plan
  §8 task 2 replaces it.
- Plan §7 limits clang-tidy `HeaderFilterRegex` to project headers under `src/` and the test directory,
  and adds doctest as a SYSTEM include directory. `philios_tests` = `tests/unit/` + `philios_core` +
  doctest; `philios` never compiles or links test code.
- The two test-only seams are the `source_file` chunk-reading loop with an injected read operation
  (§3.2, T-CLI-8) and the throwing bodies injected into `runTopLevel` (§3.10, T-RUN-8); both are
  supplied only from `tests/unit/`.
- The plan never mentions `directory-structure.md` or `docs/developers/`.

### Recommended Changes for the plan-composer
1. (D41, D45) Replace every flat `src/` path in the plan with the D45 locations, including §8 tasks,
   §9 test paths, and the D32 wording ("only `src/cli/source_file.cpp` …").
2. (D42, D43) In §8 task 2 (CMake): use a single PRIVATE include root `src/` on `philios_core`; add
   `src/` explicitly to `philios` and `philios_tests`; no `include/` directory and no `PUBLIC include`;
   keep doctest as SYSTEM; build `philios_core` from every `src/**/*.cpp` except `src/main.cpp`.
3. (D43) Add the include-style rule to the plan's coding conventions: full paths from `src/`, double
   quotes, no `..`, angle brackets only for standard library and third-party headers. Check that the §7
   `HeaderFilterRegex` still covers the nested `src/` subdirectories.
4. (D44) Add `tests/unit/` as a PRIVATE include root on `philios_tests` only; mirror the `src/`
   subdirectories under `tests/unit/`; put shared helpers in `tests/unit/support/`; keep single-file
   fakes in the anonymous namespace of their test file; keep the two test seams (the `source_file` read
   loop for T-CLI-8, the `runTopLevel` throwing bodies for T-RUN-8) out of `src/`; optionally add a
   CMake check that `philios` has no `tests/` include path or sources.
5. (D45) Add `src/runtime/evaluator/evaluate.cpp` for the `ASTNode::evaluate` definitions; keep the
   virtual declaration in `src/frontend/ast/ast.hpp` using forward declarations; create
   `src/runtime/object_model/` and `src/cli/`, with `exit_status.hpp` in `src/cli/`.
6. (D46) Add the documentation task, ordered before or alongside the CMake task, to update
   `directory-structure.md` (tree, responsibilities, "Headers and tests" section) and the Includes
   section of the style guide.
7. (Q3, derived from D42) State in the plan that there is no public-header tier and all project
   headers are internal.
8. Link the review findings: the High finding at `plans/0001_review.md` line 270 is resolved by
   D41–D45, and the Medium finding at line 293, item 3 (evaluate defined in ast.cpp makes frontend
   depend on runtime), is resolved by D45 (evaluate definitions move to
   `src/runtime/evaluator/evaluate.cpp`).

## Session 2026-11-14 (follow-up 6: O_NOCTTY)

The orchestrator relayed this session between the grill agent and the developer. It covered the
open security question in Security Review Round 1 of `plans/0001_review.md` ("Open security
questions for the developer": should `O_NOCTTY` be added to the D32 `open` flags?). The plan was at
revision 5 (D1–D46). Developer answers are quoted exactly:
- Q1 "Add O_NOCTTY to D32 open flags?": "yes"
- Next step: "Run plan-composer on remaining findings, then re-review"
- Confirmation of the final summary: "yes"
- "Add a terminal-device test anyway?": "yes". No other corrections.

The orchestrator relayed that the developer left the test mechanism to the plan-composer, within
these limits: the test runs without elevated privileges or reports a clear skip status when the
environment cannot provide a terminal device; test code stays out of `src/` (D44); the test checks
that a terminal-device `main.phil` is rejected and, if feasible, that the process did not gain a
controlling terminal.

**Correction note:** In follow-up 5, D44, the "Answered From the Repository" section, and
"Recommended Changes" item 4 say "T-RUN-8"; read this as T-SEC-8 (the `runTopLevel` test in §9,
Security and limits). Follow-up 5 also says "§3.10"; read this as §4 of `plans/0001.md`
(`runTopLevel` is defined in §4, Errors; the plan has no §3.10). Earlier sessions are left
unchanged.

### Decisions
| ID | Question | Decision | Rationale | Plan section affected |
|----|----------|----------|-----------|-----------------------|
| D47 (changes D32) | Q1: Add `O_NOCTTY` to the D32 `open` flags? Follow-up: add a terminal-device test even though it may need privileges? | Answers: "yes"; test: "yes". The `open` call in `src/cli/source_file.cpp` becomes `open("main.phil", O_RDONLY \| O_NOFOLLOW \| O_NONBLOCK \| O_CLOEXEC \| O_NOCTTY)`. The rest of D32 is unchanged: open first, then check; `fstat`; a bounded `read` loop that retries on `EINTR`; an RAII descriptor; only this file uses the POSIX calls. Add a test in which `main.phil` is a terminal device. It checks that the file is rejected as a non-regular file (startup error, exit 66, per D21 and D32) and, if feasible, that the process did not gain a controlling terminal. The test needs no elevated privileges. If the environment cannot provide a terminal device, the test reports a clear skip status. Test code stays out of `src/` (D44). The plan-composer chooses the mechanism. | Without `O_NOCTTY`, opening a terminal device named `main.phil` can make it the process's controlling terminal before `fstat` rejects it. The developer asked for a test even though the grill agent had recommended none. | §3.2 (POSIX loading sequence), §5 File handling, §8 task 4 or 8, §9 T-CLI, §11, §12, header |

### Open Decisions from AGENTS.md
| AGENTS.md entry | Decision, or "still open" | Requires an AGENTS.md update? |
|-----------------|---------------------------|-------------------------------|
| (none) | No Open Decisions entry covers file-open flags or loading tests. None is affected. | No |

### Contradictions Found
- None. D47 adds one flag to D32 and leaves its order of operations and confinement rule unchanged.
  The test requirement conflicts with no decision. T-BUILD-6 scans only `src/`, so test code under
  `tests/` may call POSIX terminal functions. T-CLI-4 already uses a skip ("skipped when running as
  root"), so a skip status follows existing practice.

### Open Questions
- None. No blocking items remain.

### Deferred / Out of Scope
- The exact test mechanism: left to the plan-composer within the D47 limits.

### Answered From the Repository
- The exact flag set is in plan §3.2 (POSIX loading sequence). §5 File handling and the
  "Areas reviewed" list of Security Review Round 1 also name flags.
- No test covers a device-node `main.phil`. T-CLI-3 covers a directory, a FIFO, and two kinds of
  symlink.
- Skips have a precedent: T-CLI-4 is "skipped when running as root" (§9).
- T-BUILD-6 (§9) checks POSIX headers only under `src/`, so it does not restrict test code.
- Test placement conventions (§8): T-CLI-8 and T-CLI-9 run as unit tests in task 4, and the loading
  CLI tests T-CLI-2 to T-CLI-7 run in task 8. The next free CLI test ID is T-CLI-10.
- Feasibility note for the plan-composer, not a decision: `open` uses the literal relative path
  `main.phil` with `O_NOFOLLOW`, so a symlink to a terminal is rejected before `fstat` and cannot
  test the terminal-device case. Without privileges, a pseudo-terminal slave (from `posix_openpt`
  and `ptsname`) cannot be renamed or hard-linked to `main.phil`, because devpts is a separate file
  system. Ways to put a terminal at `main.phil` include an unprivileged user and mount namespace
  that bind-mounts the slave onto `main.phil` (only where unprivileged user namespaces are enabled)
  or `mknod` when running as root. When neither is available, the test must skip. Checking the
  controlling terminal probably needs the code to run in a child that called `setsid()` and has no
  controlling terminal, for example a forked child in a unit test that calls `loadMainSource` and
  then checks that opening `/dev/tty` fails. The check cannot run after the CLI process has exited.

### Recommended Changes for the plan-composer
1. (D47) §3.2: add `O_NOCTTY` to the `open` flag list, with a one-line reason next to the
   `O_NONBLOCK` note: a terminal device named `main.phil` cannot become the controlling terminal
   before `fstat` rejects it.
2. (D47) §5 File handling: mention `O_NOCTTY`.
3. Header: change the decision range to D1–D47, and list this session (follow-up 6, 2026-11-14) and
   Security Review Round 1 as decision sources.
4. (D47) §11: add a row that resolves the Security Review Round 1 open question on `O_NOCTTY`.
5. §12: add a revision-history entry for this change.
6. (D47) Define and add the terminal-device test. Suggested ID: T-CLI-10, in §9 CLI and loading.
   Place it in §8 according to its form: task 4 for a unit test of `loadMainSource`, or task 8 for a
   CLI golden test. It checks rejection as a non-regular file (startup error, exit 66, message
   naming `main.phil`) and, if feasible, that no controlling terminal was gained. It needs no
   elevated privileges. When the environment cannot provide a terminal device, it reports a clear
   skip status and states the skip condition, as T-CLI-4 does. Test code goes under `tests/` and
   never under `src/` (D44). Any single-file helper stays in its test file's anonymous namespace.
   See the feasibility note in "Answered From the Repository".
7. Fix the follow-up 5 references in any plan text derived from them: T-RUN-8 should be T-SEC-8,
   and §3.10 should be §4. The plan already uses T-SEC-8 and §4 (§7.1, §8 task 2), so this is only
   a check.

## Session 2026-11-14 (follow-up 7: T-CLI-10 skip acceptance)

The orchestrator relayed this session between the grill agent and the developer. It covered the
open security question in Security Review Round 2 of `plans/0001_review.md` ("Open security
questions for the developer": is a skipped T-CLI-10 on Ubuntu 24.04 acceptable as the final check
of D47, or should there be an opt-in run during acceptance?). The plan was at revision 6 (D1–D47).
Developer answers are quoted exactly:
- Q1 "Is a skipped T-CLI-10 on Ubuntu 24.04 acceptable as the final check of D47, or should there
  be an opt-in run during acceptance?": "A skip is acceptable; code review covers O_NOCTTY"
- Q2 "Next step": "One more composer pass on remaining findings, then re-review"
- Confirmation of the final summary: "yes". No corrections.

### Decisions
| ID | Question | Decision | Rationale | Plan section affected |
|----|----------|----------|-----------|-----------------------|
| D48 (refines the test requirement in D47) | Q1: Is a skipped T-CLI-10 on Ubuntu 24.04 acceptable as the final check of D47, or should there be an opt-in run during acceptance? | Answer: "A skip is acceptable; code review covers O_NOCTTY". On the target environment (Ubuntu 24.04), T-CLI-10 may report "skipped" and acceptance still passes. If it skips, code review checks `O_NOCTTY` against the `open` flags in §3.2. The acceptance report must still state, for each build, whether T-CLI-10 passed or was skipped, with the skip reason (wording from Security Review Round 2). No opt-in privileged run is required during acceptance. D47's rule that the test needs no elevated privileges is unchanged. The rest of D47 is unchanged: the flag, the mechanism, and the rule that a test that runs must detect a regression. | Stock Ubuntu 24.04 restricts unprivileged user namespaces through AppArmor, so T-CLI-10 may only be able to skip there. The developer accepts code review as the check in that case instead of loosening the no-privileges rule in D47. Reporting each skip keeps it visible. | §9 introduction and T-CLI-10, §8 task 9, §11, §12, header |

### Open Decisions from AGENTS.md
| AGENTS.md entry | Decision, or "still open" | Requires an AGENTS.md update? |
|-----------------|---------------------------|-------------------------------|
| (none) | No Open Decisions entry covers test skips or acceptance reporting. None is affected. | No |

### Contradictions Found
- None. D48 keeps D47's rules on privileges and skips. It only states that a skip on the target
  environment is acceptable and requires the acceptance report to show it.

### Open Questions
- None. No blocking items remain.

### Deferred / Out of Scope
- An opt-in privileged run of T-CLI-10 (temporarily allowing unprivileged user namespaces, or
  `mknod` as root) during acceptance: not required (D48).

### Answered From the Repository
- The open question and the recommended acceptance-report wording come from Security Review
  Round 2 in `plans/0001_review.md` ("Open security questions for the developer" and the Low "T-CLI-10
  can be silently disabled, and on the target platform it may only ever skip").
- The skip precedent and the no-privileges rule are in D47 (grill follow-up 6) and §9 of
  `plans/0001.md`.

### Recommended Changes for the plan-composer
1. (D48) §9 introduction and the T-CLI-10 entry: state that a skip on the target environment is
   acceptable, and that if T-CLI-10 skips, code review checks `O_NOCTTY` against the §3.2 `open`
   flags.
2. (D48) §8 task 9: the acceptance report states, for each build, whether T-CLI-10 passed or was
   skipped, with the skip reason.
3. (D48) §11: add a row that closes the Security Review Round 2 open question as "Accepted
   (developer decided, D48)". Header: change the decision range to D1–D48 and list this session
   (follow-up 7, 2026-11-14) as a decision source. §12: add a revision-history entry.
4. Address the remaining Medium and Low findings in `plans/0001_review.md`. D48 does not make them
   optional:
   - Plan Review Round 4, both Lows: how the T-CLI-10 child reports its outcome to the parent; and
     record `_exit` in the child as a test-only exception to the style guide's ERR50 rule (`src/`
     never calls `_exit`).
   - Architecture Review Round 3 Low: fix the doctest test-case name in one CMake variable used by
     both `--test-case=` and `--test-case-exclude=`, and make a run that matches zero test cases
     fail.
   - Security Review Round 2, three Lows: the child-to-parent protocol rules (catch-all, a distinct
     exit code per outcome, the skip line printed only for a skip code, drain the pipe before
     waiting, a TIMEOUT, `mkdtemp`); the fix for silent disabling; and the rule that the intrusive
     worklist unlinks a node before destroying it.
   - Plan Review Round 4 Medium: specify the error location for `\` followed by a line break or end
     of file in T-LEX-12.
5. Next step chosen by the developer: one more plan-composer pass on the remaining findings, then
   re-review.
