# Updated Review of Plan 0001

**Overall verdict:** Go

Reviewed: 2026-11-01. This review supersedes the earlier assessment and compares the current plan with the repository instructions and language documentation.

## Verdict

The plan is substantially more specific and now resolves several major decisions from the first review: integer representation, overflow intent, semicolon meaning, build target, whole-file validation, ownership intent, and the untrusted-source warning. It is **still not fully implementation-ready**. There are direct contradictions inside the plan, and some critical behavior is named but not specified sufficiently for interoperable implementation and tests.

This is a design review, not an implementation audit. Security comments describe proposed-design risks, not confirmed vulnerabilities in an existing interpreter.

## First-review findings: status

| Finding | Status | Current assessment |
|---|---|---|
| Statement terminator ambiguity | Mostly resolved | `;` is declared a continuation marker. Its interaction with trailing comments, indentation, and EOF remains undefined. |
| Integer representation and process status | Mostly resolved | `int64_t` and low-eight-bit exit status are selected. Literal syntax cannot express `INT64_MIN`; failure status values and portable wrapping implementation remain unspecified. |
| Missing grammar | Improved, not resolved | EBNF is present, but it contradicts empty-body acceptance and does not specify continuation/newline token behavior or whether functions are first-class values. |
| Return/fallthrough rules | Partly resolved | `null` behavior now conflicts with no variables being in scope. `main` signature and bare-return behavior are unspecified. |
| Ownership and source-view lifetime | Improved, not implementation-specific | Acyclic ownership and stable source storage are required, but the exact owners/lifetimes and whether closures are needed for this MVP are unclear. |
| Resource exhaustion | Explicit risk accepted, other limits incomplete | No recursion limit is an explicit choice, but nontermination/native-stack crash is a severe known risk. The source-size cap has no value or enforcement rule; parser-depth and AST limits are absent. |
| Calls, binding, type enforcement | Partly resolved | Forward calls and recursion are permitted, but evaluation order, exact typing, call-frame behavior, and function values remain unclear. |
| Error/CLI behavior | Improved, gaps remain | Error classes are listed, but no-main/wrong-main-signature errors, status-code allocation, and several CLI cases are undefined. |
| File-loading security | Partly resolved | Regular-file check, bounded read, and no-sandbox warning are good. Symlink policy, concrete size cap, and read/check race behavior remain open. |
| Build and verification | Partly resolved | CMake/C++20/Linux target and test categories are selected, but no reproducible commands, test cases for all semantics, or performance targets are given. |
| Documentation alignment | Partly resolved | The MVP scope is clear, but Additional Information still describes arrays and nullable variables as if supported. |

## Current findings

### High — The grammar conflicts with accepted empty function bodies

The grammar defines `block := INDENT stmt { stmt } DEDENT`, which requires at least one statement. The acceptance criteria explicitly permit an empty `main` body. Resolve this by allowing an empty block, or by specifying a concrete empty-body syntax and changing the acceptance criterion. Comment-only and blank-only bodies need the same rule.

The grammar also says `returnStmt` and `exprStmt` end at `NEWLINE`, but the lexer defines `;` as continuing a logical statement. Specify whether the parser consumes physical newlines versus logical statement boundaries and how continuation behaves before comments, indentation changes, and EOF. As written, it is not clear how a continued return or call is represented in the grammar.

### High — `main`'s allowed declaration syntax and return behavior are not enforced

The prose requires `main` to take no parameters and always return `Integer`, with an optional return annotation. The grammar allows parameters and any `retType`, including `String` and `null`, for every function. It also permits a bare `return` without defining whether that is valid in `main` or a non-`null` helper.

Specify and validate the exact entry signature: one `main`, zero parameters, and either omitted return annotation or `Integer`; reject all other forms before execution. Define bare `return` for `main`, non-`null` functions, and `null` functions. State whether `return null` is equivalent to bare return only for `null`-returning functions.

The runtime error list does not currently name missing `main` or an invalid `main` signature, despite these being entry validation requirements.

### High — Null semantics contradict the MVP's no-variable scope

The revised plan says a nullable variable may receive a `null` result, but local variables and assignment are explicitly out of scope. The grammar has no assignment statement. Null-valued results therefore have no specified useful storage/use case. Also, the grammar permits `null` as an expression only through `IDENT` or calls, while the literal token `null` is not included in `expr`.

Define `null` as a type and return value only, and state that a `null`-returning call is permitted only as a call statement and its result is discarded; or add variable declarations/assignment to scope and define nullable types. The first choice preserves the stated narrow MVP. Align `expr`, return rules, acceptance criteria, and Additional Information accordingly.

### High — Recursion without any runtime guard allows trivial valid programs to crash

The plan explicitly accepts no interpreter call-depth limit. Because the MVP has no conditionals, arithmetic, mutation, or other termination mechanism, a direct or mutual recursive call cycle cannot make progress toward a base case. A small, valid source file can therefore recurse indefinitely until native-stack exhaustion. This is a material availability/robustness risk, not merely a theoretical risk from hostile input.

The plan may record this as an accepted limitation, but should not imply the interpreter safely handles recursive programs. Prefer an interpreter-managed call-depth limit that produces a controlled runtime error; if the no-limit decision stands, make the likely process crash explicit in acceptance/security notes and never run untrusted source in-process. Also distinguish parser nesting limits from call recursion.

### High — Integer overflow intent needs a defined C++20 implementation rule

`AGENTS.md` now agrees that `Integer` is `std::int64_t` and says arithmetic wraps. The MVP excludes arithmetic, so this is not needed to execute current expressions, but it is a promise for later language operations. Signed overflow in C++ is undefined behavior; simply performing overflowing `int64_t` arithmetic does not implement two's-complement wrapping portably.

Specify that future operations must implement wrapping without signed-overflow UB (e.g. through unsigned arithmetic and a defined bit-preserving conversion), and test boundary cases. Also decide how the minimum value is written: the MVP currently rejects a leading `-`, so `-9223372036854775808` is not representable as a literal even though it is in range. Either explicitly defer negative literals and document this limitation, or define unary-minus/literal parsing that can represent `INT64_MIN`.

The low-eight-bit POSIX exit policy should specify conversion via modulo 256 (including negative results), rather than rely on implementation-defined or platform-specific narrowing. The plan targets Linux, but the language contract should still state this clearly.

### Medium — Identifier expressions ambiguously permit first-class functions

The grammar has `expr := ... | IDENT | callExpr`, and the prose says an identifier resolves to either a parameter or a declared function. That appears to allow a bare function identifier as a value, but first-class functions are not in scope and no function-value type or invocation rule is defined. Clarify that bare `IDENT` expressions resolve only to parameters; function names are legal only as the callee in `callExpr`. Also state that function values, callbacks, and indirect calls are rejected.

### Medium — Call and type semantics still need exact rules

Forward references and direct/mutual recursion are allowed, and duplicate declarations/parameters are errors. Still specify:

- Arguments are evaluated left-to-right or another fixed order.
- Parameters bind in a fresh call-local environment; duplicate names are rejected and no coercion occurs (exact runtime type match).
- A `null` call's result cannot be passed as an argument or returned as a value under the narrow MVP.
- Whether `main` may be called by another function; if permitted, its implicit-zero behavior applies to that ordinary call too.
- Non-`null` functions return on every executed path; since there are no branches, one explicit value return is sufficient only if it is the final executed statement or return exits immediately. Define return as immediate control-flow termination.

The current acceptance criteria mention a helper returning a `String`, but should include a complete example or exact test contract to make clear that its value is consumed in a supported way (for example, passed to another helper that returns `Integer`, if that is allowed).

### Medium — Lexer and source-format rules remain incomplete

The plan adds UTF-8, CRLF normalization, indentation, and common string escapes, but does not state:

- Whether malformed UTF-8 is rejected or preserved, whether columns count bytes or Unicode scalar values, and how tabs are handled outside indentation.
- Whether raw newlines are legal inside quoted strings; behavior for NUL/control characters and unsupported escapes.
- Whether `//` comments can follow code and how comment markers inside strings are treated.
- Whether a line ending in `; // comment` continues the logical statement, since the current test for the last non-whitespace character would see the comment text.
- Whether a final line without a newline is accepted and how EOF emits final NEWLINE/DEDENT tokens.
- How inconsistent dedents, indentation jumps, and spaces after a continued line are diagnosed.

Specify these in a short lexer contract and add focused tests. The EBNF comments use `//`, but that is notation and should not be confused with source syntax.

### Medium — Resource cap is asserted but not actionable

The plan promises a bounded maximum source size but gives no limit, configuration mechanism, or failure classification. It also lacks a maximum token/string length, declaration/token/AST count, or parser nesting bound. Large but bounded files can still cause disproportionate allocations or recursion in the parser.

Choose a default source-byte cap and define whether it can be configured. Add practical token/AST and parser nesting limits with controlled diagnostics. If the no-call-depth-limit decision remains, list that separately as the explicitly accepted exception. Verify limits before allocating large buffers where possible.

### Medium — Startup and security behavior still have edge cases

The plan rejects non-regular files and warns that there is no sandbox. Specify whether symlinked `main.phil` files are allowed; regular-file checks commonly follow symlinks. If an attacker can change the path between checking and opening, check-then-open can be raced. For a local developer CLI this may be an accepted risk, but state the trust assumption and avoid claiming hostile-path safety.

Define behavior for invocation outside the project directory, extra CLI arguments, unreadable file/directory permissions, empty files, and source read errors after a partial read. Specify exact reserved interpreter failure statuses (or at least a stable mapping by error class). Diagnostics are said to escape control characters, but define whether source excerpts are printed at all and ensure filenames/excerpts are escaped as well.

### Medium — Build and acceptance requirements are still not reproducible

The platform and tool choices are now clear (C++20, Linux x86-64/WSL, CMake), but “`ctest`-driven” and “sanitizers where available” do not provide reproducible verification. Specify configure/build/test commands, minimum CMake version if relevant, compiler baseline, sanitizer configuration, and what “standalone” means regarding dynamically linked system libraries.

The current criteria should add cases for invalid/missing `main`, invalid main signature, duplicate definitions/parameters, forward references, nested calls, exact parameter/return type checks, unknown escapes, malformed UTF-8 policy, continuation/comment/EOF handling, integer boundary parsing, and controlled recursion/resource-limit behavior. Performance and binary-footprint goals from `AGENTS.md`/README remain unmeasured; either mark them as non-MVP quality goals or define representative measurements.

### Low — Additional Information is now stale and conflicts with scope

The entry paragraph correctly defers `philios.json` and fixes `main.phil`, but uses “later” fallback behavior as an uncommitted future requirement. Keep it as a note, not a normative promise, unless the fallback behavior is intended. More importantly, the example-alignment and null-return notes say `main()` may take an array and nullable variables may be assigned, despite arrays, variables, and assignment being out of scope. Reword these as deferred features, or remove them from the MVP plan.

The `Integer` variant addition is consistent with the current [AGENTS.md](AGENTS.md). C++20 is consistent with its C++17/20 range. The single-file `Module`/private `Environment` architecture is consistent with module isolation; make clear that no imports/exports or cross-file sharing are implemented in this milestone.

## Recommended next changes

Resolve the five high-priority items before implementation: empty-block grammar, exact `main` signature/bare-return rules, null behavior without variables, explicit recursion risk or a limit, and portable integer/exit conversion. Then specify call binding/evaluation order, lexer EOF/comments/UTF-8 details, concrete resource limits, symlink/CLI behavior, and reproducible test commands.

**Bottom line:** The updated plan has improved substantially, and the earlier review's broad concerns are no longer all open. The remaining blockers are now precise contract mismatches—especially empty blocks, `main`, `null`, and unbounded recursion—rather than a lack of overall structure.

## Plan Review

The content above this section is the legacy review from 2026-11-01. It is kept unchanged as history.

### Round 1 (2026-11-13), revision 3, recorded after the fact

Confirmed plan ID: `0001`. Plan path: `plans/0001.md` (revision 3, decisions D1–D39).

This round was written down later. The reviewer of revision 3 had no edit tool, so the findings were passed to the plan-composer by the orchestrator and were not written here at the time. The findings below are copied from that relay and are not re-reviewed here.

**Plan verdict:** No-go (0 Critical, 2 High, 3 Medium, 1 Low)

#### Findings
##### High: Inline comments and token separation are not specified (H1)
- **Plan reference:** §3.3 Comments
- **Problem:** The plan did not say whether an inline `/* */` comment splits tokens. For example, it was unclear whether `return/*c*/42` and `fo/*c*/o()` are one token or two, and whether a comment counts as indentation.
- **Recommendation:** Get a developer decision and record it in §3.3, with tests.

##### High: `clang-tidy` not enabled on `philios_core` (H2)
- **Plan reference:** §7 Static analysis
- **Problem:** `CXX_CLANG_TIDY` was set only on `philios` and `philios_tests`, so the `src/` files compiled into `philios_core` were not analyzed. AGENTS.md Verification requires `clang-tidy` on all code.
- **Recommendation:** Set `CXX_CLANG_TIDY` on all three targets and check this in T-BUILD-1.

##### Medium: CLI tests scheduled before the evaluator milestone (M1)
- **Plan reference:** §8 tasks 4 and 6
- **Problem:** T-CLI-2 to T-CLI-7 (task 4) and T-PARSE-3 (task 6) need the full pipeline. This conflicts with D4: each milestone must build and test on its own.
- **Recommendation:** Move these tests to task 8.

##### Medium: Wrong plan number for "Functions are Objects" (M2)
- **Plan reference:** Header, §2, §11, Revision History
- **Problem:** The plan cited "plan 0003" as "Functions are Objects", but that plan is `plans/0005.md`. `plans/0003.md` covers variables and operators.
- **Recommendation:** Cite plan 0005, and say which parts of it are absorbed.

##### Medium: Missing tests (M3)
- **Plan reference:** §3.2, §3.7, §9
- **Problem:** No test covered a read failure after a partial read. No test covered a failing argument winning over a bad callee or a wrong argument count (D6 order).
- **Recommendation:** Add both tests.

##### Low: §3.4 wording appears to ban statements everywhere (L1)
- **Plan reference:** §3.4 module-level rule
- **Problem:** The sentence read as if statements were syntax errors in general, not only at module level.
- **Recommendation:** Reword it.

#### Open questions for the developer
- H1 needed a developer decision. It was answered later as D40.

#### Notes for the secondary reviewers
- Not recorded in the relay.

#### Areas reviewed
- As relayed. The details were not recorded.

### Round 2 (2026-11-13)

Confirmed plan ID: `0001`. Plan path: `plans/0001.md` (revision 4, decisions D1–D40). Review path: `plans/0001_review.md`. Grill record: `plans/0001_grill.md`.

