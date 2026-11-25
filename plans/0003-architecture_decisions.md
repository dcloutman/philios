# Plan 0003 — Architecture Decisions

- **Plan ID:** `0003`
- **Plan:** `plans/0003.md` (revision 5; status "Ready for review"; implementation explicitly
  authorized by the developer on 2026-11-21)
- **Review:** `plans/0003_review.md` (overall verdict Go: Plan round 4 Go, Architecture round 3
  Go, Security round 2 Go)
- **Grill record:** `plans/0003_grill.md` (D1–D21)
- **Developer answers recorded in the plan:** DA1–DA7 (plan §10.1)
- **Composer architecture decisions recorded in the plan:** AD1 and AD2 (plan §10.3, §4.14)
- **Developer confirmation at implementation start (2026-11-21):** OQ-15 is confirmed: `Symbol`
  is a reserved word (the plan default).
- **Task breakdown:** `plans/0003-implementation-tasks.md`
- **Stage:** implementation-architect (this record and the task breakdown only; no stubs, tests,
  or source changes)
- **Date:** 2026-11-21

This record lists only decisions that the approved plan, its grill record, its recorded
developer answers, `AGENTS.md`, or `.github/instructions/cpp-style-guide.instructions.md`
already make. It adds no decision. A bare `Dn` is a plan 0003 grill decision. Plan 0001 and 0002
decisions are written "plan 0001 Dn" and "plan 0002 Dn". §6 lists interface details the plan
leaves to later stages (not decisions). §7 lists items that need the developer's attention.

## 1. Sources of truth (D1, D2; plan §4.1)
- Plan 0003 is the source of truth for the behavior it defines. Earlier plans do not limit it
  (D1).
- The code in `src/` and the documentation linked from `docs/index.md` describe the current
  state. `docs/language_rules.md` is out of date and deprecated, and is **not** edited (D2,
  plan 0002 D27; plan T11).
- Everything not listed in plan §3 (C1–C15) keeps its current behavior.

## 2. Approved decisions this plan relies on

### 2.1 Source layout and build (existing layout; plan §8)
- No `include/` directory. Headers sit beside their sources under `src/`; the single include
  root is `src/`. Unit tests mirror `src/` under `tests/unit/`. Shared test helpers live in
  `tests/unit/support/`. CLI fixtures live in `tests/cli/cases/` and are registered in
  `tests/cli/cli_cases.cmake`.
- `CMakeLists.txt` collects `src/*.cpp` and `tests/unit/*.cpp` with `GLOB_RECURSE ...
  CONFIGURE_DEPENDS`. The project targets compile with `-Wall -Wextra -Wpedantic -Wconversion
  -Wshadow`, and `clang-tidy` runs with `cert-*` and `bugprone-*` and `WarningsAsErrors: '*'`.
- New files are exactly those the plan names:
  - `src/runtime/object_model/boolean.{hpp,cpp}` and `src/runtime/object_model/symbol.{hpp,cpp}`
    (T4)
  - `tests/cli/cases/t3_*.phil` (T10)
  - `docs/end-users/src/reference/variables-and-constants.md` and
    `docs/end-users/src/reference/operators.md` (T11)
- No dependency is added (AGENTS.md Core Architectural Constraint 4).

### 2.2 Lexer (plan §4.2; D9, D20; plan 0001 D27)
- **Identifier rule (C1).** The lexer still reads a maximal word `[A-Za-z_][A-Za-z0-9_]*`. A
  word that is not reserved and does not match `_*[A-Za-z][A-Za-z0-9_]*` is a lexical error at
  its first byte with the message `invalid identifier`.
- **Reserved words (C2).** 35 in total: the 28 current words plus `let`, `const`, `true`,
  `false`, `symbol`, `Boolean`, and `Symbol`, each with its own token kind. The lexer never emits
  a reserved word as an identifier. `true` and `false` are reserved literal keywords, not
  bindings (D20). `Symbol` is reserved (OQ-15, confirmed by the developer 2026-11-21).
- **New tokens (C3).** `Bang` (`!`), `Assign` (`=`), `EqualEqual` (`==`), `BangEqual` (`!=`),
  with maximal munch.
- **Negative literals (C4).** The expression-start context list grows from `return`, `(`, `,`
  to also include `=`, `==`, `!=`, and `!`. All other plan 0002 negative-literal rules are
  unchanged.

