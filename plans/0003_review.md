# Review of Plan 0003

**Overall verdict:** Go

## Plan Review

### Round 1 (2026-11-21)

**Plan verdict:** Go

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| None | N/A | First review round for this plan. |

#### Findings
No Critical, High, Medium, or Low findings.

#### Open questions for the developer
- None. (Plan OQ-15, OQ-19, and OQ-20 are marked non-blocking with adopted defaults.)

#### Notes for the secondary reviewers
- Architecture: None.
- Security: None.

#### Areas reviewed
- Confirmed plan identity and paths: plan ID `0003`, plan path `plans/0003.md`, review path `plans/0003_review.md`.
- Structure and required sections.
- Developer intent against grill decisions D1–D21 and DA1–DA7, including DA3 correction.
- Specification precision and consistency.
- Open Decisions handling and AGENTS.md update tasks.
- Errors/CLI behavior definitions (trigger/class or message/location/exit status).
- Security and resource limits section presence and specificity.
- Scope/size and explicit non-goals.
- Implementation task ordering, file targeting, and concreteness.
- Acceptance criteria/test coverage, including failure/boundary/malformed/deep nesting cases.
- Review Disposition handling for available prior reviewer feedback context (none yet).

### Round 2 (2026-11-21)

**Plan verdict:** Go

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| None | N/A | Round 1 had no primary-review findings. |

#### Findings
No Critical, High, Medium, or Low findings.

#### Open questions for the developer
- None. (OQ-15, OQ-19, and OQ-20 remain explicitly non-blocking defaults in §10.2.)

#### Notes for the secondary reviewers
- Architecture: Please verify AD1/AD2 contract details in §4.14 and tests §9.12 are architecturally adequate.
- Security: Please review §6 and the AD1/AD2-driven runtime-check and ownership failure modes for adequacy.

#### Areas reviewed
- Confirmed plan identity/path consistency and plan existence (`0003`, `plans/0003.md`, `plans/0003_review.md`).
- Full required structure/sections, including Errors, Security and Resource Limits, Review Disposition, and Revision History.
- Developer intent against grill decisions D1–D21 and DA1–DA7.
- Precision of grammar/semantics/error triggers and boundary behavior.
- Consistency across plan sections, grill record, AGENTS.md, and referenced project docs.
- Open Decisions handling and AGENTS.md update task coverage.
- Error model completeness (class/message/location/exit behavior).
- Scope sizing and explicit non-goals.
- Task ordering, file targeting, and implementability.
- Acceptance criteria and test coverage, including malformed, failure, boundary, and deep nesting cases.
- Disposition coverage for latest reviewer findings (including architecture round 1 items).
- Open questions blocking status.

### Round 3 (2026-11-21)

**Plan verdict:** Go

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| None | N/A | Rounds 1 and 2 had no primary-review findings to track. |

#### Findings
No Critical, High, Medium, or Low findings.

#### Open questions for the developer
- None. (OQ-15, OQ-19, and OQ-20 remain explicitly non-blocking defaults.)

#### Notes for the secondary reviewers
- Architecture: Verify AD2's revision-5 owned-name contract (`std::string` ownership for named AST nodes) is implementable without reintroducing lifetime coupling.
- Security: Verify revision-5's retained-`Function` lifetime fix and sanitizer coverage (T3-ARCH-2, T3-ARCH-8, T3-ARCH-9 under T12) are adequate.

#### Areas reviewed
- Plan identity/path consistency and artifact existence.
- Full required section structure and revision-5 deltas.
- Developer intent against grill decisions D1–D21 and DA1–DA7.
- Precision/consistency of grammar, semantics, errors, and ownership contracts.
- Open Decisions handling and AGENTS.md update task coverage.
- Scope/size, task ordering, and file-level implementation concreteness.
- Acceptance criteria and tests (success, failure, boundary, malformed, deep nesting, sanitizer/lifetime).
- Review Disposition coverage for latest reviewer findings (including security round 1 High finding).
- Blocking status of open questions.

### Round 4 (2026-11-21)

**Plan verdict:** Go

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| None | N/A | Rounds 1–3 had no primary-review findings to track. |

#### Findings
No Critical, High, Medium, or Low findings.

#### Open questions for the developer
- None. (OQ-15, OQ-19, and OQ-20 remain explicitly non-blocking defaults in §10.2.)

#### Notes for the secondary reviewers
- Architecture: None.
- Security: None.

#### Areas reviewed
- Confirmed plan identity/path consistency and artifact existence: `0003`, `plans/0003.md`, `plans/0003_grill.md`, `plans/0003_review.md`.
- Verified gate state from latest secondary rounds before final broader review (Architecture Round 3 Go; Security Round 2 Go).
- Whole-plan consistency against grill decisions D1–D21 and DA1–DA7, including DA3 correction and accepted AD1/AD2 (§4.14) plus revision-5 owned-name storage.
- Checked for stale revision text across revisions 2–5 (shadowing rollback consistency, validator-artifact contract, and `std::string_view`-name hazard replacement with owned-name contract).
- Structure completeness, scope size, implementation-task ordering and file targeting, acceptance criteria/test coverage breadth, error/CLI definitions, security/resource section presence/specificity, and Open Decisions handling.
- Verified Review Disposition coverage for prior reviewer findings and no remaining blocking open questions.
- Cross-checked plan assumptions against current `src/` interfaces/contracts (e.g., current `Program` shape, validator signature/behavior, and current message baseline) to ensure the migration tasks remain coherent and testable.