**Plan verdict:** Go

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| R1 H1: Inline comments and token separation | Resolved | §3.3 now states D40 and the start-of-line comment rule derived from it. §4 has a syntax-error row for this. T-LEX-11 covers all five grill examples. I checked the line and column numbers in every example. |
| R1 H2: `clang-tidy` not on `philios_core` | Resolved | §7 sets `CXX_CLANG_TIDY` on all three targets, and T-BUILD-1 checks this. |
| R1 M1: Tests scheduled before the evaluator | Partially resolved | T-CLI-2 to T-CLI-7 and T-PARSE-3 are now in task 8. Other tests listed for tasks 5 to 7 still need the evaluator; see R2 Medium below. |
| R1 M2: Plan "0003" vs 0005 | Resolved | The header, §2, and §11 now cite plan 0005, and the revision-1 history entry has a bracketed correction. The plan says only functions as values are absorbed, as the developer confirmed in grill follow-up 4. The grill record still says "plan 0003"; see R2 Low. |
| R1 M3: Missing partial-read and argument-precedence tests | Resolved | T-CLI-8 is a unit test through the §3.2 seam. T-FN-8 has CLI and unit tests. I checked the locations in T-FN-8 (line 1 column 10; line 4 column 12; line 6 column 12) against §3.7 and §4. |
| R1 L1: §3.4 wording | Resolved | §3.4 now limits the ban to statements at module level and to `function` declarations inside a body. |

#### Findings

##### Medium: Several milestones still list CLI tests that need the evaluator
- **Plan reference:** §8 tasks 5–7; §8 preamble: "Each milestone ends with … passing tests for everything it adds"; task 2: "otherwise reports that `main.phil` is not yet supported via an internal error"
- **Problem:** Before task 8, a program that lexes, parses, and validates cleanly can only end in the task-2 internal error. It cannot exit with a `main` result. These tests are assigned to earlier tasks but check a successful run:
  - Task 5: T-LEX-1, T-LEX-3, T-LEX-5, and T-LEX-6 ("accepted" cases); T-LEX-4 ("exits with the same status in each case"; "the code on the next line runs"); T-LEX-7 (accepted with blocks closed); T-LEX-8 ("accepted (exits 255)").
  - Task 6: T-PARSE-1 ("`main` with only `pass` exits 0"); T-LEX-10's identifier test that expects exit 0.
  - Task 7: T-DECL-3 (the accepted annotations); T-DECL-4 (the `null` function "runs (exit 0)"); T-DECL-6 (a blank line and a comment after `return` "run normally"); T-DECL-7 (the accepted uses); T-OWN-3 (runtime, call-depth, and evaluation-depth error runs).

  This is the same problem as R1 M1, in more tests. It conflicts with D4 and with the §8 preamble. An implementer must either leave a milestone with failing tests or decide alone how to split the test.
- **Recommendation:** For each test listed, either keep only its error cases in the earlier task and move the success cases to task 8, or state that the earlier task checks the success cases with a unit test on tokens or the AST, while the CLI exit-status check runs in task 8. List the split explicitly in §8.

##### Low: The `return/*c*/42` test only shows that the program runs
- **Plan reference:** T-LEX-11, first bullet ("exits 0 (the helper runs and returns 42, which is discarded)")
- **Problem:** Exit 0 matches the grill's recommendation ("`return/*c*/42` (exit 0)"). It also rules out D40 options B and C: under B, `return42` would be an undefined-identifier error (65); under C, the comment itself would be an error. But nothing checks that the returned value is 42. The test also cannot run until task 8. Putting this code in `main` would give exit 42 and contradict the grill's stated exit 0, so using a helper is a reasonable choice.
- **Recommendation:** Optionally, add a lexer unit test in task 5 that checks that `return/*c*/42` produces the tokens `return`, `42` (value 42). This also helps with the Medium finding above.

##### Low: The read operation passed to the test seam has no defined interface
- **Plan reference:** §3.2 "Partial-read failure is testable"; T-CLI-8
- **Problem:** The developer did not choose this seam. The composer added it to implement a test the developer wanted (grill follow-up 4, M3: "Only the tests are missing"). It does not conflict with D32: POSIX calls stay in `src/source_file.cpp`, and the shipped binary passes only the POSIX wrapper. It also follows the same test-injection pattern the developer approved for D30 and T-SEC-8. So it is acceptable. However, the plan does not define the injected operation's interface:
  - its type (function pointer, template parameter, or type-erased callable);
  - how it reports an error and `EINTR` (a return value with an error code, or the global `errno`);
  - whether the 2 MiB limit check and the "no partial buffer" rule are inside the injected loop.

  T-CLI-8 depends on these answers.
- **Recommendation:** State the operation's interface in §3.2. For example, it returns a byte count or an error code without relying on the global `errno`. Also state that the size limit is enforced inside the same loop, and optionally add a unit test for the limit through the seam.

##### Low: The grill record still says "plan 0003" in D16 and D17
- **Plan reference:** Header and §2 (correct). `plans/0001_grill.md` D16 ("matching plan 0003", "absorbs plan 0003's scope"), D17 ("from plan 0003"), and the session-1 Contradictions list.
- **Problem:** This is not a defect in the plan. Grill follow-up 4 records the fix under "Answered From the Repository". The developer confirmed that it is plan 0005 and that only functions as values are absorbed. However, someone reading only the D16 row would see the wrong plan and the full scope. Read literally, `plans/0003.md` would bring in variables and operators.
- **Recommendation:** The orchestrator or grill agent should add a short correction note to the grill record without rewriting the D16 or D17 rows. Neither the plan-composer nor this reviewer may make that edit.

##### Low: §6 "Evaluation order" row is narrower than the grill table, with no stated reason
- **Plan reference:** §6 "Record call-argument order; keep the rest open"; grill session 1 Open Decisions table: "Evaluation order | Left to right (D6) | Yes: mark resolved"
- **Problem:** The plan is more conservative than the grill table, which is probably correct because operands and assignment targets do not exist yet. But the difference is not explained, so the task-1 implementer has two sources that disagree.
- **Recommendation:** Add one sentence to §6 saying that D6 covers only call arguments, so the operand and assignment-target parts stay open.

#### Open questions for the developer
- None. None of the findings above blocks implementation.

#### Notes for the secondary reviewers
- Architecture: §3.2 adds a production-code seam (an injected chunk-read operation in `src/source_file.hpp`) that the developer did not choose. Please check whether its form fits the module and ownership design.
- Security: The same seam is in the security-critical loader. Please confirm that the shipped binary cannot pass any read operation other than the POSIX wrapper, and that the limit check and error handling cannot be bypassed through it. The developer-accepted risks in §5.1 (D24, D28a, D31b, D37) are already flagged.

#### Areas reviewed
- Structure: all required sections are present.
- Developer intent: D1–D40 and the start-of-line rule derived from D40 are reflected accurately, and no earlier requirement was dropped.
- Specification precision: D40 interactions with `;` continuation, multi-line comments, indentation, and number/identifier adjacency.
- Consistency with AGENTS.md, `docs/language_rules.md`, plans 0002, 0003, and 0005, and the grill record.
- Open Decisions coverage (§6 and task 1).
- Errors and the CLI status table.
- The Security section is present and specific.
- Scope and size: D4 keeps this as one plan.
- Task ordering and file lists.
- Acceptance tests: I checked the line and column numbers in T-LEX-10, T-LEX-11, T-DECL-5/6/7, and T-FN-8 by hand.
- Review Disposition: every R1 finding and every legacy finding is dispositioned.
- Open questions: none.
- There were no earlier architecture or security rounds to check.

### Round 3 (2026-11-14)

Confirmed plan ID: `0001`. Plan path: `plans/0001.md` (revision 5, decisions D1–D46). Review path: `plans/0001_review.md`. Grill record: `plans/0001_grill.md`.

**Plan verdict:** Go (0 Critical, 0 High, 1 Medium, 4 Low)

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| R1 M1: Tests scheduled before the evaluator | Resolved | Resolved together with R2 Medium below. |
| R2 Medium: Milestones list CLI tests that need the evaluator | Resolved | The §8 preamble defines the split: error cases run as CLI tests in the detecting task, accepted cases run as unit tests on phase output, and the CLI exit-status checks run in task 8. Every task lists its split. Task 8 lists the deferred CLI checks. T-OWN-3 moved to task 8. T-LEX-11 and T-NULL-4 are split into parts. I checked every test ID in tasks 2–8 against §9. |
| R2 Low: `return/*c*/42` only shows the program runs | Resolved | T-LEX-11 part (a) is a lexer unit test that checks the tokens `return` and `42` (value 42). |
| R2 Low: Read-seam interface undefined | Resolved | §3.2 defines `ChunkReadResult`, `ChunkReader` (`std::function` over `std::span<char>` with `std::error_code`, no `errno`), `readBoundedSource` as the only place that enforces the limit, retries, and discards partial data, and `loadMainSource` with no read parameter. T-CLI-9 was added. |
| R2 Low: Grill record says "plan 0003" | Resolved | The follow-up 5 section of the grill record has the correction note, and the plan header cites it. |
| R2 Low: §6 evaluation-order row narrower than the grill | Resolved | The §6 row now explains why the operand and assignment-target parts stay open. |

#### Findings

##### Medium: Task 1 edits a style-guide "Includes section" that does not exist
- **Plan reference:** §8 task 1: "`.github/instructions/cpp-style-guide.instructions.md`, Includes section only (D46)"; §2 In scope: "the style guide's Includes section".
- **Problem:** The style guide has no Includes section. Its only include rule is one bullet under "Naming & Layout" (line 26: `#pragma once` … include order). The plan copies D46's wording, but "Includes section only" restricts the edit to a section that is not there. The implementer must guess: create a new section, or extend the existing bullet. Either choice can break the "only" restriction. The grill's "Answered From the Repository" describes the guide's include content correctly but does not name where it is.
- **Recommendation:** In task 1, name the target exactly. For example: "extend the include bullet under Naming & Layout", or "add a new `## Includes` section after Naming & Layout, and move the existing include-order rule into it". This is how to carry out D46, not a new decision, so a developer question is needed only if the composer believes D46 meant something else.

##### Low: `Module::load`'s private constructor conflicts with the style guide's `std::make_unique` rule
- **Plan reference:** §3.9: "It first allocates the `Module` on the heap (private constructor)"; style guide, Ownership & Lifetime: "never call `new`/`delete` directly … Create them with `std::make_unique`".
- **Problem:** `std::make_unique` cannot call a private constructor. A direct implementation therefore writes `std::unique_ptr<Module>(new Module(...))`, which the style guide forbids. The standard fix (a passkey or private tag type that keeps the constructor public) is not stated.
- **Recommendation:** Add one clause. For example: "the constructor is public but takes a private passkey type that only `Module::load` can create, so `std::make_unique` is used".

##### Low: §3.2 says T-BUILD-6 checks the call ban, but T-BUILD-6 checks only includes
- **Plan reference:** §3.2: "No other file may call `open`, `fstat`, `read`, or `close` directly, and no other file includes `<fcntl.h>`, `<unistd.h>`, or `<sys/stat.h>`; T-BUILD-6 checks this mechanically"; T-BUILD-6 checks include lines only.
- **Problem:** "this" reads as covering both rules. The call ban is enforced only indirectly (without the headers, the functions are normally undeclared) and by code review. An implementer or reviewer could expect T-BUILD-6 to scan for calls.
- **Recommendation:** Change the sentence to say that T-BUILD-6 checks the header rule mechanically and the call ban follows from it, or extend T-BUILD-6. The developer did not decide this either way, so either is acceptable.

##### Low: The evaluation-depth guard's unit test (task 3) needs a location before any AST node exists
- **Plan reference:** §8 task 3 runs T-SEC-5; T-SEC-5: "raises the evaluation-depth runtime error at the entered node's location"; AST nodes are created in task 6.
- **Problem:** In task 3 there is no `ASTNode` whose location the guard could read. This works only if the guard takes a `SourceLocation` (task 2) rather than a node. That is the natural design, but the plan does not say so.
- **Recommendation:** State that the evaluation-depth guard takes the entered node's `SourceLocation`, so T-SEC-5 can run in task 3 without AST nodes.

##### Low: The grill record cites "T-RUN-8" and "§3.10", which do not exist in the plan
- **Plan reference:** Revision History, revision 5: "(The grill's references to "T-RUN-8" and "§3.10" for the `runTopLevel` seam correspond to T-SEC-8 and §4 in this plan.)"; grill follow-up 5, D44 row and "Answered From the Repository".
- **Problem:** This is not a defect in the plan. The plan's normative text (§4, §7.1 Tests, T-SEC-8) uses the correct IDs and meets D44's intent: the throwing bodies are supplied only from `tests/unit/`. However, the mapping appears only in the Revision History. Someone reading the D44 row will not find T-RUN-8 (T-RUN stops at 6) or §3.10 (§3 stops at 3.9).
- **Recommendation:** The orchestrator or grill agent should add a correction note to the grill record, as was done for "plan 0003". Neither the plan-composer nor this reviewer may make that edit. No plan change is needed.

#### Open questions for the developer
- None. None of the findings above blocks implementation.

#### Notes for the secondary reviewers
- Architecture: `kMaxParseNesting` is held by the execution context in `src/runtime/evaluator/`, so `frontend/parser/` must include a runtime header. Check that this fits D45's aim of keeping the frontend free of runtime dependencies.
- Architecture: please re-check the dispositions of your round 1 findings, including the deferred literal-caching Low.
- Security: `readBoundedSource` retries without limit on `std::errc::interrupted`. A reader that keeps reporting it would loop forever. Check whether a bound is needed; only the POSIX reader ships.
- Security: T-BUILD-6 is a text scan of include lines. Consider whether alternative spellings (for example `<sys/fcntl.h>`) or transitive inclusion weaken D32's confinement.
- Security: the source buffer is now moved into a non-movable, heap-allocated `Module` before any view exists (§3.9), and the interim pipeline in tasks 4–6 holds a non-moved local. Please confirm this removes the use-after-free risk raised in architecture round 1.

#### Areas reviewed
- Structure: all required sections are present, including the new §7.1.
- Developer intent: D41–D46 and the rule derived from D42 (no public-header tier) appear accurately in §7.1, §3.2, §3.9, §4, §5, §7, and §8 tasks 1–2. I compared the D45 file placements with the §7.1 table entry by entry. D1–D40 are unchanged from revision 4. No earlier requirement was dropped.
- Specification precision: the read seam (§3.2), `Module::load` and the buffer handoff (§3.9), `NodeKind`/`kind()`, placeholder `evaluate` bodies, `ObjectType`, and `matchesDeclaredType`.
- Consistency: checked against AGENTS.md, `directory-structure.md`, the style guide (findings above), the grill record, and the existing `src/` placeholders.
- Open Decisions: the §6 table is unchanged except for the evaluation-order explanation. D41–D46 correctly need no AGENTS.md change.
- Errors and CLI: the §4 table is unchanged. The `InternalError` for an over-reported byte count is defined in §3.2 and tested by T-CLI-8.
- Security section: present and specific, and updated with the new paths and the T-BUILD-6 reference.
- Scope and size: D4 still applies, and no split is recommended.
- Implementation tasks: checked the order and the split between each task and task 8 for every listed test ID.
- Acceptance criteria: new and changed tests T-BUILD-4, T-BUILD-6, T-CLI-8, T-CLI-9, T-FN-6, T-LEX-11 (parts a–c; I checked the line and column numbers again), and T-NULL-4.
- Review Disposition: all 5 round-2 findings and all 9 architecture round-1 findings have rows. The one rejection (literal caching) has a specific reason that follows the reviewer's own condition. The architecture open question was answered by D41–D46. I did not judge whether the architecture findings are fixed.
- Open questions: none, and none blocks implementation.