### 2.3 Parser structure (plan §4.4, §4.12, §6.1; D6, D7, D16, DA5, DA7)
- Grammar exactly as plan §4.4. Assignment is a statement only (D7). `let` at module level is a
  syntax error at `let` (D13).
- **Extensible precedence.** Either one parse function per precedence level or one table-driven
  precedence-climbing loop; each level delegates to the next tighter level, so later plans can
  insert levels without restructuring (D6). The choice between these two structures is left to
  the implementation (§6).
- **AST shape for operators.** One unary-operator node (`UnaryExpr`, operator enumerator `Not`)
  and one binary-operator node (`BinaryExpr`, operator enumerators `Equal`, `NotEqual`). No
  node class per operator.
- **Equality chains** are left-associative (D16) and parsed **iteratively** (a loop and a
  counter, not one C++ frame per operator).
- **`!` operand** is a `primary`; `!` followed by `!` is the syntax error
  `repeated ! is not allowed` at the second `!` (DA5).
- **Nesting.** Expression operators count toward the existing parser nesting limit of 256
  (plan 0001 D7, `ExecutionContext::kMaxParseNesting`; DA7). `!` adds one level; each `==`/`!=`
  in a chain adds one level. A function body is level 1; a module-level initializer starts at
  level 0. Reaching level 257 is a syntax error at the operator token that would open it.
- No grouping parentheses (DA7).

### 2.4 AD1 — Validator-to-evaluator contract for runtime checks (plan §4.14.1, §10.3)
- **No validator artifact.** `void validateProgram(const Program& program)` in
  `src/analysis/semantic_analyzer/validator.hpp` keeps its signature and `void` return. It
  returns if the program is valid and throws `DeclarationError` (or `InternalError`) otherwise.
  Static types are computed inside `validator.cpp` and discarded. AST nodes stay immutable after
  parsing: no `mutable` fields, no annotation pass, and no side table keyed by node address.
- **The evaluator always performs the §4.10 runtime checks, unconditionally,** through the single
  acceptance function `matchesDeclaredType` (or an `ObjectType` test for `!`). It never asks
  whether the validator proved a site safe. This is the model `src/runtime/evaluator/call.cpp`
  already uses for argument and return checks.
- **Sites, expected-type sources, checks, and failures (plan §4.14.1 table):**

  | Site | Expected type comes from | Runtime checks, in order | Failure |
  |---|---|---|---|
  | `LetDecl` initializer | `LetDecl::type()` | `matchesDeclaredType(value, type)` | `AssignmentType` at the initializer's first byte |
  | `ConstDecl`, nullable type, no initializer or a `NullLiteral` initializer | none | none; the initializer is not evaluated; bind `context.nullObject()` | none |
  | Any other `ConstDecl` initializer | `ConstDecl::type()` | 1. nullable type and `null` value → `ConstantNullWrite`; 2. `matchesDeclaredType` | `ConstantNullWrite` or `AssignmentType` at the initializer's first byte |
  | `AssignStmt` right-hand side | the target binding's declared type via `Environment::bindingInfo` | §4.8 steps as in §2.6 below | §4.8 and §5.4 |
  | `UnaryExpr` with `Not` | fixed: `Boolean` | `value.objectType() == ObjectType::Boolean` | `OperandType` at the operand's first byte |
  | Call arguments, return values, `echo` arguments, callees | unchanged (DA7) | unchanged | unchanged |

- **Validator guarantees the evaluator may rely on** (a violation is an `InternalError`, never a
  script error): every evaluated `Identifier` resolves through `Environment::lookup`; every
  `AssignStmt` target resolves to a `Parameter`, `Variable`, or `Constant` binding; `SymbolDecl`,
  `LetDecl`, and `ConstDecl` names are not already bound in their defining environment. Name,
  visibility, bare-symbol, `main`, and module-level-initializer rules are checked only by the
  validator. The constant write-once checks (§4.8 steps 1 and 4) stay real runtime errors
  (`ConstantReassignment`) as defense in depth.
- **Rationale (plan §10.3).** Static soundness makes the unconditional check observably identical
  to checking only undecidable sites; it reuses the current `call.cpp` model and the single
  `matchesDeclaredType`; it keeps AST nodes immutable and shared with `Function` objects
  (plan 0001 D3); a validator defect can't silently skip a check (security first).