## Architecture Review

### Round 1 (2026-11-21)

**Architecture verdict:** No-go

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| None | N/A | First architecture review round. |

#### Findings
##### High: Validator-to-evaluator contract for runtime checks is undefined
- **Plan reference:** §4.10 (“It checks at runtime what it can't decide before execution”), and T7 (“Mark each runtime-checked site so that the evaluator performs the check.”)
- **Problem:** The plan requires validator-produced runtime-check markings, but it does not define where those markings live or how they flow to evaluation. Current architecture has `validateProgram(const Program&)` with no analysis artifact return and no specified AST annotation contract.
- **Impact:** Different implementers will choose incompatible cross-component designs (AST mutation vs side tables vs evaluator recomputation), which can cause missing checks, duplicated logic, or rework.
- **Recommendation:** Add a concrete interface decision: either (a) explicit AST fields for check mode/expected type on relevant nodes, or (b) a dedicated typed-analysis artifact returned by validation and consumed by evaluation, with ownership/lifetime rules and exact files to change.

##### High: Module-item/FunctionDecl ownership model is under-specified after Program reshaping
- **Plan reference:** T2 (“Change `Program` to hold module items … in source order.”), §4.13 (module and function environments), T9 (module-level execution before `main`)
- **Problem:** The plan changes `Program` shape but does not define the canonical ownership path for `FunctionDecl` objects after mixing module items. Existing architecture relies on shared declaration ownership for function runtime objects and deterministic module teardown.
- **Impact:** Implementers must choose a new ownership design themselves, risking dangling declaration references, accidental strong cycles, or teardown/lifetime regressions.
- **Recommendation:** Specify the exact module-item representation and declaration ownership contract (including how runtime `Function` objects reference declarations and how this preserves current teardown/cycle-breaking behavior).

#### Open questions for the developer
- Should runtime-check metadata be carried on AST nodes, or in a separate validator output artifact passed to evaluation?
- After `Program` becomes mixed module items, what is the required ownership/reference model for `FunctionDecl` used by runtime `Function` objects?

#### Notes for the security reviewer
- None until architecture findings are resolved.

#### Areas reviewed
- Confirmed plan identity/path: plan ID `0003`, `plans/0003.md`.
- Core architectural constraints from `AGENTS.md` and style-guide architectural rules.
- Open-decision interaction (evaluation order and other still-open project decisions).
- Component boundaries/interfaces (parser/AST/validator/evaluator/environment/module).
- Ownership/lifetime and reentrancy implications.
- Reuse/DRY with current `src/` architecture.
- Naming/layout, error-model placement, performance architecture, evolution, binary footprint, and architectural completeness.

### Round 2 (2026-11-21)

**Architecture verdict:** Go

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| High: Validator-to-evaluator contract for runtime checks is undefined | Resolved | §4.14.1 and §4.14.3 now define AD1 concretely: no validator artifact, `validateProgram(const Program&)` remains `void`, evaluator performs all runtime checks unconditionally with specified type sources/check order; tasks T7–T9 and tests §9.12 cover this contract. |
| High: Module-item/FunctionDecl ownership model under-specified after Program reshaping | Resolved | §4.14.2 now defines AD2 concretely: `ModuleItem`, `Program`, `asFunctionDecl`, ownership/lifetime/teardown, load sequence, and unchanged cycle-breaking behavior; tasks T2/T3/T6/T9 and tests §9.12 anchor implementation. |

#### Findings
No new Critical, High, Medium, or Low architecture findings.

#### Open questions for the developer
- None.

#### Notes for the security reviewer
- Verify AD1/AD2 failure modes in implementation: runtime-check enforcement without validator artifacts, and module-load initializer/runtime-error teardown behavior.

#### Areas reviewed
- Confirmed plan identity/path: plan ID `0003`, `plans/0003.md`.
- Core architectural constraints in `AGENTS.md`.
- Open-decision interaction and whether this plan silently commits unresolved project-wide decisions.
- Component boundaries/contracts across parser, AST, validator, evaluator, environment, module loader, and CLI boundary.
- Ownership/lifetime and teardown design (including `FunctionDecl` sharing, module items, and cycle-breaking).
- Reuse/DRY against current `src/` architecture and existing contracts.
- Naming/layout and style-guide architectural rules (smart-pointer ownership, error model placement, no mutable static state).
- Error-model phase placement, performance architecture implications, evolution/headroom, binary footprint, testability, and architectural completeness.

### Round 3 (2026-11-21)

**Architecture verdict:** Go

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| High: Validator-to-evaluator contract for runtime checks is undefined | Resolved | Revision 5 keeps AD1 intact (`validateProgram` stays `void`, no validator artifact, evaluator-side unconditional runtime checks remain fully specified in §4.14.1/§4.14.3 and tasks T7–T9). |
| High: Module-item/FunctionDecl ownership model under-specified after Program reshaping | Resolved | Revision 5 keeps AD2 ownership/load/teardown contracts and extends them with owned-name storage in §4.14.2 without changing ownership edges or introducing new cycles. |

#### Findings
No new Critical, High, Medium, or Low architecture findings.

#### Open questions for the developer
- None.

#### Notes for the security reviewer
- Verify implementation of §4.14.2 owned-name storage across all listed named AST nodes (`Identifier`, `Param`, `FunctionDecl`, `LetDecl`, `ConstDecl`, `SymbolDecl`, `AssignStmt`) and retained-function post-`Module`-release behavior in T3-ARCH-2/8/9 under ASan.

#### Areas reviewed
- Confirmed plan identity/path: `0003`, `plans/0003.md`.
- AD2 revision-5 owned-name contract against current `src/` interfaces (`ast.hpp/.cpp`, parser/module/environment/function/evaluator paths) for ownership, lifetimes, and interface churn.
- Consistency with AGENTS.md architectural constraints (no global mutable state, isolated module/env model, reference-counted ownership, object model hierarchy, evaluator/environment contracts).
- Component boundaries and dependency direction across parser/AST/validator/evaluator/module/environment.
- Performance and footprint implications of `std::string` name ownership vs `std::string_view` borrowing.
- Testability/completeness of the design updates (tasks/tests T2, T9, T12, T3-ARCH-2/8/9).

## Security Review

### Round 1 (2026-11-21)

**Security verdict:** No-go

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| None | N/A | First security review round. |

#### Findings
##### High: A retained FunctionDecl can outlive its borrowed source text
- **Plan reference:** §4.14.2 says a retained `Function` continues to co-own its `FunctionDecl` after `module.reset()`, while teardown subsequently calls `source_.clear()`. T3-ARCH-2 expressly retains the function and declaration but avoids reading source views because “the source buffer is gone.”
- **Rule:** CERT MEM50; style guide, Ownership & Lifetime and Types & Data.
- **Security impact:** `FunctionDecl`, its parameters, and nested identifier nodes contain `std::string_view`s into the module source. A retained `Function` can therefore keep the AST alive after the source buffer is destroyed. Accessing declaration names or invoking such a function can read dangling views, causing use-after-free and undefined behavior through the public runtime interfaces.
- **Recommendation:** Revise AD2 so every AST or `FunctionDecl` that may outlive `Module` shares ownership of an immutable source buffer, or stores owned text instead of borrowed views. The source owner must remain alive until the final AST owner is destroyed and must not introduce a back-edge or weaken an owning edge. Replace T3-ARCH-2’s “do not read” caveat with a sanitizer-covered test that safely accesses or invokes the retained function after module release.

#### Open security questions for the developer
- None.

#### Key assumptions
- Publicly retained `Function` values remain usable unless the design explicitly and safely invalidates them without consulting source-backed views.

#### Areas reviewed
- Runtime-check enforcement without validator artifacts.
- Module initialization failure cleanup and environment/function cycle teardown.
- Assignment reentrancy and nullable-constant store-time rechecking.
- Parser, evaluator, call-depth, and expression-nesting limits.
- Iterative AST and environment destruction.
- Numeric equality and conversion safety.
- Lexer and malformed-input handling.
- Exception boundaries, diagnostics, globals, and verification coverage.
- Module, AST, source-buffer, environment, function, Boolean, and Symbol lifetimes.

### Round 2 (2026-11-21)

**Security verdict:** Go

#### Earlier findings
| Finding | Status | Reason |
|---------|--------|--------|
| High: A retained FunctionDecl can outlive its borrowed source text | Resolved | Revision 5 requires every named AST node to own its name as `std::string`; no AST node retains a source-buffer view. T3-ARCH-2, T3-ARCH-8, and T3-ARCH-9 exercise inspection and invocation after module destruction under ASan/UBSan. |

#### Findings
No Critical, High, Medium, or Low findings.

#### Open security questions for the developer
- None.

#### Key assumptions
- Revision 5’s owned-name requirement is implemented for every node listed in §4.14.2 and T2, including nested identifiers and parameters.
- Existing source-size, call-depth, evaluation-depth, exception-boundary, and iterative-destruction protections remain in force.

#### Areas reviewed
- Revision 5’s retained-`Function` and source-buffer lifetime fix against the current AST, parser, module, function, and evaluator interfaces.
- Parse-time `std::bad_alloc` after removing `noexcept`, including CLI handling and partial-construction cleanup.
- Memory cost of owned long names under the 2 MiB source limit and clean failure through `std::bad_alloc`.
- Iterative destruction of new AST nodes and retained declarations.
- Assignment and operator reentrancy, owning references, explicit evaluation sequencing, and the constant store-time recheck.
- AD1’s unconditional evaluator-side runtime checks.
- Module-initialization failure teardown and environment/function cycle release.
- Numeric equality, malformed and deeply nested input, diagnostics, globals, and sanitizer/static-analysis coverage.