### Round 4 (2026-11-14)

Confirmed plan ID: `0001`. Plan path: `plans/0001.md` (revision 6, decisions D1–D47). Review path: `plans/0001_review.md`. Grill record: `plans/0001_grill.md`.

**Plan verdict:** Go (0 Critical, 0 High, 1 Medium, 2 Low)

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| R3 Medium: Task 1 edits a style-guide "Includes section" that does not exist | Resolved | §2 and §8 task 1 now target the `#pragma once` / include-order bullet under "Naming & Layout" (style guide line 26), extend only that bullet, and keep its include-order rule. |
| R3 Low: Private constructor vs `std::make_unique` | Resolved | §3.9 uses a public constructor that takes a private nested passkey type; `Module::load` uses `std::make_unique`. §8 task 7 matches. |
| R3 Low: §3.2 says T-BUILD-6 checks the call ban | Resolved | §3.2 and T-BUILD-6 now say the probe checks headers only, and the call ban follows from it plus code review. |
| R3 Low: Evaluation-depth guard needs a location before AST nodes exist | Resolved | §3.7 and §8 task 3: the guard takes a `SourceLocation`; T-SEC-5 states it runs without AST nodes. |
| R3 Low: Grill cites "T-RUN-8" and "§3.10" | Resolved | The grill's follow-up 6 correction note maps them to T-SEC-8 and §4. A search of the plan finds them only in the header's source list, the §11 row, and the revision history, none normative. |

#### Findings

##### Medium: The error location for a backslash followed by end of file or by a line break inside a string is set only by a test, or not at all
- **Plan reference:** §3.3 Tokens, String literals ("any other escape is a lexical error (D23c). A raw line break … inside a string is a lexical error …, as is end of file before the closing quote"); §4 row "Unterminated string; raw line break …; unknown escape; `${` in string", location "the `"` / the line break / backslash / `$`"; T-LEX-12 ("an unterminated string whose last byte is `\` … lexical error (end of file before the closing quote) at the `"`").
- **Problem:** A `\` followed by end of file, or by LF, CR LF, or a lone CR, matches two §3.3 rules with different §4 locations: "unknown escape" at the backslash, and "unterminated string" at the `"` (end of file) or "raw line break" at the line break. The class and exit status are the same, but tests match line and column (§4). T-LEX-12 now settles the end-of-file case (composer-inferred, at the `"`), but only inside a test, and §3.3 prose does not say it. The line-break case is not settled anywhere, and no test covers it, so two implementers could report different columns for `"a\` followed by a newline. The `$` case in T-LEX-12 is not ambiguous: `$` without `{` is ordinary content, so end of file then gives the unterminated-string error at the `"`.
- **Recommendation:** Add one sentence to §3.3 String literals, for example: "A `\` is an escape only if a character follows it on the same line; a `\` followed by end of file is the unterminated-string error at the `"`, and a `\` followed by a line break is the raw-line-break error at the line break" (or the alternative the composer prefers, stated just as precisely). Add the line-break case to T-LEX-9 (or T-LEX-4) with its exact column. This is a diagnostic-location detail that the grill left to the composer ("define message classes and source-location formats"), so it needs no developer decision.

##### Low: T-CLI-10 does not say how the child passes its skip reason and check results to the parent
- **Plan reference:** T-CLI-10 steps 3, 4, and 6 ("the test writes `T-CLI-10 SKIPPED: <reason>` naming the failed step"; "stderr redirected to a pipe read by the parent. The result must be 66, and the stderr text must start with …"; "The child reports pass, skip (with the reason), or which check failed through its exit status and ends with `_exit`").
- **Problem:** An exit status cannot carry free text. The plan does not say whether each failed step gets its own exit code that the parent maps to a reason, or whether the child writes the reason to a pipe. Anything the child writes to `stdout` through buffered I/O is lost at `_exit`. It is also only implied that the parent, not the child, checks the stderr prefix (the child cannot read its own redirected stderr). The test still works if an implementer fills these in sensibly, so this does not block.
- **Recommendation:** State that the child exits with a distinct code per outcome (pass, one code per skip step, one per failed check), that the parent maps the code to the `T-CLI-10 SKIPPED: <reason>` line or a failure message, and that the parent reads the pipe and checks the stderr prefix. Alternatively, give the child a second pipe for its reason text.

##### Low: `_exit` in the T-CLI-10 child is a test-only exception to the style guide's ERR50 rule but is not recorded as one
- **Plan reference:** T-CLI-10 step 6 ("ends with `_exit`, the POSIX way to end a forked child without running the parent's `atexit` handlers or doctest teardown"); §4 ("Nothing calls `std::terminate`, `assert`, `abort`, or `exit` (CERT ERR50)"); style guide, Errors & Diagnostics (no `std::abort`, `std::exit`, or `std::terminate`, "which skip cleanup (CERT ERR50)").
- **Problem:** `_exit` skips cleanup in the same way as the functions the style guide bans, and CERT ERR50-CPP covers `_Exit`. Using it in a forked child is the standard POSIX idiom, and the plan gives the reason, so the choice itself is sound (composer-inferred, test code only). But the plan records the other test-only style-guide exception (D36, `cert-err58-cpp`) explicitly as an exception, and AGENTS.md asks that conflicts be reported rather than chosen silently. §4's sentence reads as covering the whole project.
- **Recommendation:** Add one sentence to T-CLI-10 or §7: "This is a test-only exception to the style guide's ERR50 rule: a forked child must not run the parent's `atexit` handlers, flush the parent's duplicated stdio buffers, or unwind into doctest. `src/` never calls `_exit`." Optionally limit the child to work that needs no destructor with external effects before `_exit`.

#### Open questions for the developer
- None. None of the findings above blocks implementation.

#### Notes for the secondary reviewers
- Architecture: T-CLI-10 adds a second `ctest` entry that runs `philios_tests` filtered to one test case, with the main entry excluding it (§7). Check that this fits the test layout. The integer-literal, parser-limit, and call-sequence changes are your round 2 findings; I did not judge them beyond consistency.
- Security: T-CLI-10 may skip on stock Ubuntu 24.04, whose AppArmor setting restricts unprivileged user namespaces, so `O_NOCTTY` (D47) may go unverified on the target platform even though skips are allowed. Please also check that `_exit` in the forked child and the precondition `open` (which uses `O_NOCTTY`) cannot hide a regression.

#### Areas reviewed
- Structure: all required sections are present.
- Developer intent: D47 appears in the header, §3.2 (flag list and reason), §5, §7, §7.1, §8 task 4, T-CLI-10, §11, and §12. T-CLI-10 meets every limit the developer set: no elevated privileges, a clear skip with its reason, test code only under `tests/`, rejection as a non-regular file with exit 66 and a message naming `main.phil`, and the controlling-terminal check. D1–D46 are unchanged, and no earlier requirement was dropped.
- Specification precision: T-CLI-10's mechanism (pseudo-terminal, `setsid`, user and mount namespace, bind mount, `O_NOCTTY` precondition open, `/dev/tty` `ENXIO` check) is technically sound; without `O_NOCTTY` the session-leader child would gain the terminal, so the test detects a regression. Also checked: the passkey constructor, the `SourceLocation` guard, `parseDecimalInteger`, the parser's nesting value, `callFunction`, non-allocating `noexcept` destruction, the bounds-checked lexer accessor, and the `ChunkReadResult` error-with-bytes rule.
- Composer-inferred items checked: T-LEX-12's end-of-file `\` and `$` cases (Medium above for `\`; `$` is unambiguous); `_exit` in the T-CLI-10 child (Low above).
- Consistency: checked against AGENTS.md, the style guide (Naming & Layout bullet; Errors & Diagnostics), the grill record (follow-up 6 and its correction note), and earlier rounds. Every T-LEX-12 expected result matches §3.3 and §4.
- Open Decisions: §6 is unchanged; D47 affects no Open Decision, as the grill states.
- Errors and CLI: a terminal-device `main.phil` falls under the existing §4 startup row (not a regular file, exit 66). No new error condition.
- Security section: present and specific, and updated for `O_NOCTTY`, the non-allocating destructors, and the bounds-checked lexer.
- Scope and size: D4 still applies, and no split is recommended.
- Implementation tasks: T-CLI-10 sits in task 4, after `runTopLevel` (task 2) and alongside `loadMainSource`. T-LEX-12 is in task 5, T-OWN-5 in task 7, and T-BUILD-6 and the T-CLI-10 `ctest` entry in task 4. Every §9 test ID is assigned to a task.
- Acceptance criteria: new and changed tests T-CLI-8, T-CLI-10, T-LEX-8 (unit), T-LEX-12, T-SEC-5, T-OWN-5, and T-BUILD-6.
- Review Disposition: all 15 items have rows (5 from Plan Review round 3, 3 from Architecture round 2, 6 findings and the open question from Security round 1). There are no rejections in this revision. I did not judge whether the secondary reviewers' findings are fixed.
- Open questions: §10 lists none, and none blocks implementation.

### Round 5 (2026-11-14)

Confirmed plan ID: `0001`. Plan path: `plans/0001.md` (revision 7, decisions D1–D48). Review path: `plans/0001_review.md`. Grill record: `plans/0001_grill.md`.

**Plan verdict:** Go (0 Critical, 0 High, 1 Medium, 1 Low)

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| R4 Medium: The location for a `\` followed by end of file or a line break is set only by a test, or not at all | Resolved | §3.3 now has the rule "A `\` at the end of a line or of the file": end of file gives the unterminated-string error at the `"`, and a line break gives the raw-line-break error at its first byte. §4 states the same. T-LEX-9 adds LF, CR LF, and lone-CR cases at line 2, column 15. I checked the column (`"` at 12, `\` at 14, break at 15), and T-LEX-12 cites the rule. The rule follows from D23b/D28a and is within the composer's discretion over location formats (grill, session 2026-11-04), so it needs no developer decision. One edge case remains (Medium below). |
| R4 Low: T-CLI-10 does not say how the child passes its skip reason and check results to the parent | Resolved | T-CLI-10 steps 6–7 give named exit codes: pass 10 (not 0 or 1), skips 20–25, and failures 30–34. The parent maps each code, prints the skip line only for a skip code, and checks the stderr prefix itself after draining the pipe. Every unmapped outcome fails. |
| R4 Low: `_exit` in the T-CLI-10 child is not recorded as a test-only ERR50 exception | Resolved | §7 has a "Test-only `_exit` exception" bullet that gives the reason and limits the exception to the T-CLI-10 file, with a `NOLINT` rule. §4 states that nothing under `src/` calls `exit`, `_exit`, or `_Exit`. |

#### Findings

##### Medium: A `\` followed by a forbidden character matches two rules that report different locations
- **Plan reference:** §3.3 "A `\` at the end of a line or of the file" ("The unknown-escape error at the backslash applies only when a character other than `\`, `"`, `n`, or `t` follows the `\` on the same line"); §3.3 Encoding and characters (malformed UTF-8, disallowed control characters, and bidi characters are lexical errors "everywhere, including inside strings"; bidi errors are reported "at the character's first byte"); §4 lexical rows (location "offending byte" vs "backslash").
- **Problem:** Inside a string, a `\` followed by NUL or another forbidden control character, by a bidi formatting character (U+202A–U+202E, U+2066–U+2069), or by a malformed UTF-8 sequence matches two rules. It is an unknown escape, reported at the backslash (column *c*), and it is also a forbidden or malformed character, reported at that character (column *c*+1). The class and exit status match, but tests check columns (§4), so implementations can disagree. An implementation that validates characters before handling escapes reports *c*+1. One that switches on the byte after the `\` reports *c*. The ambiguity existed before revision 7, but the new sentence now says explicitly that the backslash rule covers any other character, which makes the conflict visible. T-LEX-3 places these characters in strings but never directly after a `\`. This is the same kind of issue as the R4 Medium, which revision 7 settled for line breaks and end of file.
- **Recommendation:** In the §3.3 rule, state which rule takes precedence. Consistent with the line-break case, one option is: "A malformed UTF-8 sequence, a disallowed control character, or a bidi formatting character after a `\` is reported by its own rule at its first byte, not as an unknown escape." Alternatively, the unknown-escape error could take precedence, if stated just as precisely. Add one T-LEX-9 or T-LEX-3 case, for example `"a\` followed by U+202E, with its exact column. Like the R4 Medium, this is a diagnostic-location detail that the grill left to the composer, so it needs no developer decision.

##### Low: T-BUILD-7 checks only that the zero-test pattern matches, not that it rejects a normal run, and the example pattern is likely to be over-broad once written in CMake
- **Plan reference:** §7 "T-CLI-10 registration" (`FAIL_REGULAR_EXPRESSION` "for example the pattern `test cases: *0 *\|`"); T-BUILD-7 (passes if "the pattern matches the pinned doctest release's zero-test summary"; "`ctest -N` (or the generated test list) shows that the dedicated T-CLI-10 entry passes `--test-case=`…").
- **Problem:** In a quoted CMake argument, `\|` is an identity escape that becomes a bare `|`, and CMake regular expressions treat `|` as alternation. Written as shown, the example pattern becomes `test cases: *0 *` OR an empty pattern, which matches any output. T-BUILD-7 would still pass, because it checks only that the pattern matches a zero-test run. The dedicated entry would then fail every run in which T-CLI-10 actually executes. On a host where T-CLI-10 always skips (D48), whether the defect shows up depends on how `ctest` ranks a skip match against a fail match. This is not a silent false pass of the check of D47, so it does not block. Separately, plain `ctest -N` lists only test names, not commands or properties.
- **Recommendation:** Have T-BUILD-7 also check that the pattern does not match the pinned release's summary for a run of one passing test case (for example a second guard entry, or a check of a captured summary line in the probe script). Note that the CMake source must write the escape as `\\|`, or use `[|]`. Name the inspection command, for example `ctest -N --show-only=json-v1`, or the generated `CTestTestfile.cmake`.

#### Open questions for the developer
- None. Neither finding blocks implementation, and both are within the composer's discretion.

#### Notes for the secondary reviewers
- Architecture: None beyond T-BUILD-7's new filter-guard `ctest` entry (§7, task 4), which adds no target.
- Security: In the T-CLI-10 child, the inherited pseudo-terminal master descriptor and the zero-test `FAIL_REGULAR_EXPRESSION` escaping (Low above) may deserve a glance. Check whether a `SKIP_REGULAR_EXPRESSION` match can mask a `FAIL_REGULAR_EXPRESSION` match under CMake 3.28.