### 2.5 AD2 — Module items and declaration ownership (plan §4.14.2, §10.3)
- **Representation** (`src/frontend/ast/ast.hpp`, replacing the current
  `using Program = std::vector<std::shared_ptr<const FunctionDecl>>;`):

  ```cpp
  /// @brief One `moduleItem` (§4.4): a FunctionDecl, ConstDecl, or SymbolDecl.
  using ModuleItem = std::shared_ptr<const ASTNode>;

  /// @brief The parsed `program` production: every module item in source order.
  using Program = std::vector<ModuleItem>;

  /// @brief The FunctionDecl that @p item points to, sharing ownership with @p item, or null if
  ///        @p item is not a FunctionDecl. Uses `kind()` and `std::static_pointer_cast`.
  /// @throws InternalError if @p item is null.
  [[nodiscard]] std::shared_ptr<const FunctionDecl> asFunctionDecl(const ModuleItem& item);
  ```

  Invariant: every item is non-null and its `kind()` is `NodeKind::FunctionDecl`,
  `NodeKind::ConstDecl`, or `NodeKind::SymbolDecl`. The parser guarantees it; the validator and
  `Module::load` check it and throw `InternalError` on a violation. Dispatch over items uses
  `kind()` and a `switch` that handles every `NodeKind` explicitly.
- **Name storage (security review round 1).** Every AST node with a name stores it as an owned
  `std::string`: existing `Identifier`, `Param`, `FunctionDecl`, and new `LetDecl`, `ConstDecl`,
  `SymbolDecl`, and `AssignStmt` (target name). Constructors keep their `std::string_view name`
  parameter and copy it. `[[nodiscard]] std::string_view name() const noexcept` keeps its
  signature and returns a view of the node's own member. The `Identifier` and `Param`
  constructors lose `noexcept` and document `std::bad_alloc`. Destructors stay `noexcept`. After
  this change no AST node refers to the source buffer; only tokens do, and tokens live only
  inside `Module::load` (and the test pipeline).
- **Destruction.** Every new node class with children (`LetDecl`, `ConstDecl`, `AssignStmt`,
  `UnaryExpr`, `BinaryExpr`) implements `detachChildren` and a `noexcept` destructor that moves
  its children onto a local worklist and runs `destroyWorklist`, following
  `ExprStmt::~ExprStmt` in `src/frontend/ast/ast.cpp`. `SymbolDecl` and `BooleanLiteral` have no
  children but still implement `detachChildren`. A module item owned through `std::shared_ptr` is
  never pushed onto a worklist.
- **Load sequence** (`Module::load`, `src/runtime/evaluator/module.cpp`): lex, parse, validate,
  create the module environment (as today), then:
  1. Register functions: for each item with non-null `asFunctionDecl`, in source order, bind a new
     `Function(declaration, environment_)` with `BindingKind::Function`. All functions are bound
     before any initializer runs.
  2. Run module-level initializers (DA1 a): for each `ConstDecl` or `SymbolDecl` item in source
     order, call `item->evaluate(context, environment_)`. A non-`Normal` completion is an
     `InternalError`. No script function runs (DA1 b).
  3. Return the module.

  `Module::load` documents `RuntimeError` (raised by step 2; exit 70 through the existing CLI
  `PhiliosError` handler). A throw destroys the partly built `Module`, whose destructor calls
  `release()`, freeing the module environment and its `Function` cycle. `runMain` and
  `src/cli/cli.cpp` do not change.
- **Teardown** (`Module::release`): unchanged order and unchanged member declaration order
  (`source_`, `program_`, `environment_`): `environment_->releaseBindings()`, then
  `program_.clear()`, then `source_.clear()`. A retained `Function` keeps a self-contained
  `FunctionDecl` and stays safely inspectable and callable; a body that looks up a module-level
  name raises the existing `RuntimeError(UndefinedIdentifier)`. `Module` stays non-copyable and
  non-movable.
- **Rationale (plan §10.3).** Keeps the plan 0001 D3 ownership edge and teardown order, adds no
  cycle, keeps the existing `kind()`/`static_cast` dispatch convention, and leaves `Function`,
  `callFunction`, `runMain`, and the CLI interfaces unchanged. Owned names remove the
  borrowed-view hazard from the AST without a lifetime rule.

### 2.6 Binding records and the `Environment` interface (plan §4.13, §4.14.3; supports AD1)
In `src/runtime/environment/environment.hpp`:

```cpp
/// @brief What declared a binding (§4.13).
enum class BindingKind { Function, Parameter, Variable, Constant, Symbol };

/// @brief A binding's kind and declared type, copied out of the table.
struct BindingInfo {
    BindingKind kind;
    std::optional<DeclaredType> declaredType; ///< No value for BindingKind::Function.
};
```

- The table stores, per name, the value, the `BindingKind`, and the declared type by value. No
  AST pointer.
- `define(name, value)` keeps its signature and records `BindingKind::Function` with no declared
  type.
- New overload `define(name, value, BindingKind kind, DeclaredType declaredType)` binds
  parameters (`callFunction`, `Parameter`), `let` variables (`Variable`), constants
  (`Constant`), and symbols (`Symbol`, with `DeclaredType{TypeName::Symbol, false}`). Same
  preconditions as `define`.
- `[[nodiscard]] std::optional<BindingInfo> bindingInfo(std::string_view name) const` searches
  this environment then each enclosing scope, like `lookup`, and returns a copy.
- `void assign(std::string_view name, std::shared_ptr<Object> value, SourceLocation
  targetLocation)` performs §4.8 step 4: a `Constant` whose current value is not `null` →
  `RuntimeError(ConstantReassignment)` at `targetLocation`, nothing stored; a `Function` or
  `Symbol` binding, or an unbound name → `InternalError`; otherwise replace the value. Strong
  guarantee; never calls script code; no reference into the table escapes.
- `AssignStmt::evaluate` (`src/runtime/evaluator/evaluate.cpp`), with owning copies:
  1. `bindingInfo(target)`; absent → `InternalError`. For a `Constant`, `lookup(target)`; a
     non-`null` value → `ConstantReassignment` at the target, before the right-hand side runs.
  2. Evaluate the right-hand side.
  3. For a `Constant`, a `null` value → `ConstantNullWrite` at the right-hand side. Then a failing
     `matchesDeclaredType(value, *info.declaredType)` → `AssignmentType` at the right-hand side.
  4. `environment->assign(target, value, targetLocation)`.
- Lookups still return owning copies and still search the call environment before the module
  environment. Because of DA3, the two never hold the same name.

### 2.7 Object model and type checks (plan §4.3, §4.11.2, T4, T5; D9–D11, D14, D21)
- `Boolean` and `Symbol` are new object-model classes extending `Object`, with new `ObjectType`
  tags. `Boolean` is immutable and compares by value; `Symbol` compares by identity.
- **No process-global or `static` `Boolean` instance** (AGENTS.md Core Architectural
  Constraint 1). Canonical `true`/`false`, if used, are owned by `ExecutionContext`, like the
  `null` instance; otherwise each literal evaluation creates a new object (plan §4.3). Whether
  canonical instances are used is left to the implementation (§6).
- A `Symbol` holds no AST pointer and no view into the source buffer; it is pure identity.
- `matchesDeclaredType` and `scriptTypeName` are extended (`Object` accepts every non-`null`
  value including `Boolean` and `Symbol`; `typeof` returns `"Boolean"` and `"Symbol"`).
- One `valuesEqual(const Object&, const Object&) noexcept` implements D11: same runtime class
  required; `Integer` by value (exact `std::int64_t` comparison, no arithmetic); `String` by
  length then bytes; `Boolean` by value; `null == null`; `Function`, `Symbol`, and every other
  reference-like value by instance identity. Static types do not affect equality (D21). No
  recursion into contents.

### 2.8 Validator rules (plan §4.5, §4.5.1, §4.7–§4.10; D4, D13, D15, D17–D19, DA1–DA3, DA6, DA7)
- **Item iteration.** `Program` items are iterated with a `kind()` `switch`; a null item or
  another kind is an `InternalError`.