#### Areas reviewed
- Structure: all required sections are present, and the header is updated to D1–D48 and lists the follow-up 7 source.
- Developer intent: D48 appears in the header, the §9 introduction, T-CLI-10, §8 task 9 (per-build Debug/ASan/Release outcome with the skip reason, and a code-review statement for `O_NOCTTY` when T-CLI-10 skips), §10, §11, and §12. No opt-in privileged run is required. D47's no-privileges and regression-detection rules are unchanged. D1–D47 are unchanged, and no earlier requirement was dropped.
- Composer-derived rule: a `\` followed by end of file gives the unterminated-string error at the opening `"`, and a `\` followed by a line break gives the raw-line-break error at the line break. This is consistent with D23b ("raw newline in a string is an error") and D28a ("every line break, including a lone CR, inside a string is a raw newline"), and with §4 and T-LEX-9 and T-LEX-12. Accepted. Remaining edge case: Medium above.
- Specification precision: T-CLI-10 steps 1–7. The skip steps match their codes (parent-side failures in step 1 and of `fork` are skips; `mkdtemp`, `main.phil`, `pipe`, `dup2`, and `chdir` failures are failures). The pipe is drained before `waitpid`. The skip line is printed only for a skip code. Also checked: the §7 `ctest` wiring through `PHILIOS_TCLI10_TEST_CASE`, `TIMEOUT 10`, the §3.9 unlink invariant, and T-OWN-4's worklist of at least 200,000 nodes.
- Consistency: checked against AGENTS.md (Verification, Open Decisions), `plans/AGENTS.md`, the grill record (follow-up 7), and §4, which states that `src/` never calls `_exit` while §7 records the test-only exception. No contradictions found.
- Open Decisions: D48 affects none, as the grill states. §6 is unchanged.
- Errors and CLI: no new error condition. The §4 lexical row now gives the `\` locations.
- Security section: present and unchanged in substance.
- Scope and size: D4 still applies, and no split is recommended.
- Implementation tasks: T-BUILD-7 and the T-CLI-10 wiring are in task 4, and T-CLI-10 runs only `loadMainSource` and `runTopLevel`, both of which exist by then. The T-LEX-9 line-break cases are CLI error cases in task 5, run through the interim pipeline. The task 9 report requirement is present.
- Acceptance criteria: new or changed tests are T-LEX-9, T-LEX-12, T-CLI-10, T-BUILD-7, and T-OWN-4.
- Review Disposition: all 8 items from Plan Review round 4 (3), Architecture round 3 (1), and Security round 2 (3 Lows and the open question) have Accepted rows with plan sections. There are no rejections. I did not judge whether the secondary reviewers' findings are fixed.
- Open questions: §10 lists none, and none blocks implementation.

## Architecture Review

### Round 1 (2026-11-13)

Confirmed plan ID: `0001`. Plan path: `plans/0001.md` (revision 4, decisions D1–D40). Review path: `plans/0001_review.md`. Grill record: `plans/0001_grill.md`. Gate checked: Plan Review Round 2 is **Go**, and the overall verdict was `Pending architecture review`.

**Architecture verdict:** No-go (0 Critical, 2 High, 3 Medium, 4 Low)

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| None | — | This is the first architecture round. |

#### Findings

##### High: The file layout contradicts the documented `src/` directory structure
- **Plan reference:** §7 Layout ("a static library `philios_core` (all of `src/` except `main.cpp`)"); §8 tasks 2–8, which place every file directly in `src/` (`src/lexer.cpp`, `src/parser.cpp`, `src/ast.cpp`, `src/validator.cpp`, `src/environment.cpp`, `src/errors.cpp`, and so on); §3.1, §3.2, and §5, which name `src/source_file.cpp` as the only file allowed to make POSIX calls (D32).
- **Problem:** `docs/developers/src/getting-started/directory-structure.md` documents a different layout, and the repository already has it as `.gitkeep` placeholders: `src/frontend/{ast,lexer,parser}/`, `src/analysis/semantic_analyzer/`, `src/runtime/{environment,evaluator,virtual_machine}/`, `src/backend/bytecode_compiler/`, and `src/common/`. The document says "Keep the structure aligned with these responsibilities as source files are added", and that runtime values and module execution belong in `runtime/`. The plan never mentions this structure. `ai-usage.md` tells the `implementation-architect` to follow "the approved plan and existing source layout", but here the two disagree. The plan also gives no location for components that the document does not cover: the CLI, source loading, and exit statuses.
- **Impact:** The implementer has to pick a layout on its own. Each choice breaks one of the two sources. A flat layout also forces later plans to move files, update include paths and CMake source lists, and rewrite the D32 path rule.
- **Recommendation:** Map every file in §8 to the documented directories. For example: lexer, `utf8`, and `token` in `frontend/lexer/`; parser in `frontend/parser/`; AST in `frontend/ast/`; validator in `analysis/semantic_analyzer/`; environment in `runtime/environment/`; the `evaluate` implementations and interpreter in `runtime/evaluator/`; object model, function, module, execution context, and completion in `runtime/` (state which subdirectory); errors and exit statuses in `common/`. Say where `cli`, `source_file`, and `main.cpp` go. State the include-path convention, such as paths relative to `src/`. Update D32's file path, §7's `philios_core` definition (all `.cpp` files under `src/` except `main.cpp`), and every §8 file list. If the developer wants a flat layout instead, record that as a decision and update the documentation. The plan must not silently diverge from it.

##### High: The source buffer's lifetime relative to its `string_view`s is not pinned down, and no component is assigned to run the load pipeline
- **Plan reference:** §3.2 "The loaded bytes become an immutable `std::string` owned by the `Module`"; §3.9 "A `Module` is returned to the caller only after lexing, parsing, validation, and function registration all succeed" and "tokens and AST nodes may hold `std::string_view`s into the module's source buffer"; §8 tasks 4–6 ("Wire into `runProject`") and task 7 (`src/module.hpp/.cpp`: "load, register functions").
- **Problem:** The plan says who finally owns the buffer, but not when it takes ownership or whether the buffer or the `Module` may move afterward. Suppose the bytes are lexed into views while held in a local in `runProject`, then moved into a `Module`, or the `Module` is returned by value. Moving a `std::string` gives no guarantee that pointers into its data stay valid; with the small-string optimization they do not. Every token and AST view would then dangle. The plan also never says which component runs the pipeline: `runProject`, which tasks 4–6 extend step by step, or a `Module` loading function, which §3.9 and task 7 imply.
- **Impact:** A straightforward implementation can create dangling views. That is undefined behavior, and AGENTS.md ranks preventing it above everything else. The test sources are larger than the inline-string capacity, so ASan may never see the problem. Separately, two implementers could split orchestration between `cli.cpp` and `module.cpp` in incompatible ways, and task 7 would have to rewrite the wiring from tasks 4–6.
- **Recommendation:** Specify the following. (1) A single loading function, for example `Module::load(ExecutionContext&, std::string source)` returning `std::unique_ptr<Module>`, owns the order lex → parse → validate → register. `runProject` calls it, then the interpreter. Before task 7, the earlier milestones may use a temporary local pipeline, but the plan must say where that pipeline goes when task 7 replaces it. (2) The source buffer is put into its final, non-moving owner before the first view is created, and is never moved afterward. For example, the `Module` is created on the heap first and the buffer is moved into it, or the buffer is held as `std::unique_ptr<const std::string>` and shared with the module. (3) `Module` is neither copyable nor movable. The test-only `weak_ptr` observers in T-OWN-1 and the D3 teardown order are unchanged.

##### Medium: The read-operation seam in `src/source_file.hpp` is sound, but its interface and responsibilities are not defined
- **Plan reference:** §3.2 "Partial-read failure is testable … declares the bounded chunk-reading loop as a function that takes the chunk-read operation as a parameter. The shipped binary passes only the POSIX `read` wrapper"; §8 task 4; T-CLI-8.
- **Assessment (requested by the orchestrator):** The seam is architecturally acceptable. It does not conflict with D32: the loop calls no POSIX function itself, the only real reader is the wrapper in `src/source_file.cpp`, and the fake in `tests/unit/` calls no POSIX function either. It follows the same pattern as `runTopLevel`'s injected body (D30/T-SEC-8): behavior injected through a parameter, no mutable hook in production state, and nothing a script can reach. That fits AGENTS.md Core Constraint 1 and the style guide's ban on mutable static state. It also makes the 2 MiB boundary testable cheaply in unit tests. The gaps are in the interface:
  - **Header leakage.** If the parameter mirrors `read(int, void*, size_t) -> ssize_t`, then POSIX types and a file descriptor appear in `source_file.hpp`. The fake would also need to report failure through the global `errno`. That spreads the POSIX dependency D32 confines to one `.cpp` file, and it makes the `EINTR` case in T-CLI-8 depend on `errno`.
  - **Where the loop lives.** If the operation is a template parameter, the style guide ("Headers define only inline, `constexpr`, or template functions") puts the whole loop, including the size-limit check, in the header and outside `source_file.cpp`. A type-erased callable or a function reference keeps it in the `.cpp` file.
  - **What the loop owns.** The plan does not say whether the loop enforces the 2,097,152-byte limit, retries `EINTR`, and raises `StartupError` without returning a partial buffer, or whether the caller does some of that. If the caller does, the production path and the tested path differ, and T-CLI-8 no longer tests the code that ships.
  - The body type that `runTopLevel` receives is just as unspecified. Both seams should use the same form.
- **Impact:** Each implementer would choose these differently. The worst choice exposes POSIX in a header, or leaves limit enforcement outside the tested path.
- **Recommendation:** In §3.2, define the read operation in the project's own types. For example, it takes a `std::span<char>` destination and returns a small result struct holding a byte count and an explicit error code, with `0` bytes meaning end of file and no dependence on `errno`. The real reader is a lambda or function in an anonymous namespace of `source_file.cpp` that captures the RAII descriptor. Pass it as a non-template callable, such as `const std::function<…>&`, so the loop body stays in `source_file.cpp`. State that the loop is the only place that enforces the size limit, retries `EINTR`, and discards partial data on error. State that `source_file.hpp` includes no POSIX header. The public entry point that opens `main.phil` should take no read operation. Use the same callable form for `runTopLevel`'s body. Optionally add a unit test of the limit at 2,097,152 and 2,097,153 bytes through the seam (plan-reviewer area).

##### Medium: The boundary between the AST, the validator, and the evaluator is unspecified
- **Plan reference:** §3.9 "every AST node derives from `ASTNode`, which provides a virtual `… evaluate(…) const`"; §8 task 6 (AST nodes are `final`, the parser builds them, and the milestone must build and test), task 7 (validator), and task 8 ("`evaluate` implementations in `src/ast.cpp`").
- **Problem:** Three architectural choices are left to the implementer:
  1. **Validator traversal.** The validator must walk every function body for D25, D26, D29, and D31a. The plan does not say how it identifies node kinds: a `kind()` enumeration, a visitor interface, or chains of `dynamic_cast`. AST destruction (iterative) and later plans' passes need the same capability.
  2. **`evaluate` before task 8.** In task 6, concrete nodes must be instantiable, so `evaluate` needs a definition. The plan does not say whether task 6 adds placeholder bodies that throw `InternalError`, or whether `evaluate` is declared later. Declaring it later changes `ASTNode` between milestones.
  3. **Where `evaluate` lives.** Task 8 places the definitions in `src/ast.cpp`, which makes the AST depend on `Function`, `Environment`, and `ExecutionContext`. The documented layout puts AST evaluation in `runtime/evaluator/` (see the High finding above). With `virtual evaluate` required, the definitions can still live in an evaluator source file. The plan should say which.
- **Impact:** Each implementer would build a different traversal shape. Later plans that add node kinds would build on whichever was chosen. Task 6 may not be able to finish as an independent milestone (D4).
- **Recommendation:** State the node-kind mechanism. A small `enum class NodeKind` with a virtual `kind()` is enough, and the validator and iterative destructor can both use it; do not build a generic visitor framework. State that task 6 defines `evaluate` bodies that throw `InternalError` ("not yet implemented"), which task 8 replaces. Name the file that holds the real `evaluate` definitions, in line with the layout decision.

##### Medium: T-FN-6 leaves an unspecified test hook in production code
- **Plan reference:** T-FN-6 "with an instrumented execution context or test hook, arguments `a(), b(), c()` are evaluated in that order".
- **Problem:** "Instrumented execution context or test hook" lets the implementer add test-only state or callbacks to `ExecutionContext`, a production class. That cuts against the pattern the plan uses elsewhere, where seams are parameters supplied only from `tests/unit/` (T-SEC-8, T-CLI-8). The design can already support this test without any hook: `ASTNode` is a non-`final` polymorphic base. A test-only `ASTNode` subclass in `tests/unit/` that records the order of its `evaluate` calls, used as `CallExpr` arguments, observes the order directly. This works only if `CallExpr` accepts any `ASTNode` expression as an argument.
- **Impact:** A hook in `ExecutionContext` would add mutable test-only state to every interpreter run and widen the interface that later plans must keep.
- **Recommendation:** Specify the mechanism: a test-only `ASTNode` subclass defined in `tests/unit/`. Forbid adding test hooks to `ExecutionContext`. Make sure the `CallExpr` constructor accepts argument nodes typed as the expression base, so the test can be built.

##### Low: No runtime type-identity mechanism is specified for exact typing (D14)
- **Plan reference:** §3.6 "an argument or return value matches a declared type only if its runtime class is exactly that type"; §3.7 "check the callee is a `Function`".
- **Problem:** The plan does not say how the evaluator compares a value's class with a `typeName`. It could use a virtual type tag on `Object`, `dynamic_cast`, or `typeid`. Plan 0002 will add `Object` as a declarable type and `not()`, so this comparison will grow.
- **Recommendation:** Put a single check in one place, for example a function that compares a value with a parsed `typeName`, backed by a virtual type tag or `dynamic_cast` on `Object`. Both the argument and the return checks should call it.

##### Low: Literal objects may be allocated on every evaluation
- **Plan reference:** §3.6 (every value is a `std::shared_ptr<Object>`); §3.9 `Completion`.
- **Problem:** The plan does not say whether an `IntegerLiteral` or `StringLiteral` creates a new `Integer` or `String` each time it is evaluated, or creates one immutable object when the module is loaded and returns it each time. In recursive code, allocating every time multiplies allocations. Caching is safe because the types are `ImmutableObject`s (style guide: immutable contents can be shared).
- **Recommendation:** Optionally, state that literal nodes create their value once, at parse or registration time, and return an owning copy of it. If this would make the frontend depend on the runtime object model in a way the layout decision forbids, defer it, but say so.

##### Low: Depth-limit errors versus the "resource exhaustion is a distinct type" sentence
- **Plan reference:** §4 "Host I/O failures, internal invariant failures, and resource exhaustion are distinct types", but the call-depth and evaluation-depth errors are `RuntimeError` kinds (D7, D24).
- **Problem:** A reader could take the depth limits to be "resource exhaustion" and give them a separate type, which would contradict D24's `runtime` class and status 70.
- **Recommendation:** Add one clause saying that in this plan "resource exhaustion" means `std::bad_alloc` only, and that the depth limits are language runtime errors per D7 and D24.

##### Low: D32's confinement rule has no mechanical check
- **Plan reference:** §3.2 "No other file may call `open`, `fstat`, `read`, or `close` directly".
- **Problem:** The rule is architectural, but only code review enforces it. Once the seam exists, a later change could add a second reader elsewhere.
- **Recommendation:** Optionally, also state that no other file includes `<fcntl.h>`, `<unistd.h>`, or `<sys/stat.h>`. Consider a small `ctest` source scan, like T-BUILD-5. Whether to add the test belongs to the plan-reviewer.

#### Open questions for the developer
- Source layout (High above): the default fix is to follow the documented `src/` structure, which needs no developer input. A developer decision is needed only if the developer wants a flat layout. In that case the documentation must change too. This blocks implementation until the plan states one layout.

#### Notes for the security reviewer
- Read seam (§3.2): confirm that the redefined interface keeps the size limit, `EINTR` retry, and partial-buffer discard inside the single loop that both production code and T-CLI-8 exercise, and that no POSIX type or header appears in `source_file.hpp`.
- Source-buffer lifetime (High above): `string_view`s into a moved `std::string` are a use-after-free risk. Check that the revised wording rules it out.
- The developer-accepted risks in §5.1 (D24, D28a, D31b, D37) were not re-reviewed here.

#### Areas reviewed
- Core architectural constraints: no globals (execution context is passed explicitly and the `null` instance is per context); single `Module` with a private `Environment`; reference counting with a strong `closureEnv` and a deterministic teardown break (D3, consistent with the interim cycle default and with not weakening owning edges); standard library plus header-only test dependency; `Object`/`ImmutableObject` hierarchy with private storage; `closureEnv`/`enclosingScope`; virtual `evaluate`; missing names are errors with no fallback. No violations.
- Open Decisions: evaluator control flow (D5 completion type), call-argument evaluation order (D6), cycle reclamation (holds for any future outcome, since only module-level functions exist), concurrency (single-threaded counters), import resolution (not used). No silent commitments.
- Component boundaries and dependencies: findings on layout, pipeline orchestration, and the AST/validator/evaluator boundary. The parser takes `ExecutionContext` only for `kMaxParseNesting`, as AGENTS.md requires.
- Ownership and lifetimes: finding on the source buffer. D3 teardown order, iterative destruction, reentrancy rules (owning copies of callee and arguments, owning environment lookups), and `Completion` were checked with no further issues.
- Reuse/DRY: centralized type check (Low). Exit-status modulo is in `Integer`; numeric parsing uses `std::from_chars`.
- Naming and layout: High finding. `snake_case` `.hpp`/`.cpp` and `kPascalCase` constants are fine.
- Style-guide conformance at the design level, and error-model placement: one Low on wording. The error hierarchy, `runTopLevel` catch order, and no `assert`/`abort` were checked.
- Performance architecture: one Low on literal allocation. Lookup through a two-level environment chain is fine.
- Evolution: anonymous functions and closures (v0.1.0) and swappable built-in storage are not blocked. `final` on `Function` is a C++ detail and does not prevent plan 0005's Philios-level subclassing.
- Binary footprint: no new runtime dependencies; doctest is test-only.
- Testability: requested seam evaluation (Medium) and the T-FN-6 hook (Medium). T-SEC-8, T-DECL-8, and T-OWN-1 are constructible in isolation.
- Architectural completeness: covered by the findings above.

### Round 2 (2026-11-14)

Confirmed plan ID: `0001`. Plan path: `plans/0001.md` (revision 5, decisions D1–D46). Review path: `plans/0001_review.md`. Grill record: `plans/0001_grill.md`. Gate checked: Plan Review Round 3 is **Go**, and the overall verdict was `Pending architecture review`.

**Architecture verdict:** Go (0 Critical, 0 High, 2 Medium, 1 Low)

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| R1 High: The file layout contradicts `directory-structure.md` | Resolved | D41–D45 and §7.1 map every file to the documented directories plus `runtime/object_model/` and `cli/`. There is one include root `src/`, D32 names `src/cli/source_file.cpp`, and task 1 updates the document (D46). |
| R1 High: Source-buffer lifetime and pipeline owner | Resolved | §3.9: `Module::load` is the only pipeline. The `Module` is allocated on the heap, and the buffer is moved into it before any view exists. `Module` cannot be copied or moved. The interim local `const std::string` pipeline (tasks 4–6) is defined and is removed in task 7. (The plan-reviewer's private-constructor and `make_unique` Low is a construction detail; it does not affect lifetime.) |
| R1 Medium: Read-seam interface undefined | Resolved | §3.2 uses `ChunkReader` (a `std::function` over `std::span<char>` that returns a `std::error_code`), has no POSIX type in the header, and keeps the loop in the `.cpp` as the only place limits are enforced. `loadMainSource` takes no reader. `runTopLevel` uses the same callable form. |
| R1 Medium: AST/validator/evaluator boundary | Resolved | `NodeKind` plus a virtual `kind()`, with no visitor. A virtual child-detach member handles iterative destruction. `evaluate` is declared in `ast.hpp` with forward declarations and defined in `runtime/evaluator/evaluate.cpp`, which has placeholder bodies from task 6. `ASTNode` stays stable across milestones. |
| R1 Medium: T-FN-6 test hook in production code | Resolved | A test-only `ASTNode` subclass is defined in the test file's anonymous namespace. Hooks under `src/` are forbidden, and `CallExpr` accepts any `ASTNode`. |
| R1 Low: No type-identity mechanism | Resolved | §3.6 adds a pure virtual `objectType()` and a single `matchesDeclaredType` in `runtime/evaluator/type_check`, used by the argument, return, and callee checks. `dynamic_cast` and `typeid` are banned for Philios types. |
| R1 Low: Literal objects allocated on every evaluation | Withdrawn (rejection accepted) | The deferral meets my round-1 condition (no `frontend/` dependency on `runtime/object_model/`), and D34 makes performance a non-goal. §3.9 now states the behavior. The same layering reason applies to the new Medium on integer literal parsing below. |
| R1 Low: "Resource exhaustion" wording | Resolved | §4 now limits it to `std::bad_alloc`; depth limits are `RuntimeError` kinds. |
| R1 Low: No mechanical check for D32 | Resolved | §3.2 adds the POSIX-header rule, and T-BUILD-6 scans `src/`. The call-versus-include wording is the plan-reviewer's round 3 Low. |

#### Findings

##### Medium: The location of integer literal parsing is ambiguous, and one reading makes the lexer depend on `runtime/object_model/`
- **Plan reference:** §8 task 3: "`src/runtime/object_model/integer.hpp/.cpp` (literal parsing, exit-status modulo)". §3.3 Tokens: integer literals are a lexical error when out of range; "Parse with `std::from_chars`, require full consumption, and handle `result_out_of_range`". T-LEX-11 (a) checks the token value 42. §3.9 Literal values: "`IntegerLiteral` … stores its parsed value (an `std::int64_t` …)". §11 literal-caching row: making the frontend depend on `runtime/object_model/` "D45 avoids". D45 rationale (grill): "removes the frontend's dependency on runtime types".
- **Problem:** The lexer (task 5) must turn digits into an `std::int64_t` token value and report out-of-range literals as `LexicalError`s at the literal's first byte. Task 3, however, assigns "literal parsing" to `Integer`, in the runtime object model. The plan does not say which component owns the conversion. An implementer can either (a) call `Integer`'s parser from `frontend/lexer/lexer.cpp`, or (b) write a second `from_chars` conversion in the lexer. Option (a) adds a `frontend → runtime/object_model` include. That contradicts the layering rationale the plan uses to reject literal caching, and it pulls `Object` and `ImmutableObject` into the lexer and its unit tests. It also leaves open whether `Integer`'s parser throws a located `LexicalError`, a runtime-side type that would need a `SourceLocation`, or returns a status. Option (b) duplicates the logic, and `Integer`'s "literal parsing" becomes dead code. That conflicts with the style guide's centralized-conversion intent.
- **Impact:** Implementers will place the code and the error boundary differently. With option (a), the layering that D45 set up is quietly broken in the first frontend component.
- **Recommendation:** Assign the conversion to the lexer. For example, a `noexcept` function in `src/frontend/lexer/lexer.cpp`, or its own `src/frontend/lexer/integer_literal.hpp/.cpp`, converts a digit span to `std::optional<std::int64_t>` (or a small result enum), and the lexer raises the `LexicalError`. Remove "literal parsing" from task 3. `Integer` then needs only a constructor from `std::int64_t` and the exit-status modulo. If the composer wants one shared helper instead, place a pure function that depends on no `Object` in `src/common/` and state that `frontend/` and `runtime/` both use it. Either way, state in §7.1 that `frontend/` includes nothing from `runtime/`.

##### Medium: The parser includes `runtime/evaluator/execution_context.hpp` only to read one compile-time constant
- **Plan reference:** §3.4: "`kMaxParseNesting = 256` held by the execution context". §3.9: the execution context "is … passed explicitly by reference to the parser". AGENTS.md Open Decisions, recursion limits: "Limits are named constants held by the execution context". §7.1 puts `execution_context.hpp` in `src/runtime/evaluator/`. D45 rationale: "removes the frontend's dependency on runtime types".
- **Problem:** AGENTS.md requires the limit value to be held by the execution context. It does not require the parser to depend on the context. The parser runs no script code, so the style guide's reentrancy rule (functions that can run Philios code take the context) does not apply. Taking `ExecutionContext&` creates the one remaining `frontend → runtime/evaluator` edge. It needs the full class definition, which owns the `NullObject` instance and the depth guards. Parser unit tests (tasks 6 and 7, T-PARSE-*, T-SEC-3 if parser-based) would then have to build a full execution context. There is no include cycle: `execution_context.hpp` does not include the parser. So this is a layering and testability problem, not a correctness one.
- **Impact:** The frontend is coupled to the runtime from its first milestone. Later plans that add a standalone parse step (tooling, fuzzing per AGENTS.md, the bytecode backend) would inherit a dependency on the runtime state object.
- **Recommendation:** Keep `kMaxParseNesting` as a `static constexpr` member of `ExecutionContext` (this meets AGENTS.md), and pass the parser the value: for example, a `std::size_t maxNesting` constructor argument or a small `ParseLimits` struct in `frontend/parser/parser.hpp`. `Module::load`, and the interim `runProject` pipeline in tasks 4–6, read the value from the context and pass it in. Then update §3.9 ("passed … to the parser") and the parser entry in task 6. If the composer prefers to keep the reference, the plan must state that this is the only sanctioned `frontend → runtime` include, and that `execution_context.hpp` only forward-declares object-model types.

##### Low: `Function` call logic in `object_model/function.cpp` makes the object model and the evaluator depend on each other
- **Plan reference:** §8 task 8: "call logic in `src/runtime/object_model/function.cpp`"; §3.7, the call sequence (call-depth guard, arity and `matchesDeclaredType` checks, fresh `Environment`, body `evaluate`, `Completion`); §7.1 puts the guards, `type_check`, and `completion.hpp` in `runtime/evaluator/`.
- **Problem:** The call sequence uses `ExecutionContext` guards, `matchesDeclaredType`, `Completion`, and `Environment`. All of these except `Environment` live in `runtime/evaluator/`, and `evaluate.cpp` in turn uses `Function`. Defining the call in `object_model/` therefore creates a two-way `object_model ↔ evaluator` dependency. It compiles, because everything is in one library and forward declarations work, but it puts evaluator responsibilities into a value class. Plan 0005's `invoke()` and subclassing will build on this location.
- **Recommendation:** Put the call sequence in `runtime/evaluator/`, for example a `callFunction(ExecutionContext&, …)` function in `evaluate.cpp` or in a new `call.hpp/.cpp`. `Function` then stays a holder of its declaration and its `closureEnv`. Alternatively, state that this is a deliberate, single `object_model → evaluator` edge.

#### Open questions for the developer
- None. Both Medium findings can be fixed by placing the code differently within D45's stated intent. Neither is a language or Open Decision question, and neither blocks implementation.

#### Notes for the security reviewer
- §3.9 buffer handoff: the `Module` is allocated on the heap and cannot be moved, and the buffer is moved in before any view exists. I consider the round-1 dangling-view risk closed by design. Please confirm the interim pipeline in tasks 4–6, where a local `const std::string` must outlive all tokens and AST nodes.
- The plan-reviewer's notes on the unbounded `EINTR` retry and the T-BUILD-6 header-scan spellings remain for you.

#### Areas reviewed
- Core architectural constraints: no global state; the context is passed explicitly, and `null` is one instance per context. There is a single `Module` with a private `Environment`. Memory is reference-counted, with D3's teardown break. `Object`, `ImmutableObject`, and private storage; `closureEnv` and `enclosingScope`; virtual `evaluate`; and missing-name errors are all present. No violations.
- Open Decisions: unchanged from round 1. D41–D46 touch no Open Decision. The recursion-limit interim default ("held by the execution context") is met, and the parser-dependency Medium keeps it met.
- Component boundaries and dependencies: I checked every cross-directory edge in §7.1. `runtime → frontend` (`Module::load`, `type_check` → `TypeName`, `evaluate.cpp`) is fine. `cli → runtime` is fine. `frontend → runtime` has two edges, both covered by the Medium findings above. `object_model ↔ evaluator` is the Low finding.
- Ownership and lifetimes: the buffer handoff, the non-movable `Module`, the interim pipeline, the teardown order, owning copies across reentrant calls, and iterative destruction through the virtual child-detach member are sound.
- Reuse and DRY: the integer-parsing Medium. There is one type check and one read loop.
- Naming and layout: §7.1 files, directories, and include style match the style guide. The style guide's "Includes section" target is the plan-reviewer's round 3 Medium.
- Style guide and error model: `std::function` seams keep bodies in the `.cpp` files. Error types live in `common/`. The private-constructor and `make_unique` issue is the plan-reviewer's Low.
- Performance: per-evaluation literal allocation is accepted under D34. No hard-to-optimize structures were added.
- Evolution: v0.1.0 closures, plan 0005, and swappable storage are not blocked. The call-logic Low affects plan 0005.
- Binary footprint: no change. `std::function` is used only for two cold-path seams.
- Testability: the seams, T-FN-6, T-SEC-5 (the plan-reviewer's Low on `SourceLocation`), and parser tests (see the Medium).
- Architectural completeness: an implementer could build the plan, apart from the placement ambiguities above.

### Round 3 (2026-11-14)

Confirmed plan ID: `0001`. Plan path: `plans/0001.md` (revision 6, decisions D1–D47). Review path: `plans/0001_review.md`. Grill record: `plans/0001_grill.md`. Gate checked: Plan Review Round 4 is **Go**, and the overall verdict was `Pending architecture review`.

**Architecture verdict:** Go (0 Critical, 0 High, 0 Medium, 1 Low)

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| R2 Medium: Integer literal parsing location; lexer could depend on `runtime/object_model/` | Resolved | §3.3: `parseDecimalInteger(std::string_view) noexcept` → `std::optional<std::int64_t>` in `src/frontend/lexer/integer_literal.hpp/.cpp`; the lexer raises the located `LexicalError`. Task 3 drops literal parsing from `Integer`. §7.1 adds the "Dependency direction" rule (`frontend/` includes nothing from `runtime/` or `analysis/`). |
| R2 Medium: Parser receives `ExecutionContext` only to read `kMaxParseNesting` | Resolved | §3.4 and §3.9: the constant stays a `static constexpr` member of the execution context (meets AGENTS.md), and the parser receives a `std::size_t maxNesting` value declared in `parser.hpp`. `Module::load` and the interim pipeline pass it (tasks 6, 7). Parser unit tests no longer need a context. |
| R2 Low: Call sequence in `object_model/function.cpp` creates an `object_model ↔ evaluator` cycle | Resolved | §3.7: `callFunction(ExecutionContext&, …)` in `src/runtime/evaluator/call.hpp/.cpp`; `Function` holds only its declaration and `closureEnv`; §7.1 forbids `object_model → evaluator` includes; tasks 7 and 8 match. |

#### Findings

##### Low: The T-CLI-10 `ctest` split couples CMake to a doctest case name that the plan does not fix, and a mismatch passes silently
- **Plan reference:** §7 "T-CLI-10 registration (D47)" ("runs `philios_tests` filtered to that test case (and the main `philios_tests` entry excludes it)"; `SKIP_REGULAR_EXPRESSION`); §8 task 4; §9 T-CLI-10.
- **Assessment (requested by the plan-reviewer):** The split fits the test layout. The test source sits in `tests/unit/cli/` (D44, mirroring `src/cli/`), its helpers stay in the file's anonymous namespace, it compiles into the single `philios_tests` executable, and only its `ctest` registration differs. That keeps the three test tiers (unit in `philios_tests`, CLI golden in `tests/cli/`, build probes in `tests/build/`) intact, adds no target, puts nothing test-only under `src/`, and T-BUILD-6's `src/`-only scan is unaffected. Using `SKIP_REGULAR_EXPRESSION` on a dedicated entry is the right way to surface a skip without making the whole unit suite report "skipped". The fork, `setsid`, and `unshare` all run in the child, which is single-threaded after `fork`, so `unshare(CLONE_NEWUSER)` is not blocked by doctest or sanitizer threads in the parent.
- **Problem:** The two `ctest` entries and the test source are linked only by a test-case name string, which the plan does not state, and the plan does not say which doctest options do the filtering (`--test-case=` / `--test-case-exclude=`). If the name in CMake and the name in the source drift apart (a typo, or a later rename), the filtered entry matches no test case, doctest exits 0, and `ctest` reports T-CLI-10 as **passed** while it never ran. The main entry would then also run it, where a skip line is not detected and counts as a pass. This silently disables the only test of D47.
- **Impact:** Not a blocker; an implementer can pick a name. But the failure mode is a silent false pass of a developer-required security check, which neither ASan nor the skip mechanism would reveal.
- **Recommendation:** In §7, fix the exact doctest test-case name (for example `"T-CLI-10 terminal device main.phil"`), state that the dedicated entry uses `--test-case=<name>` and the main entry `--test-case-exclude=<name>`, both from one CMake variable, and guard against zero matches. For example, give the dedicated entry a `FAIL_REGULAR_EXPRESSION` that matches doctest's summary when no test case ran (`test cases: *0 *\|`), or have the test print a `T-CLI-10 RAN` line and treat its absence as failure. Choose one mechanism and state it.

#### Open questions for the developer
- None. The Low above is a test-wiring detail within the composer's discretion.

#### Notes for the security reviewer
- T-CLI-10 can be silently disabled by a test-case name mismatch between CMake and the source (Low above); relevant to whether D47's `O_NOCTTY` is ever verified, in addition to the AppArmor skip the plan-reviewer flagged.
- The plan-reviewer's two T-CLI-10 Lows (how the child reports its skip reason across `_exit`; `_exit` as an unrecorded ERR50 test-only exception) were not re-reviewed here; they do not affect the architecture.

#### Areas reviewed
- Core architectural constraints: unchanged and satisfied. The execution context is still passed explicitly and never global; the parser's limit is a value, not shared state; `Function` holds `closureEnv`; `Object`/`ImmutableObject` and private storage are intact.
- Open Decisions: D47 touches none. The recursion-limit interim default ("named constants held by the execution context") is still met after the parser change.
- Component boundaries and dependencies: rechecked every edge in §7.1. `frontend → common` only; `runtime → frontend` (`Module::load`, `type_check`, `evaluate.cpp`, `Function` → `FunctionDecl`); `cli → runtime`; no `object_model → evaluator`. No cycles between directories.
- Ownership and lifetimes: the security-driven changes (intrusive `unique_ptr` AST worklist with a non-virtual helper and no virtual call on the object being destroyed, loop-unlinked environment chain, table swap for the D3 cycle break, all `noexcept` and non-allocating) are sound and keep ownership single-rooted. The bounds-checked lexer `peek` over a `std::string_view` keeps the buffer owner unchanged.
- Reuse and DRY: one integer conversion (frontend), one type check, one call sequence, one read loop.
- Naming and layout: `integer_literal.hpp/.cpp` and `call.hpp/.cpp` follow `snake_case` and §7.1; T-CLI-10 lives in `tests/unit/cli/` per D44.
- Style guide and error model: `parseDecimalInteger` returns a status and the lexer owns the located error, so no runtime type carries frontend locations. `_exit` in the test child is the plan-reviewer's Low.
- Performance: no change; parser limit passed by value removes a context dependency at no cost.
- Evolution: the frontend can now be reused by a standalone parse step, fuzzing, or the bytecode backend without the runtime. Plan 0005's `invoke()` can build on `callFunction`.
- Binary footprint: no change; T-CLI-10's POSIX usage is test-only.
- Testability: lexer, parser, and `parseDecimalInteger` are now testable without any runtime object; T-CLI-10 registration assessed above.
- Architectural completeness: an implementer can build the plan without further architectural decisions.

### Round 4 (2026-11-14)

Confirmed plan ID: `0001`. Plan path: `plans/0001.md` (revision 7, decisions D1–D48). Review path: `plans/0001_review.md`. Grill record: `plans/0001_grill.md`. Gate checked: Plan Review Round 5 is **Go**, and the overall verdict was `Pending architecture review`.

**Architecture verdict:** Go (0 Critical, 0 High, 0 Medium, 0 Low)

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| R3 Low: The T-CLI-10 `ctest` split depends on an unfixed doctest case name, and a mismatch passes silently | Resolved | §7 fixes the name `T-CLI-10 terminal device main.phil` in one CMake variable, `PHILIOS_TCLI10_TEST_CASE`, which both `--test-case=` (dedicated entry) and `--test-case-exclude=` (main entry) use, and the `TEST_CASE` uses the same literal. One mechanism is chosen: a zero-test `FAIL_REGULAR_EXPRESSION`, self-checked by the T-BUILD-7 `WILL_FAIL` guard entry. A name mismatch now fails rather than passing. The plan-reviewer's Round 5 Low (the example pattern's `\|` becomes alternation in CMake and over-matches, and T-BUILD-7 checks only the positive case) leaves a residual wiring defect. It fails loudly, because the dedicated entry would fail on every run that executes, or shows up as a skip on a skip-only host, which D48 makes visible in the report. It does not reopen the silent false pass, so I did not raise it again. |

#### Findings
None. I checked every revision-7 change for architectural effects:
- D48 (skip acceptable, per-build report in task 9) is a verification-policy decision. It changes no component, ownership, or dependency.
- T-BUILD-7 adds a `ctest` entry that reuses the `philios_tests` executable. It adds no target, no test-only code under `src/`, and no new test tier. It keeps the three-tier layout I assessed in Round 3.
- T-CLI-10's exit-code protocol, named constants, catch-all, and parent-side checks all stay in the test file's anonymous namespace under `tests/unit/cli/` (D44, §7.1). Nothing reaches `src/` or `ExecutionContext`. The child calls only `runTopLevel` and `loadMainSource` through their public interfaces. That matches the seam pattern used elsewhere (no production hooks).
- The `_exit` exception (§7, §4) is limited to that one test file, recorded like D36, and consistent with the style guide's suppression rule. Shipped code is unaffected.
- The §3.9 unlink invariant states the move order the intrusive worklist needs, so ownership stays single-rooted and destruction stays non-recursive, `noexcept`, and non-allocating. T-OWN-4's 200,000-argument `CallExpr` and 200,000-deep chain are built directly from frontend node types. `IntegerLiteral` stores an `std::int64_t`, so the test needs no runtime object, which keeps the AST testable in isolation.
- The §3.3 rule for a `\` at the end of a line or of the file stays inside the lexer and its bounds-checked `peek`. It adds no dependency. The remaining location-precedence edge case (plan-reviewer Round 5 Medium) is a specification detail with no architectural effect.

#### Open questions for the developer
- None.

#### Notes for the security reviewer
- The plan-reviewer's Round 5 Low on the `FAIL_REGULAR_EXPRESSION` escaping (`\|` vs `\\|` or `[|]`) and on whether a `SKIP_REGULAR_EXPRESSION` match can mask a `FAIL_REGULAR_EXPRESSION` match under CMake 3.28 bears on whether D47 is verified. I judge that it causes no silent false pass, but please confirm the CTest precedence.

#### Areas reviewed
- Core architectural constraints: unchanged and satisfied. There is no global state, and test-only state lives only in the test file's anonymous namespace. The execution context is passed explicitly. Memory is reference-counted, and destruction is iterative with the unlink invariant. The `Object`/`ImmutableObject` hierarchy, `closureEnv`/`enclosingScope`, and virtual `evaluate` are intact.
- Open Decisions: D48 touches none, and §6 is unchanged.
- Component boundaries and dependencies: the §7.1 direction rules are unchanged (`frontend → common` only, no `object_model → evaluator`). Revision 7 adds no cross-directory edges.
- Ownership and lifetimes: the intrusive worklist unlink order, and RAII cleanup in the T-CLI-10 parent (master, pipe, `mkdtemp` directory).
- Reuse and DRY: one test-case name variable feeds both entries, and the zero-test pattern is defined once and reused by T-BUILD-7.
- Naming and layout: `PHILIOS_TCLI10_TEST_CASE` follows CMake's upper-case convention. T-CLI-10 is in `tests/unit/cli/`. T-BUILD-7 sits with the other build probes under §7.
- Style guide and error model: `_exit` is recorded as a test-only ERR50 exception; §4 states that `src/` never calls `exit`, `_exit`, or `_Exit`. No new error conditions.
- Performance and binary footprint: no change.
- Evolution: no change. The plan 0005 and v0.1.0 paths are still open.
- Testability: T-OWN-4 and T-CLI-10 can each be built and run without a full interpreter run.
- Architectural completeness: an implementer can build the plan without further architectural decisions.

## Security Review

### Round 1 (2026-11-14)

Confirmed plan ID: `0001`. Plan path: `plans/0001.md` (revision 5, decisions D1–D46). Review path: `plans/0001_review.md`. Grill record: `plans/0001_grill.md`. Gate checked: Architecture Review Round 2 is **Go**, Plan Review Round 3 is **Go**, and the overall verdict was `Pending security review`.

**Security verdict:** Go (0 Critical, 0 High, 2 Medium, 4 Low)

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| None | — | This is the first security round. |

#### Assessment of the items the other reviewers flagged
- **Source-buffer handoff (§3.9) and the interim pipeline (tasks 4–6):** I found no use-after-free risk. `Module::load` takes the `std::string` by value and moves it into a heap-allocated, non-copyable, non-movable `Module` before any view exists. Members are destroyed in the order environment, then AST, then buffer. Error objects copy their text. The only value that leaves execution is converted to `int` before teardown. In the interim pipeline, the buffer is a local `const std::string`, so `std::move` on it copies and cannot change the original, and every token and AST node is destroyed before `runProject` returns. Moves before the first view is created are harmless, even with the small-string optimization.
- **`ChunkReader` seam:** `loadMainSource` takes no reader. The only production reader is file-local to `source_file.cpp`. The limit, the retry, and the discard of partial data all happen in the one loop that the tests exercise. No script can reach the seam. Two clarifications are recorded below as Lows.
- **Unbounded `EINTR` retry:** I found no defect. The shipped reader can see `EINTR` only when a signal with an installed handler interrupts it. The plan installs no handler (§2 Non-goals), so each retry needs a signal sent from outside the process. Script input cannot cause it, and it cannot cause undefined behavior. A fake that never stops returning `interrupted` exists only in tests. A retry cap is optional.
- **T-BUILD-6 text scan:** Alternative spellings are a gap in the regression check (Low below). They do not open a path in this plan, because `source_file.cpp` is the only code that loads files.
- **§5.1 accepted risks:** D28(a) (lone CR and mixed line endings) is a risk of code looking different in a viewer than to the lexer. It does not cause a crash or undefined behavior. Under the interim trust model it is acceptable, because the diagnostics escape CR (T-SEC-7) and a raw CR in a string is rejected. D24 (8 MiB stack) depends on the environment, not on input alone. T-SEC-4 checks it in the Debug, ASan, and Release builds. I accept both as recorded and do not reopen them. The lexer's handling of a CR at end of file is covered by the second Medium below.

#### Findings

##### Medium: Iterative destructors allocate a worklist and can terminate the process while unwinding from out of memory
- **Plan reference:** §3.9 "AST and environment destructors detach children into a local worklist"; §3.9 the `Module` destructor "moving the environment's table into a local and destroying it there"; §5 Memory: "`std::bad_alloc` is caught by the top-level handler and reported"; D30.
- **Rule:** CERT DCL57-CPP (do not let exceptions escape destructors), ERR50-CPP; style guide, Functions & Classes ("Destructors must never throw") and Errors & Diagnostics (`std::bad_alloc` must be caught and reported).
- **Security impact:** Destructors are implicitly `noexcept`. A worklist that grows, such as a `std::vector` with `push_back`, can throw `std::bad_alloc` while a destructor runs. The process then calls `std::terminate` instead of reaching the D30 out-of-memory report. This is most likely on the path the plan designs for: when the lexer, parser, or evaluator runs out of memory, unwinding destroys partly built ASTs and the `Module` while memory is still nearly exhausted. A large source file under a low memory limit can trigger it. T-SEC-8 throws `std::bad_alloc` from a body that has no AST, so it does not cover this path.
- **Recommendation:** In §3.9, state that every AST, environment, and `Module` destructor (and `Module::release()`) is `noexcept` and never lets an allocation failure escape. Name the strategy. For example, build the worklist so that releasing nodes never allocates: reserve its capacity when the structure is built, or splice child lists that already exist. Alternatively, catch `std::bad_alloc` inside the destructor and fall back to recursive release. The fallback is safe for parser-built ASTs because their depth is bounded by `kMaxParseNesting`, and for environment chains because their depth is bounded by `kMaxCallDepth`. Add a unit test that destroys a nested AST while the worklist allocation fails, or document why that test cannot be built.

##### Medium: ASan cannot detect lexer over-reads into a `std::string` buffer, and no test covers lookahead at end of file
- **Plan reference:** §5 "the lexer never reads past the buffer and validates every index"; §3.2 the source is a `std::string` that can be reserved from `sizeHint`; §3.3 line breaks (CR followed by LF), `/*`, `*/`, `//`, escapes, `${`, `;`, and digit-then-identifier rules; T-LEX-1 covers only truncated UTF-8 at end of file.
- **Rule:** CERT CTR50-CPP and STR53-CPP; style guide, Types & Data ("Validate every index … `operator[]` on `span`, `string_view`, `vector`, and `string` is unchecked"); AGENTS.md Verification (boundary and malformed-input regression tests).
- **Security impact:** Many lexer rules look one byte ahead: CR then LF, `/` then `*` or `/`, `*` then `/`, `\` then the escape character, `$` then `{`, and a digit then an identifier character. Each lookahead can read past the end when the trigger is the last byte of the file. A `std::string` always has a NUL byte at `data()[size()]`, and after `reserve(sizeHint)` it may have spare capacity after that. A read one byte (or more) past the end therefore lands in allocated memory. ASan does not report it, and the test passes while the lexer reads uninitialized bytes from the spare capacity. The sanitizer acceptance gate cannot find this class of bug.
- **Recommendation:** Specify that the lexer reads input only through one checked accessor (for example, a `peek(offset)` that returns "end of input" after a bounds check), and that the lexer takes a `std::string_view`. Add lexer unit tests (task 5) that pass a `std::string_view` over heap storage of exactly the input's size (for example a `std::vector<char>` or a `std::unique_ptr<char[]>` sized with no terminator), so ASan detects any over-read. Cover a file that ends with each of these: a lone CR, `/`, `/*` with no closing `*/` and a final `*`, a string ending in `\`, a string ending in `$`, `;`, a digit, and an incomplete UTF-8 lead byte. Reject each case or accept it as §3.3 specifies. Fuzzing (its own plan) is the long-term defense for this input surface. I recommend creating that plan soon after this one.

##### Low: `ChunkReadResult` does not say what an error with a nonzero byte count means
- **Plan reference:** §3.2 "`ChunkReadResult` holding a `std::size_t` byte count and a `std::error_code` error … an error equal to `std::errc::interrupted` means 'retry'"; the only checked misreport is "a reported byte count larger than `destination.size()`".
- **Rule:** Style guide, Types & Data (validate lengths) and Errors & Diagnostics (never return a fake success).
- **Security impact:** A result that holds both an error and a nonzero count is valid by the type but has no defined meaning. Implementers may disagree about whether the bytes are kept, for example on retry. That leads to duplicated or dropped source bytes, and possibly a buffer that does not match the file. This does not cause undefined behavior while the count is at most `destination.size()`.
- **Recommendation:** State that a result with an error must report 0 bytes, and that `readBoundedSource` raises `InternalError` otherwise, in the same way as an over-reported count. Add that case to T-CLI-8. Optionally state that the `EINTR` retry is deliberately unbounded, because only signals sent from outside the process can cause it.

##### Low: How the real reader captures the RAII descriptor is not specified, and `std::function` requires a copyable callable
- **Plan reference:** §3.2 "a function in an anonymous namespace of `src/cli/source_file.cpp` that captures the RAII descriptor"; `ChunkReader` is a `std::function`.
- **Rule:** Style guide, Ownership & Lifetime (RAII; a lambda must not outlive what it captures by reference, CERT EXP61); Functions & Classes (Rule of Zero/Five).
- **Security impact:** `std::function` accepts only copyable callables. An implementer who captures the descriptor by value might make the RAII class copyable to satisfy that requirement. Each copy would then close the same descriptor, and a closed descriptor number can be reused by an unrelated file.
- **Recommendation:** State that the descriptor class cannot be copied (move-only or immovable) and that the reader captures it by reference. This is allowed because the `std::function` is a temporary that does not outlive `loadMainSource`.

##### Low: T-BUILD-6 does not match alternative header spellings
- **Plan reference:** T-BUILD-6 scans only for `<fcntl.h>`, `<unistd.h>`, and `<sys/stat.h>`.
- **Rule:** AGENTS.md Verification (regression checks); D32 confinement.
- **Security impact:** On glibc, `<sys/fcntl.h>` and `<sys/unistd.h>` declare the same functions. `<sys/syscall.h>` with `syscall()` would bypass the rule entirely. A later change could add a second, unchecked file-loading path that skips `O_NOFOLLOW`, the regular-file check, or the size cap, and T-BUILD-6 would still pass. This does not affect this plan's code.
- **Recommendation:** Make the scan match these spellings and `#` / `include` lines with extra whitespace. Add `<sys/fcntl.h>`, `<sys/unistd.h>`, and `<sys/syscall.h>` to the list. Keep the plan-reviewer's wording fix: the call ban is enforced only indirectly.

##### Low: A `kind()` check followed by `static_cast` is sound only for the `final` node classes, but test-only `ASTNode` subclasses also exist
- **Plan reference:** §3.9 "The validator identifies nodes with `kind()` and a `static_cast` to the matching `final` class"; `CallExpr` "accepts any `ASTNode`"; T-FN-6 and T-DECL-8 use test-only subclasses.
- **Rule:** Style guide, Casts ("Never use `static_cast` to downcast a runtime object unless its dynamic type has already been verified").
- **Security impact:** `NodeKind` has exactly one value per concrete production class, so a test-only subclass must return a production class's kind. If such a node ever reaches the validator, for example as a `CallExpr` argument, the `static_cast` turns it into an object of the wrong type, which is undefined behavior. The risk is limited to tests today, but it weakens the guarantee that the downcast is verified.
- **Recommendation:** State that the validator's switch over `NodeKind` has no default case that casts, and that test-only subclasses are never passed to the validator. Alternatively, reserve a `NodeKind` value for test nodes that the validator rejects with `InternalError`.

#### Open security questions for the developer
- Should `O_NOCTTY` be added to the D32 `open` flags? Opening a terminal device named `main.phil` without it can make that terminal the process's controlling terminal before `fstat` rejects the file. Only a local device node can trigger this, and symlinks are already rejected. This does not block implementation. Because D32 lists the exact flag set, only the developer can change it.

#### Key assumptions
- The interim trust model applies, and the plan claims no support for running untrusted code (§5). Unbounded CPU time and the lone-CR display risk are therefore not findings.
- The lexer classifies characters with explicit ASCII range checks or follows the style guide's `<cctype>` rule (CERT STR37-C). The plan does not say which.
- `fstat`'s `st_size` (`off_t`) is compared with the limit using `std::cmp_*` or `std::in_range`, and `read`'s `ssize_t` result is converted only after a check, as the style guide requires. The plan's "All size arithmetic is checked" covers this.
- `runTopLevel` is not `noexcept`. If reporting inside a handler throws, `main`'s last-resort `catch (...)` returns 70.

#### Areas reviewed
- Reentrancy: owning copies of the callee and arguments; argument evaluation into named locals (CERT EXP50) with the D6 order recorded; no references into environment tables held across calls; no script comparators (not applicable).
- Ownership and lifetime: the buffer handoff and interim pipeline, D3 teardown with no weakened edges, `shared_ptr<const FunctionDecl>`, error objects copying their text, iterative destruction (Medium), `make_unique` with a private constructor (the plan-reviewer's Low, not a security issue).
- Stack and resources: parser nesting 256, call depth 1,000, evaluation depth 10,000 with RAII guards and a precedence rule, the lexer indentation stack bounded by the source cap, the 2 MiB cap enforced while reading, `std::bad_alloc` handling (Medium).
- Numeric safety: `std::from_chars` with full consumption and out-of-range handling, no arithmetic, exit-status `% 256` (no `% -1` case), no float conversions, no enum decoding from integers.
- Input handling: UTF-8 validation, control and bidi characters, line breaks, comments, continuation, indentation, end-of-file lookahead (Medium), the open-then-check sequence with `O_NOFOLLOW`, `O_NONBLOCK`, and `O_CLOEXEC`, the TOCTOU-safe size bound, no imports.
- Errors: the `runTopLevel` catch chain and order, `main`'s last-resort `catch (...)`, no `assert`/`abort`/`exit`/`terminate`, `std::runtime_error`-based errors (CERT ERR60), a module that fails to load is never visible, and no dynamically initialized statics outside the D36 test-only exception.
- Diagnostics: escaping of control characters, CR, and ESC (T-SEC-7); no script text used as a format string; no host details beyond `main.phil`.
- Global state and concurrency: no globals, no signal handler, single-threaded.
- Verification: warning flags, `clang-tidy` `cert-*`/`bugprone-*` as errors on all three targets, ASan+UBSan+LSan for the full suite, strict C++20 with extensions off, T-SEC and T-OWN boundary tests. Lexer and parser fuzzing is deferred to its own plan (recommended above).

### Round 2 (2026-11-14)

Confirmed plan ID: `0001`. Plan path: `plans/0001.md` (revision 6, decisions D1–D47). Review path: `plans/0001_review.md`. Grill record: `plans/0001_grill.md`. Gate checked: Architecture Review Round 3 is **Go**, Plan Review Round 4 is **Go**, and the overall verdict was `Pending security review`.

**Security verdict:** Go (0 Critical, 0 High, 0 Medium, 3 Low)

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| R1 Medium: Iterative destructors allocate a worklist and can terminate while unwinding from OOM | Resolved | §3.9: every AST, `Environment`, and `Module` destructor, `detachChildren`, and `Module::release()` are `noexcept` and allocate nothing (intrusive `unique_ptr` link list, loop-unlinked environment chain, table swap with an empty local). No virtual call on the object being destroyed (CERT OOP50). T-OWN-5 adds `static_assert`s for nothrow destruction. Not building an allocation-failure injection test is justified: it would need a replaced global `operator new` with mutable global state, which the style guide forbids. |
| R1 Medium: ASan cannot see lexer over-reads into a `std::string`; no end-of-input lookahead tests | Resolved | §3.3 "Reading the input": the lexer takes a `std::string_view` and reads only through one bounds-checked `peek(offset)` with checked arithmetic. The UTF-8 helpers check every index. T-LEX-12 lexes every lookahead trigger from exact-size heap storage under ASan. (The plan-reviewer's Round 4 Medium on where the error for `\` followed by a line break is reported is a diagnostic-location issue, not a safety issue. Both readings stay inside the checked accessor.) |
| R1 Low: `ChunkReadResult` with an error and a nonzero byte count | Resolved | §3.2: a result with an error must report 0 bytes, otherwise `InternalError` with no partial buffer. T-CLI-8 covers both `io_error` and `interrupted` with 5 bytes. The unbounded `EINTR` retry is explained. |
| R1 Low: Descriptor capture and `std::function` copyability | Resolved | §3.2: the descriptor class deletes copy and move. The lambda captures it by reference, and the `ChunkReader` temporary never outlives the local descriptor (CERT EXP61). |
| R1 Low: T-BUILD-6 misses alternative spellings | Resolved | T-BUILD-6 covers `<sys/fcntl.h>`, `<sys/unistd.h>`, and `<sys/syscall.h>` (banned in all of `src/`), ignores whitespace, accepts either delimiter, and checks its own pattern. Transitive inclusion is left to code review, which is acceptable because the plan does not claim the probe enforces the call ban. |
| R1 Low: Test-only `ASTNode` subclasses reaching the validator's `static_cast` | Resolved | §3.9: only the validator downcasts. Its `switch` has no casting `default`, and unhandled kinds raise `InternalError`. Test-only subclasses never reach the validator. |
| R1 Open question: `O_NOCTTY` | Resolved | D47: the flag is in the §3.2 `open` call with its reason, in §5, and in task 4, and T-CLI-10 tests it. |

#### Assessment of D47 and T-CLI-10
- **The flag:** `O_NOCTTY` closes the round-1 gap. Combined with `O_NOFOLLOW`, `O_NONBLOCK`, open-then-`fstat`, and the regular-file check, opening a device node named `main.phil` now has no side effect before rejection.
- **The test mechanism is sound and cannot be fooled by its own precondition.** The precondition `open` uses `O_NOCTTY`. Without it, that open would itself give the session leader a controlling terminal, and the later `/dev/tty` check would pass even if `loadMainSource` lacked the flag. Closing a descriptor does not release a controlling terminal, so the order matters, and the plan gets it right. `isatty` and `fstat` do not acquire a terminal. After `setsid()`, the child is a session leader with no controlling terminal, and the never-opened slave has no session. Without `O_NOCTTY`, opening the slave would therefore acquire it, so the test detects a regression whenever it runs.
- **`_exit` in the child:** from a security standpoint this is the correct choice, not merely a tolerated one. `exit` would run the parent's `atexit` handlers and LSan's exit-time check against a copy of the parent's heap, and flush duplicated stdio buffers. A return or an exception would unwind into doctest, and the child would go on running the rest of the suite inside the new user and mount namespaces. I agree with the plan-reviewer's Round 4 Low that the plan should record it as a test-only exception to the ERR50 rule. The Low below adds the rule that keeps a child from ever reaching doctest.
- **Thread safety after `fork`:** the child calls functions that are not async-signal-safe (`runTopLevel`, allocation, exceptions). That is safe only because the test process is single-threaded when it forks (doctest runs serially; AGENTS.md concurrency default). `unshare(CLONE_NEWUSER)` fails with `EINVAL` in a multithreaded process, so a stray thread would produce a skip, not undefined behavior in a passing run.
- **AppArmor on Ubuntu 24.04:** on a stock Ubuntu 24.04 host with AppArmor active, `kernel.apparmor_restrict_unprivileged_userns=1` either makes `unshare(CLONE_NEWUSER)` fail or moves the child into a profile with no capabilities, so the bind mount fails with `EPERM`. Either way the test skips at a named step (step 3 catches both), so this is a clear skip, not a false pass. WSL2 kernels may not enable AppArmor, so the test may run there. The developer explicitly allowed skips (D47), so a skip is not a finding. The risk is that on the target platform D47 may be checked only by code review. See the second Low.

#### Findings

##### Low: T-CLI-10's child-to-parent protocol can report a false pass
- **Plan reference:** T-CLI-10 steps 3–6 ("The child reports pass, skip (with the reason), or which check failed through its exit status and ends with `_exit`"; "the test writes `T-CLI-10 SKIPPED: <reason>`"; "stderr redirected to a pipe read by the parent"); §7 `SKIP_REGULAR_EXPRESSION`.
- **Rule:** AGENTS.md Verification (regression tests must detect regressions); style guide, Errors & Diagnostics ("Never … return a fake success value", CERT ERR50 for the `_exit` exception).
- **Security impact:** This affects only the D47 regression check, not the shipped binary. The plan does not exclude these false passes:
  1. **An exception that escapes the child body**, or a doctest `CHECK`/`REQUIRE` used in the child, unwinds into doctest. The child then runs the rest of the suite and exits with doctest's status, usually 0. If 0 means pass, the parent reports a pass while the checks never completed.
  2. **Ambiguous pass code.** Sanitizer reports in the child exit with ASan's default `exitcode` 1, and a signal kills the child. If the parent treats only "non-zero" or a range of codes as failure, it may map these to a skip or a pass.
  3. **The skip line overrides failure.** CTest's `SKIP_REGULAR_EXPRESSION` marks a test as skipped when its output matches, even when it fails. If the skip line is printed in any path where a check failed, the failure is hidden.
  4. **Pipe handling.** If the parent keeps its write end of the stderr pipe open, or calls `waitpid` before draining the pipe, the test can hang until the default `ctest` timeout (1,500 s) instead of failing promptly.
- **Recommendation:** In T-CLI-10, state these rules:
  - Wrap the whole child body in a `try` with a final `catch (...)` that maps every exception to a dedicated failure code. Every path ends in `_exit`. The child uses no doctest macros.
  - Use a pass code that is neither 0 nor 1, a distinct code for each skip step, and a distinct code for each failed check. The parent treats every other outcome as failure, including `WIFSIGNALED`, unknown codes, and 0.
  - The parent prints the `T-CLI-10 SKIPPED:` line only for a skip code. Skips can happen only in step 3, before any check runs.
  - The parent closes its write end, reads the pipe to end of file, then waits for the child, and checks the stderr prefix itself.
  - Give the dedicated `ctest` entry a `TIMEOUT` (for example 10 s, like the CLI driver).
  - Create the temporary directory with `mkdtemp`, so another local user cannot plant a file there.
  - Record `_exit` as the test-only ERR50 exception, as the plan-reviewer recommended. `src/` never calls it.

##### Low: T-CLI-10 can be silently disabled, and on the target platform it may only ever skip
- **Plan reference:** §7 "T-CLI-10 registration (D47)"; §9 introduction ("T-CLI-10 may be reported as skipped"); §8 task 9.
- **Rule:** AGENTS.md Verification; D47 (a terminal-device test was explicitly requested).
- **Security impact:** I agree with the architecture Round 3 Low. The dedicated `ctest` entry and the doctest case are linked only by a name the plan does not fix. A mismatch makes the filtered run match nothing, and doctest then exits 0, which reports a **pass**. Meanwhile the main entry runs the case with no skip detection, so a skip there also counts as a pass. Separately, on stock Ubuntu 24.04 with AppArmor's user-namespace restriction (see above), the test can only skip. In either case the only automated check of D47 never runs, and nothing tells the developer.
- **Recommendation:** Apply the architecture reviewer's fix:
  - Fix the exact test-case name in one CMake variable, used for both `--test-case=` and `--test-case-exclude=`.
  - Make a run that matches zero test cases fail. For example, the test prints `T-CLI-10 RAN`, and the dedicated entry treats its absence as failure, or uses a `FAIL_REGULAR_EXPRESSION` on doctest's zero-test summary.
  - In task 9, require the acceptance report to state, for each build, whether T-CLI-10 passed or was skipped (with the reason). A skip on the developer's platform is then visible, and D47 is known to be verified only by code review there.

##### Low: The intrusive worklist must unlink a node before destroying it
- **Plan reference:** §3.9 AST: each node has "a private `std::unique_ptr<ASTNode>` link member"; the helper "repeatedly takes the head node off the list … and destroys it".
- **Rule:** Style guide, Ownership & Lifetime (destruction must not recurse once per node).
- **Security impact:** The link member is itself an owning `unique_ptr`. If a node is destroyed while its link is still set, for example if the helper destroys the head before moving its link into the list head, the base class's member destructor destroys the next node, and that one destroys its next. This brings back per-node recursion through the links, and a long list overflows the native stack. The plan's wording ("takes the head node off the list") implies the correct order but does not state the invariant. T-OWN-4 and T-OWN-5 would catch the error only if their lists are long enough.
- **Recommendation:** Add one sentence to §3.9 stating that a node's link is always empty when the node is destroyed. The helper moves the head out, then moves that node's link into the head (`node = std::move(head); head = std::move(node->link_);`) before `detachChildren` and destruction. Make sure T-OWN-4's structure, which is "far deeper than the limit", produces a long worklist, for example a wide `CallExpr` or a long chain, so that a mistake here overflows the stack under ASan.

#### Open security questions for the developer
- Is a skipped T-CLI-10 on your Ubuntu 24.04 machine acceptable as the final verification of D47, so that the flag is checked only by code review there? Or do you want an opt-in run during acceptance, for example temporarily allowing unprivileged user namespaces or running the single test as root with `mknod`, which D47 currently rules out? This does not block: D47 already allows skips, and the flag itself is specified.

#### Key assumptions
- The interim trust model applies, and the plan claims no support for running untrusted code (§5).
- The test process is single-threaded when T-CLI-10 forks. doctest runs serially, and the plan adds no threads.
- The toolchain's default-constructed empty environment table does not allocate, and its `swap` does not throw. This is true for libstdc++'s `std::unordered_map` and `std::map`, as §3.9 states for D9's supported toolchain.
- The AppArmor behavior described above applies to stock Ubuntu 24.04 hosts. WSL2 kernels may differ. The test's step-3 checks produce a skip with a reason either way.

#### Areas reviewed
- Reentrancy: unchanged from Round 1. `callFunction` in `runtime/evaluator/call.cpp` holds owning copies, arguments are evaluated into named locals (D6, CERT EXP50), and there are no script comparators.
- Ownership and lifetime: the intrusive AST worklist, `FunctionDecl` shared ownership, environment chain unlinking, the `Module` table swap, the passkey constructor with `make_unique`, the non-movable `Module` and buffer handoff, the interim pipeline, and the descriptor captured by reference. One Low above.
- Stack and resources: parser nesting passed as a value, call and evaluation depth guards taking a `SourceLocation`, non-allocating `noexcept` destruction, the 2 MiB cap in the single read loop, and the `std::bad_alloc` report written without allocating.
- Numeric safety: `parseDecimalInteger` (`std::from_chars`, full consumption, `result_out_of_range` returns no value, 1,000-digit test), exit modulo 256, no arithmetic, no float conversions, no integer-to-enum decoding.
- Input handling: the bounds-checked `peek`, UTF-8 helpers over `string_view`, T-LEX-12 on exact-size storage, `O_NOCTTY` (D47) with the other open flags, and the read-seam error-with-bytes rule.
- Errors: the `runTopLevel` chain and `main`'s last resort are unchanged. `_exit` in the T-CLI-10 child is assessed above and nothing under `src/` uses it. Destructors cannot throw.
- Diagnostics, global state, and concurrency: unchanged. The test's `fork` relies on a single-threaded process.
- Verification: T-CLI-10's mechanism, skip path, `ctest` wiring, and protocol (two Lows above). T-BUILD-6's expanded scan. T-OWN-5. The sanitizer and `clang-tidy` gates are unchanged. A lexer and parser fuzzing plan is still recommended as a follow-up.
- Not applicable: script comparators and hashes, imports, signal handlers, threads in shipped code.

### Round 3 (2026-11-14)

Confirmed plan ID: `0001`. Plan path: `plans/0001.md` (revision 7, decisions D1–D48). Review path: `plans/0001_review.md`. Grill record: `plans/0001_grill.md`. Gate checked: Architecture Review Round 4 is **Go**, Plan Review Round 5 is **Go**, and the overall verdict was `Pending security review`.

**Security verdict:** Go (0 Critical, 0 High, 0 Medium, 2 Low)

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| R2 Low: T-CLI-10's child-to-parent protocol can report a false pass | Resolved | T-CLI-10 steps 6–7: the child body is one `try` with a final `catch (...)` (code 34), there are no doctest macros in the child, and every path ends in `_exit`. The pass code is 10, skip codes are 20–25, and failure codes are 30–34. The parent treats 0, 1, unknown codes, and `WIFSIGNALED` as failures. The skip line is written only for a skip code. The parent closes its write end, drains the pipe, then calls `waitpid`. There is `TIMEOUT 10`, `mkdtemp`, and the `_exit` exception is recorded in §7 and §4. `-fno-sanitize-recover=all` (§7) means a UBSan report in the child aborts, so it becomes a failure, not a pass. |
| R2 Low: T-CLI-10 can be silently disabled, and on the target platform it may only ever skip | Resolved | `PHILIOS_TCLI10_TEST_CASE` feeds both `--test-case=` and `--test-case-exclude=`. The zero-test `FAIL_REGULAR_EXPRESSION` is self-checked by T-BUILD-7. Task 9 requires a per-build pass or skip report with the reason (D48). The CTest precedence check below confirms that no wiring defect can turn into a silent pass. |
| R2 Low: The intrusive worklist must unlink a node before destroying it | Resolved | §3.9 "Unlink invariant" gives the exact move order (`node = std::move(head); head = std::move(node->link_);`) before `detachChildren` and destruction, and requires the child's link to be empty when pushed. T-OWN-4 builds a worklist of at least 200,000 nodes and a chain at least 200,000 deep, destroyed under ASan on the 8 MiB stack. |
| R2 Open question: Is a skipped T-CLI-10 on Ubuntu 24.04 acceptable? | Resolved | D48: a skip is acceptable, `O_NOCTTY` is then checked by code review against §3.2, the acceptance report states each build's outcome with the reason, and no privileged run is required. This is reflected in the §9 introduction, T-CLI-10, §8 task 9, §11, and the header. |

#### CTest precedence of `SKIP_REGULAR_EXPRESSION` and `FAIL_REGULAR_EXPRESSION` (requested by the architecture reviewer)
- **Precedence in CTest 3.28.** CTest first applies `FAIL_REGULAR_EXPRESSION`, which forces a failure. Then, if the process exited normally (no timeout, no signal), it checks `SKIP_RETURN_CODE` and `SKIP_REGULAR_EXPRESSION`. A skip match sets the result to "not run" whatever the exit code or the forced failure, and the `WILL_FAIL` logic is evaluated only when no skip matched. The CMake documentation for these properties says the same: the skip properties take precedence over the pass and fail expressions. So **a skip match masks a fail match, but it never produces a pass.** A timeout or a signal bypasses the skip check and is reported as a failure.
- **Can this give a silent pass for D47?** No. I checked each case:
  - *Name mismatch* (zero tests run): the T-CLI-10 case never runs, so the skip line is never printed. The fail expression matches, and the result is **Failed**.
  - *Correct pattern, test runs and passes*: doctest reports one test case, so the zero-test pattern does not match, and the result is **Passed**. That is a genuine pass.
  - *Correct pattern, test skips*: the result is **Not Run**, with the reason shown (D48 report).
  - *Over-broad pattern* (the plan-reviewer's Round 5 Low, where `\|` becomes alternation with an empty branch): a run that executes and passes **Fails** loudly. A skip run is reported as **Not Run**. The T-BUILD-7 guard still passes. This misleads but never passes silently. On a host where T-CLI-10 always skips (D48), the defect stays hidden until a host where the test runs, which is the plan-reviewer's Low. I agree with its recommended negative check and the `\\|` or `[|]` spelling, and I do not raise it again.
  - *Pattern that never matches*: the T-BUILD-7 guard run exits 0, no fail match occurs, and with `WILL_FAIL` the guard **Fails**. That is the case T-BUILD-7 exists to catch.
- The remaining consequence of skip precedence is narrow, and is the first Low below.
- **Inherited pseudo-terminal master descriptor in the child** (raised by the plan-reviewer): this is harmless. A terminal becomes the controlling terminal only when a session leader opens the *slave* without `O_NOCTTY`. Holding the master has no such effect. The child never `exec`s, so `O_CLOEXEC` does not matter. The master is not a write end of the stderr pipe, so it cannot delay the parent's end-of-file. `_exit` releases it. No finding.

#### Findings

##### Low: On a skip outcome, CTest ignores the exit status, so parent-side failures after the skip line are hidden
- **Plan reference:** T-CLI-10 step 7 ("the parent writes `T-CLI-10 SKIPPED: <reason>` to stdout … and records no failure"; "The parent then closes the master and removes the temporary directory"); §7 `SKIP_REGULAR_EXPRESSION`.
- **Rule:** AGENTS.md Verification (tests must pass under ASan and UBSan; regression tests must detect failures).
- **Security impact:** Skip precedence applies to the whole process output. The plan writes the skip line *before* the parent's cleanup (closing the master, removing the `mkdtemp` directory). Any later doctest assertion failure, ASan or UBSan abort, or LSan exit-time leak report in the T-CLI-10 parent is reported as "Not Run" instead of "Failed". The main `philios_tests` entry excludes this case, so the parent code runs only in the dedicated entry. On the target platform, where it always skips (D48), that code therefore never gets sanitizer coverage. This affects only test code and cannot cause undefined behavior in the shipped binary, so it does not block.
- **Recommendation:** In T-CLI-10 step 7, require the parent to finish all cleanup and checks (RAII scope closed, temporary directory removed, master closed) *before* writing the skip line, as its last action in the test case. In §7, note that a skip match overrides the exit status, so LSan's exit-time check cannot be seen on skip runs of this entry. Accept that as a test-only limitation.

##### Low: The diagnostic-escaping rule does not cover bidi characters, C1 controls, or malformed UTF-8 that a lexical message might echo
- **Plan reference:** §4 ("Control characters (including CR) and terminal escape sequences in any echoed text are escaped"); §3.3 "A `\` at the end of a line or of the file" ("The unknown-escape error at the backslash applies only when a character other than `\`, `"`, `n`, or `t` follows the `\`"); T-SEC-7 (control characters, CR, ESC only); plan-reviewer Round 5 Medium.
- **Rule:** CERT FIO30-C (exclude unsanitized input from output); style guide, Errors & Diagnostics; §3.3/D28e (bidi characters are rejected because they make displayed text misleading).
- **Security impact:** The message prose is unspecified. An implementation that reports `\` followed by U+202E, a C1 control (U+0080–U+009F, which includes the 8-bit CSI U+009B), or a malformed byte as an unknown escape, and quotes the offending character (for example ``unknown escape `\X` ``), writes that character raw to the developer's terminal. "Control characters" most naturally means C0 and DEL, and T-SEC-7 tests only those and ESC. Bidi characters are format characters, not controls. A raw bidi override or a C1 CSI in a diagnostic can reorder or hide the reported location or message. This is the diagnostic counterpart of what D28e prevents in source text. The plan-reviewer's Round 5 Medium (which rule applies when a forbidden character follows a `\`) makes the echo path more likely. Display spoofing only; no undefined behavior.
- **Recommendation:** In §4, state that echoed text escapes every byte outside printable ASCII (0x20–0x7E), or at least C0 controls, DEL, C1 controls, the D28e bidi characters, and any byte that is not part of a valid UTF-8 sequence. Alternatively, state that lexical diagnostics never echo the offending character. Extend T-SEC-7 with a U+202E, a U+009B, and an invalid byte (for example 0xFF). Whichever precedence the composer picks for the plan-reviewer's Medium, the diagnostic stays safe.

#### Open security questions for the developer
- None.

#### Key assumptions
- The interim trust model applies, and the plan claims no support for running untrusted code (§5, AGENTS.md Open Decisions).
- CTest 3.28 evaluates skip properties after `FAIL_REGULAR_EXPRESSION` and lets a skip match override it, as described above and in the CMake property documentation. If a different CTest version reversed this, the effect would only turn some skips into failures, never into passes.
- doctest exits 0 when a `--test-case=` filter matches nothing (the premise of the plan's zero-test check, which T-BUILD-7 verifies).
- The test process is single-threaded at `fork`, as Round 2 assumed and T-CLI-10 now states.

#### Areas reviewed
- Revision-7 changes: the §3.3 rule for a `\` at the end of a line or of the file (all paths stay in the bounds-checked `peek`, and the first error stops processing, so neither location choice in the plan-reviewer's Medium can over-read or loop; the diagnostic aspect is the second Low), the T-CLI-10 protocol and parent checks, the `_exit` exception, the §7 registration with T-BUILD-7, the §3.9 unlink invariant, and T-OWN-4.
- D48: a verification-policy decision only. It changes no shipped behavior, and `O_NOCTTY` remains in §3.2 and §5.
- Reentrancy, numeric safety, file loading, error handling (`runTopLevel` chain, `main`'s last resort), global state, and concurrency: unchanged since Round 2. No regressions found.
- Ownership and lifetime: intrusive worklist ordering, loop-unlinked environment chain, `Module` table swap, `noexcept` destruction with no allocation (T-OWN-5).
- Diagnostics: escaping scope (second Low).
- Verification: CTest skip and fail precedence, the zero-test guard, the inherited pseudo-terminal master, sanitizer behavior in the child (`-fno-sanitize-recover=all`), `TIMEOUT 10`. A lexer and parser fuzzing plan is still recommended as a follow-up.
- Not applicable: script comparators and hashes, imports, signal handlers, threads in shipped code.