- **Scopes.** Module scope and one function scope per body. One namespace per scope (D19).
- **DA3 (no shadowing).** All module-level names (every function including the function's own
  name and `main`, every module-level constant and symbol) are collected in a first pass over all
  items (order-independent). For each function, **before** any statement or expression of its
  body is checked, every function-scope name (parameters in order, then body `let`/`const`/
  `symbol` names in source order) is checked in this order: the `main` rule (existing message),
  then the same-scope duplicate rule (`duplicate declaration`), then the module-level rule
  (`name must not match a module-level declaration`, replacing the plan 0001 D15 message
  `parameter name must not match a module function`). The validator keeps the plan 0001 D15
  check and generalizes it. The flat `parameters`/`functionNames` sets in `FunctionContext` are
  replaced by module-level and function-scope name tables.
- **Module-level initializers (DA1 b).** Only integer, string, `null`, `true`, `false` literals,
  `!`, `==`, `!=`, and earlier module-level constants and symbols. Calls, `echo`/`typeof`, and
  function names as values → `not allowed in a module-level initializer`. Later or own names →
  `used before declaration`. A module-level `Function` constant therefore can't hold a function
  (OQ-20 default adopted in the approved plan).
- **Static types (D4, §4.10).** Rules 1–5 exactly as plan §4.10, including DA6 for non-nullable
  `let`/`const` initializers and assignment targets including parameters (OQ-19 default adopted
  in the approved plan), D15 for nullable-constant writes, and the `!` operand rule. Static
  checks cover every function, including uncalled ones, and every module-level initializer.
  First error in source order; class `declaration`, exit 65 (DA7).
- **Nullable constants (§4.7).** The validator tracks each function-scope nullable constant as
  *unassigned* or *assigned* through the body (no control flow exists yet). Module-level
  constants can't be written from functions at all (DA1 d).

### 2.9 Errors (plan §5)
- Diagnostics keep the format `main.phil:<line>:<column>: <class> error: <message>`.
- Four new `RuntimeErrorKind` values: `AssignmentType`, `OperandType`, `ConstantReassignment`,
  `ConstantNullWrite` (exit 70). Existing kinds unchanged.
- New declaration messages are exactly those listed in plan §5.3; new syntax message
  `repeated ! is not allowed`; new lexical message `invalid identifier`.
- A runtime error during module initialization uses the same kinds and exit status; `main` is not
  called and stdout is empty.

### 2.10 Evaluation order (D5; plan §4.8, §4.11.4)
- Operands strictly left to right; the left operand completes before the right; a failing left
  operand prevents evaluating the right.
- Assignment: check target assignability, evaluate the right-hand side, check the value, store
  (with the store-time constant re-check).
- The C++ evaluator sequences every step explicitly into named locals (style guide, CERT EXP50).
- This settles the AGENTS.md Open Decision "Evaluation order of operands and assignment targets"
  (recorded in AGENTS.md by T11).

## 3. Ownership and lifetime model (plan §4.14.2, §6.3)
| Owner | Owned | Edge |
|---|---|---|
| `Module` | source buffer (`source_`) | value; viewed only by tokens during `load` |
| `Module` | `Program` (`program_`) | one strong `std::shared_ptr` per module item |
| `Program` item / runtime `Function` | `FunctionDecl` | shared control block (`asFunctionDecl` via `std::static_pointer_cast`; plan 0001 D3) |
| `Program` only | module-level `ConstDecl`, `SymbolDecl` | strong; no runtime object refers to them |
| `Block` | function-scope `LetDecl`, `ConstDecl`, `SymbolDecl`, `AssignStmt` | `std::unique_ptr<ASTNode>` |
| AST node | its name text | owned `std::string` |
| `Environment` binding | value, `BindingKind`, `DeclaredType` | value semantics; no AST pointer |
| module environment ↔ `Function` | closure environment | existing cycle, broken by `releaseBindings()` (plan 0001 D3) |

- No new reference cycle. Module-level constants and symbols hold only `Integer`, `String`,
  `Boolean`, `Symbol`, or `null` values.
- No owning edge is weakened. Nothing points from the AST back to the `Module`.
- Assignment holds owning copies of the evaluated value and the target environment from step 1 to
  step 4. No reference, pointer, or iterator into a binding table is held across evaluation.
- Equality of `Function`/`Symbol` compares identity while both operands are held by owning copies.
- A per-call symbol (DA4) returned through a `Symbol` value outlives its call environment through
  that owning reference.

## 4. Trade-offs the plan records
- **AD1 rejected alternatives:** (a) check-mode/expected-type fields on AST nodes (would need
  `mutable` fields or a mutating pass on nodes shared with `Function` objects); (b) a
  validator-returned side table (new artifact with a lifetime tied to the `Module`, a lookup per
  evaluation, and a silently skipped check on a missing entry). Accepted cost: one `ObjectType`
  comparison per checked site.
- **AD2 name storage rejected alternative:** a shared immutable
  `std::shared_ptr<const std::string>` source buffer co-owned by each escaping node (keeps a
  lifetime rule every future escaping node must follow and changes the `Parser`, `FunctionDecl`,
  and test-support interfaces). Accepted cost: parse-time copies of names and 8–16 extra bytes
  per named node; small-string names do not allocate; evaluation and lookup costs unchanged.
- **Iterative equality parsing** trades a slightly more complex loop for immunity to stack
  overflow on very long chains (§6.1).

## 5. Earlier decisions replaced or extended by plan 0003
| Earlier decision / code fact | Plan 0003 effect | Plan ref |
|---|---|---|
| Name pattern `[A-Za-z_][A-Za-z0-9_]*` | Stricter `_*[A-Za-z][A-Za-z0-9_]*` | C1, §4.2.1 |
| 28 reserved words (plan 0002) | 35 reserved words | C2, §4.2.2 |
| Negative-literal context closed list `return`, `(`, `,` (plan 0002) | Adds `=`, `==`, `!=`, `!` | C4, §4.2.4 |
| Plan 0001 D15: parameter must not match a module function | Kept and generalized (DA3); new message `name must not match a module-level declaration` | C12, §4.5 |
| Parameters read-only | Assignable like `let` of the declared type (DA2) | C13, §4.8 |
| Program start: validate, call `main` | Validate, run module-level initializers once in source order, call `main` (DA1) | C14, §4.5.1 |
| `Program = std::vector<std::shared_ptr<const FunctionDecl>>`; names as `std::string_view` into the source buffer; "no node may outlive the Module" | `ModuleItem`/`Program`/`asFunctionDecl`; owned `std::string` names; wording replaced | C15, §4.14.2 |
| AGENTS.md Open Decision: operand and assignment-target evaluation order | Settled by D5 | §7 |
| AGENTS.md Document Precedence | Revised per D1, D2 | §7 |

## 6. Details left to later stages (not architecture decisions)
These are implementation details the plan does not fix. Later stages choose them following
existing conventions in `src/` and the style guide, without changing any interface the plan
names. If a choice would change a named interface or script-visible behavior, stop and ask.
- Per-level parse functions vs. a table-driven precedence-climbing loop (§2.3).
- Names of the operator enumeration types for `UnaryExpr`/`BinaryExpr` (enumerators are fixed:
  `Not`, `Equal`, `NotEqual`) and accessor names on new nodes beyond `name()`, `type()`, and the
  optional initializer, following existing `ast.hpp` conventions.
- Whether canonical `true`/`false` instances exist (if they do, `ExecutionContext` owns them).
- The public interface and private storage of `Boolean` and `Symbol`, following the existing
  `Integer`/`NullObject` pattern.
- The internal data structures of the validator's name tables and static-type representation.

## 7. Items for the developer
These are recorded in full, with affected tasks, in `plans/0003-implementation-tasks.md`
("Developer decisions required"). Summary:
1. **DR-1 (blocking for tasks 2, 4, and 8):** the plan's per-task gate ("each task keeps the
   build warning-free and the whole suite passing", plan §8) conflicts with the task file lists.
   Changes required in T2, T4, and T8 break compilation, linking, or `-Wswitch`/`clang-tidy`
   cleanliness in files that the plan assigns to later tasks (T5, T6/T7, T9). No option is
   chosen here.
2. **DR-2 (non-blocking; task 4):** T4 lists `CMakeLists.txt` "Add the new source files to the
   build", but the existing `GLOB_RECURSE` already builds new `src/` files.
3. **DR-3 (non-blocking until task 11):** the plan does not say where in the
   `docs/end-users/src/index.md` table of contents the two new pages are linked, and does not name
   `docs/sitemap.xml`, which lists every current reference page.
4. **DR-4 (non-blocking; tasks 6, 7, 10):** the error location for the second case of
   T3-CONST-5 (`n = null` after an assignment) is ambiguous against §4.10's "first error in
   source order" and the §5.3 `cannot assign to constant` row.
