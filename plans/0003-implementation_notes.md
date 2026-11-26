# Plan 0003 — Implementation Notes

## Task 1 — Lexer stubs and preliminary tests
- **Earlier runs.** None found. Task 1 `Stub and tests` was `Pending`, `token.hpp` had no plan
  0003 kinds, `lexer_test.cpp` had no T3 cases, and this file did not exist.
- **Stubs (`src/frontend/lexer/token.hpp`).**
  - New punctuation kinds, inserted after `Question`: `Bang`, `Assign`, `EqualEqual`,
    `BangEqual`.
  - New reserved-word kinds, appended in a "Reserved by plan 0003" group after `KeywordTypeof`:
    `KeywordLet`, `KeywordConst`, `KeywordTrue`, `KeywordFalse`, `KeywordSymbol`,
    `TypeBoolean`, `TypeSymbol`.
  - Docs updated: the file brief cites plan 0003 §4.2; the enum brief says 35 reserved words; the
    `Identifier` doc states the §4.2.1 rule and the `invalid identifier` error.
  - Adding enumerators triggers no `-Wswitch` warning. The only `switch` on `TokenKind` is
    `toTypeName` in `parser.cpp`, and it has a `default`.
- **`src/frontend/lexer/lexer.cpp`: no change at the stub stage.** The plan names no new
  function (task record: "no new functions are named by the plan"), and no private helper was
  stubbed. For the generator:
  - extend the keyword table (`std::array<Pair, 28>`) to 35 entries;
  - add the identifier-rule check after the maximal word is read (keywords first; then
    `invalid identifier` at the word's first byte);
  - lex `!`, `!=`, `=`, `==` with maximal munch;
  - add `Assign`, `EqualEqual`, `BangEqual`, and `Bang` to `negativeLiteralContext`.
- **Unit tests added (`tests/unit/frontend/lexer/lexer_test.cpp`).** The new cases are appended
  after "Lexer::tokenize may be called only once". Two helpers are added in a second anonymous
  namespace: `lexicalFailure` (location and message) and `statementKinds`.
  - T3-LEX-1: the 7 names lex as identifiers, also in `let`/`const`/`symbol` name positions.
  - T3-LEX-2: `_`, `__`, `_1count`, `__9a` give `invalid identifier` at the first byte: alone, as
    a function name, a parameter name, a `let` name, and an operand. `1count` keeps
    `digit sequence immediately followed by an identifier`.
  - T3-LEX-3: the 7 words lex as their kinds; the near words lex as identifiers; `?Boolean` and
    `?Symbol` lex as `Question` and then the type word.
  - T3-LEX-4: the 10 listed streams with byte columns. `!` at end of file is checked on
    exact-size storage, plus `=` at end of file (same lookahead boundary).
  - T3-LEX-5: negative literals after `=`, `==`, `!=`, `!` (including INT64_MIN); `x = - 1` is
    `Minus`. Lexical errors at the `-` are checked for `x = -0` and `a == -007`, plus the range
    (`a != -9223372036854775809`) and following-identifier (`!-1abc`) rules in the new contexts.
  - T3-LEX-6: `;` continuation after `=` (with `-1` on the next line), after `==`, and between
    `let x:` and the type.
- **Status before implementation.**
  - Debug and `build-asan` builds: no warnings.
  - `clang-tidy-18 -p build` on `lexer.cpp` and `lexer_test.cpp`: no diagnostics.
  - Unit tests: 111 cases. The 105 existing cases pass. The 6 new T3-LEX cases fail as expected
    (61 assertions), with no sanitizer reports.
  - ctest: 382 of 383 pass. The one failure is the aggregate `philios_tests` entry, which fails
    only because of the 6 new cases.
- **Open item for the developer (not blocking).** The `Lexer::tokenize` doc comment in
  `src/frontend/lexer/lexer.hpp` still lists the expression-start context as "`(`, `,`, or
  `return`". The file header still says "the 28 reserved words". `lexer.hpp` is not in task 1's
  file list, so this stage did not edit it. Once implemented, these comments will be stale.
  Either authorize a comment-only edit in task 1 or leave it to task 11 or a later decision.


## Task 1 — Lexer implementation
- Implemented in `src/frontend/lexer/lexer.cpp` only (stubs and T3 tests were already present):
  - keyword table now includes all 35 reserved words, adding `let`, `const`, `true`, `false`,
    `symbol`, `Boolean`, and `Symbol`;
  - identifier validation for non-keyword maximal words now enforces `_*[A-Za-z][A-Za-z0-9_]*` and
    throws `LexicalError` with `invalid identifier` at the word's first byte;
  - punctuation lexing now recognizes `!`, `=`, `==`, and `!=` with maximal munch;
  - negative-literal context now also includes `Assign`, `EqualEqual`, `BangEqual`, and `Bang`.
- Verification after implementation:
  - `cmake --build build` passes with no warnings;
  - `ctest --test-dir build --output-on-failure` passes (383/383);
  - `cmake --build build-asan` and `ctest --test-dir build-asan --output-on-failure` pass (383/383),
    with no sanitizer failures;
  - `clang-tidy-18 -p build src/frontend/lexer/lexer.cpp tests/unit/frontend/lexer/lexer_test.cpp`
    returns exit code 0 with no user-code diagnostics.

## Task 2 — AST stubs and preliminary tests
- **Earlier run (found in the workspace, kept as is).** Task 2 `Stub and tests` was
  `In progress`. An earlier run had already made these changes, and this run audited each one
  against the Task 2 scope and DR-1 (a):
  - `src/frontend/ast/ast.hpp`:
    - `TypeName::Boolean` and `TypeName::Symbol`.
    - The 7 new `NodeKind` values.
    - `UnaryOperator { Not }` and `BinaryOperator { Equal, NotEqual }`.
    - Declarations of `BooleanLiteral`, `UnaryExpr`, `BinaryExpr`, `LetDecl`, `ConstDecl`,
      `SymbolDecl`, and `AssignStmt`, with docblocks.
    - `ModuleItem`, `Program`, and `asFunctionDecl` exactly as in §4.14.2.
    - The file, `DeclaredType`, `FunctionDecl`, constructor, and `name()` comments now describe
      owned names and module items.
    - `noexcept` is removed from the `Identifier` and `Param` constructors.
    - `Identifier`, `Param`, and `FunctionDecl` still hold `std::string_view name_`, each marked
      with a "becomes an owned std::string" stub comment.
  - `src/frontend/ast/ast.cpp`: bodies for every new node, with `noexcept` worklist destructors
    for `LetDecl`, `ConstDecl`, `AssignStmt`, `UnaryExpr`, and `BinaryExpr`, plus
    `asFunctionDecl`.
  - DR-1 (a) compile-only edits:
    - `validator.cpp`: placeholder `case` labels that throw `InternalError`, and `Program`
      iteration through `asFunctionDecl`.
    - `evaluate.cpp`: 7 `evaluate` bodies that throw `InternalError`.
    - `module.cpp`: iteration through `asFunctionDecl`.
    - `type_check.cpp`: `Boolean`/`Symbol` cases that return "not accepted".
  - `parser_test.cpp`: the AC-2 `asFunctionDecl` adaptation. Only `program[i]->…` and
    `*program[i]` accesses changed; the inputs and assertions did not.
  - No Task 2 unit tests existed, and none of the comment-only files had been updated.
- **Added by this run.**
  - Comment-only edits named by plan T2:
    - `parser.hpp`: the `Parser` ownership comment. The AST keeps no view of the buffer; only
      the tokens do.
    - `module.hpp`: the class comment and the `release()` comment. Only tokens view the buffer,
      during `load()`.
    - `ast_builders.hpp`: the `name` and `callee` parameters are copied and need not outlive
      the node.
    - `source_pipeline.hpp`: `parseSource`'s source must outlive the tokens, not the AST.
  - `ast_builders.cpp` is unchanged. The plan names no builders for the new nodes, and the tests
    construct them directly.
  - The ownership comments in `ast.hpp`, `parser.hpp`, and `module.hpp` describe the target
    contract. They become true when the generator converts the three `name_` members.
- **Unit tests added (`tests/unit/frontend/ast/ast_test.cpp`).** The new tests are appended in a
  second anonymous namespace plus 5 test cases. The only changes to existing lines are 3 new
  `#include` lines and a plan 0003 note in the file comment.
  - Static asserts:
    - The 7 new nodes are nothrow destructible.
    - The `Identifier` and `Param` constructors are not nothrow.
  - `T3-ARCH-9 every named node owns its name text`: one section for each of the 7 named node
    types, using a 47-byte heap name.
    - First the source string is overwritten in place, and the test `REQUIRE`s the original
      `name()`. The buffer stays allocated, so a node that still holds a view fails cleanly with
      no sanitizer error.
    - Only if that passes is a second source string destroyed before `name()` is read. This
      keeps ASan from aborting the binary while the stub views remain.
  - `Plan 0003 asFunctionDecl shares FunctionDecl items and rejects other items`:
    - Shared control block: same address, `use_count` is 2, and `owner_before` is equivalent in
      both directions. The declaration survives `item.reset()` and expires with the last owner.
    - `ConstDecl`, `SymbolDecl`, and other kinds give null and take no ownership.
    - A null item, or `nullptr`, throws `InternalError`.
    - A mixed `Program` keeps its items in order.
  - `Plan 0003 declaration, assignment, and operator nodes keep their parts`: `kind()`,
    locations, and accessors for every new node, including `BinaryExpr`'s location coming from
    its left operand. Null operands or values throw `InternalError`.
  - `T3-NEST-5 a body with 10,000 let declarations and 10,000 assignments is destroyed`: the
    statements are in a `main` body owned by a `FunctionDecl`.
  - `T3-NEST-5 operator expressions are destroyed without recursion per node`:
    - The equality chain and the alternating `!`/`!=` nesting are each built at
      `kMaxParseNesting`.
    - Chains of 200,000 operators nest through the left operand and through the right operand.
    - 200,000 mixed levels are used as initializers of a `let`, an assignment, and a module-level
      `const` held through a `ModuleItem` (the `shared_ptr` destruction path).
- **Verification (this run).**
  - `cmake --build build` and `cmake --build build-asan`: no warnings.
  - `clang-tidy-18 -p build` on the following files: no diagnostics (exit 0).
    - `ast.cpp`, `validator.cpp`, `evaluate.cpp`, `module.cpp`, `type_check.cpp`, `parser.cpp`
    - `ast_test.cpp`, `parser_test.cpp`, `ast_builders.cpp`, `source_pipeline.cpp`
  - Unit tests: 116 cases in both Debug and `build-asan`. 115 pass. The one failure is T3-ARCH-9,
    with 3 failed assertions, one each in the `Identifier`, `Param`, and `FunctionDecl` sections.
    The overwritten source shows through the stub `std::string_view` members. This is the
    expected failure until the generator converts the three `name_` members.
  - No ASan or UBSan reports. LeakSanitizer was enabled.
  - ctest: 382 of 383 pass in `build` and in `build-asan`. The one failure is the aggregate
    `philios_tests` entry, caused only by T3-ARCH-9.
  - All existing tests pass unchanged, apart from the AC-2 `parser_test.cpp` adaptation.
- **For the generator.** Change `name_` of `Identifier`, `Param`, and `FunctionDecl` to
  `std::string` and remove the stub comments. The constructors already copy through
  `name_(name)`. No other Task 2 behavior is outstanding.
- **Open item for the developer (not blocking).** The `Module::load` doc comment in `module.hpp`
  still ends with "so every token and AST view refers to the buffer's final owner". After Task 2,
  AST nodes hold no views, so the words "and AST view" become stale. Plan T2 names only the
  `module.hpp` class and `release()` comments, so this stage left that comment alone. Task 9
  edits `Module::load`'s documentation, so it could fix this if authorized.


## Task 2 — AST implementation
- Continued from the existing Task 2 stubs run: only the remaining owned-name change in
  `src/frontend/ast/ast.hpp` was outstanding.
- Updated `Identifier`, `Param`, and `FunctionDecl` member storage from `std::string_view` to
  owned `std::string`, and removed the three "Plan 0003 task 2 stub" comments.
- Kept constructor signatures and `name()` signatures unchanged (`std::string_view` in, copied
  into owned storage; `std::string_view` out as a view of the owned member).
- Verified Task 2 behavior already present in prior artifacts remains intact: `detachChildren`,
  worklist-based `noexcept` destructors using `destroyWorklist`, `Identifier`/`Param` constructor
  `std::bad_alloc` contract, and `asFunctionDecl` null/shared-ownership semantics.
- Verification after this implementation:
  - `cmake --build build` and `cmake --build build-asan` succeeded with no warnings.
  - `ctest --test-dir build --output-on-failure` and
    `ctest --test-dir build-asan --output-on-failure` both passed (383/383), including the
    previously failing T3-ARCH-9 check through `philios_tests`, with no sanitizer reports.
  - `clang-tidy-18 -p build src/frontend/ast/ast.cpp` completed with exit code 0 and no
    user-code diagnostics.


## Task 3 — Parser stubs and preliminary tests
- **Earlier runs.** None found. Task 3 `Stub and tests` was `Pending`. `parser.cpp` had no
  changes from git `HEAD`. `parser.hpp` had only Task 2's ownership-comment change.
  `parser_test.cpp` had only Task 2's AC-2 `asFunctionDecl` adaptation. This file had no Task 3
  section.
- **Structure chosen (architecture decisions §2.3, §6).** One parse function per precedence
  level, each delegating to the next tighter level:
  `parseExpression` (`expression := equality`) → `parseEquality` → `parseUnary` →
  `parsePrimary`. `parseExpression` keeps its name and call sites. Its current body (the plan 0002
  `primary` alternatives) is meant to move into `parsePrimary`.
- **Stubs (`src/frontend/parser/parser.cpp`, private members of `ParserImpl`).** They are
  appended after `parseCallExpression` and before the data members. Each has a docblock with the
  rules to implement, and each body throws `InternalError("Parser::<name> is a plan 0003 task 3
  stub")`. No existing code calls them yet, so all current behavior is unchanged. The generator
  wires them in.
  - `struct VariableDeclarationParts`: keyword location, name, name location, `DeclaredType`, and
    initializer. It is shared by `let`/`const` in both scopes. The caller builds the node with
    `std::make_unique` in a block, or with `std::make_shared<const ConstDecl>` for a module item
    (§4.14.2).
  - `ModuleItem parseModuleItem()`: dispatches `function`/`const`/`symbol`. `let` is an error at
    `let`; any other token is an error at that token. Initializers start at nesting level 0.
  - `VariableDeclarationParts parseVariableDeclarationParts()`
  - `const Token& parseSymbolDeclarationName()`
  - `std::unique_ptr<ASTNode> parseAssignStatement()`
  - `parseEquality()`: iterative, with one `enterNesting` per operator at the operator token.
    The levels are released when the chain ends, including on the error path.
  - `parseUnary()`: `!` enters one level at the `!`. `!!` gives `repeated ! is not allowed` at
    the second `!`.
  - `parsePrimary()`: adds `true` and `false`. `parsePseudoCall`'s `echo` literal rule extends
    to `BooleanLiteral`.
  - The existing helpers `isBaseTypeToken`, `isNullableBaseTypeToken`, and `toTypeName`, plus
    `parseProgram`, `parseStatement`, and `parsePseudoCall`, still need their Task 3 edits:
    `Boolean`/`Symbol` types, module-item dispatch, `let`/`const`/`symbol`/assignment dispatch,
    and the `echo(true|false)` rule. They are documented in the stub comments and left unchanged
    for the generator.
- **`src/frontend/parser/parser.hpp` (docblock only).** In the `Parser` constructor and
  `parseProgram` docblocks:
  - Replaced the `@return`, which no longer holds after the move to module items, with the
    module-item contract.
  - Appended the plan 0003 §6.1 nesting rules to `@param maxNesting`.
  - Appended the §4.3 types to the type-annotation paragraph.
  - Appended the §5.2 conditions to `@throws`.

  No signature changed.
- **Unit tests (`tests/unit/frontend/parser/parser_test.cpp`).** Existing lines are unchanged,
  apart from a plan 0003 note in the file comment and 6 new `#include` lines (`common/errors.hpp`,
  `frontend/lexer/token.hpp`, `support/require_value.hpp`, `<cstdint>`, `<limits>`, `<vector>`).
  Everything else is appended in a second anonymous namespace (helpers) and 21 test cases.
  - The helpers take expected error locations from the lexer's token stream (the n-th token of a
    kind). `syntaxFailure` catches only `SyntaxError`. Any other exception, including a stub's
    `InternalError`, fails the test case cleanly. Every node access goes through `nodeAs`, which
    `REQUIRE`s non-null and the right `kind()`, so a wrong AST can't crash the binary.
  - Test cases:
    - **T3-ARCH-1:** the four item kinds, `asFunctionDecl` results and shared pointers, the
      `ConstDecl` and `SymbolDecl` parts, and `InternalError` on a null item.
    - **T3-PARSE-1 to T3-PARSE-10:** T3-PARSE-5 also checks `a == b == c`, `true`/`false`
      operands, and `BinaryExpr` location and operator location. T3-PARSE-8 also checks that
      `echo(!true)` is accepted (§4.11.5: only literals are syntax errors).
    - **Function-scope parts:** a test of `LetDecl`, `ConstDecl`, `SymbolDecl`, and
      `AssignStmt` parts and locations.
    - **T3-LEX-1 (parser part):** the valid names and the T3-LEX-3 near words in all 7 name
      positions.
    - **T3-LEX-3 (parser part):** the 7 words in 7 name positions.
    - **T3-LEX-5 (parser part):** `x = - 1` at `-`, plus the accepted negative literals after
      `=`, `==`, `!=`, and `!`.
    - **T3-LEX-6 (parser part).**
    - **T3-SYM-5:** in a function and at module level.
    - **T3-NEST-1:** 255 and 256 operators for `==` and for `!=`, with the depth of the left
      spine checked.
    - **T3-NEST-2:** accepted at 256; rejected at the `(` and at the `!` that open 257.
    - **T3-NEST-3:** 100,000 `!` tokens in a `return` and in a `let`; 100,000-operator chains in
      a body (fails at operator 256) and at module level (fails at operator 257).
    - **T3-NEST-4:** three mixes, each at 256 and 257.
    - **T3-NEST-6:** 256 and 257 operators.
  - T3-NEST-3's "no per-operator recursion" check uses the `build-asan` run, which the plan
    allows. No 256 KiB thread-stack test was added, because the test suite has no thread
    facility today.
- **Verification (this run).**
  - `cmake --build build` and `cmake --build build-asan`: no warnings. Both build with
    `PHILIOS_CLANG_TIDY=ON`.
  - `clang-tidy-18 -p build src/frontend/parser/parser.cpp
    tests/unit/frontend/parser/parser_test.cpp`: exit 0, no user-code diagnostics.
  - Unit tests: 137 cases in both Debug and `build-asan`. 118 pass and 19 fail (110
    assertions). Every failing case is a new Task 3 case. All 116 earlier cases pass unchanged.
  - Three new cases already pass, because the current parser's errors fall at the same tokens:
    T3-PARSE-1, T3-PARSE-3, and T3-PARSE-7.
  - No ASan or UBSan reports, and no crash or abort.
  - ctest: 382 of 383 pass in `build` and in `build-asan`. The one failure is the aggregate
    `philios_tests` entry, which fails only because of the new Task 3 cases.
- **Open items for the developer.**
  1. **`a === b` and `a !== b` as whole statements (§5.2, T3-PARSE-2): the location is
     ambiguous.**
     - §5.2 lists them under "a second `=` … | that `=`", which puts the error at the `=` token
       (column 9).
     - The row "An identifier at statement start followed by a token other than `(` or `=` …
       | that token" puts the error at the `==`/`!=` token (column 7).
     - The tests check these sequences only in expression position (`return a === b`,
       `x = a === b`), where both rules give the `=` token.
     - Please decide the statement-position location. Tests can then be added.
  2. **T3-NEST-4 wording, "200 deep plus 56 `!` or `==` levels".** The tests read this as a
     maximum level of 256 that counts the function body as level 1, so 200 call levels plus 55
     operator levels is accepted and one more is rejected. This matches T3-NEST-1 and T3-NEST-2.
  3. **`!` or a call inside a later right operand of an equality chain (`a == a == !b`): the
     level is not tested.** A counter that holds every chain level until the chain ends (§6.1:
     "Each `==` or `!=` in a chain adds one level") gives a different level from the tree depth.
     The tests avoid this case. If the developer wants it pinned down, the counting rule needs a
     decision.


## Task 4 — Object model stubs and preliminary tests
- **Earlier runs.** None found. Task 4 `Stub and tests` was `Pending`. No `boolean.*` or
  `symbol.*` files existed. `object.hpp`, `execution_context.*`, and `object_test.cpp` had no
  changes from git `HEAD`. This file had no Task 4 section.
- **Choices left to this stage (architecture decisions §6).**
  - **No canonical `true`/`false` instances.** As with `Integer` literals, each evaluation that
    produces a Boolean creates a new `Boolean` object (plan §4.3, "Otherwise each literal
    evaluation creates a new object"). Booleans compare by value (D11), so identity is never
    observable. As a result, **`execution_context.{hpp,cpp}` is unchanged** and has no new
    accessors.
  - **`Boolean` derives from `ImmutableObject`**, like `Integer`, because it is an immutable
    value. **`Symbol` derives from `Object` directly**, like the stateless `NullObject`: it is
    pure identity with no contents. Both are `final`.
  - Public interface, following the `Integer`/`NullObject` pattern:
    - `explicit Boolean(bool) noexcept`, `objectType()`, and `bool value() const noexcept`.
    - `Symbol() noexcept = default` and `objectType()`. `Symbol` has no data members.
- **Stubs.**
  - `src/runtime/object_model/object.hpp`: `ObjectType::Boolean` and `ObjectType::Symbol` are
    appended after `Function`. Nothing else changed.
  - New `src/runtime/object_model/boolean.{hpp,cpp}` and `symbol.{hpp,cpp}`, with docblocks.
    Ownership, identity, "no AST pointer or source view", and "no global/static instance" are
    stated.
  - Stub bodies: the constructor stores the value. `Boolean::objectType()` and
    `Symbol::objectType()` return `ObjectType::Null`, and `Boolean::value()` returns `false`.
    Each is marked "Plan 0003 task 4 stub". These are `noexcept`, so they return wrong values
    instead of throwing.
  - **DR-1 (a) compile-only edit:** `src/runtime/evaluator/type_check.cpp` `scriptTypeName` gets
    `case ObjectType::Boolean:` and `case ObjectType::Symbol:` labels that return `{}`, with a
    "task 5 adds the script type names" comment. This was the only `switch` on `ObjectType`
    in `src/` or `tests/`. `matchesDeclaredType` needed no change.
  - `CMakeLists.txt` was not changed (DR-2). The `GLOB_RECURSE ... CONFIGURE_DEPENDS` picked up
    `boolean.cpp` and `symbol.cpp` on the next build. Both are in `build/compile_commands.json`.
- **Unit tests (`tests/unit/runtime/object_model/object_test.cpp`).**
  - Changes to existing lines: a plan 0003 paragraph in the file comment, and 3 new `#include`
    lines (`boolean.hpp`, `symbol.hpp`, `<memory>`). Everything else is appended after the
    existing test case: a second anonymous namespace of `static_assert`s, plus 4 test cases.
  - Static asserts:
    - `Boolean` derives from `ImmutableObject`. `Symbol` derives from `Object` but not from
      `ImmutableObject`. Both are `final`.
    - `Boolean` is nothrow-constructible from `bool`, is `explicit`, and has no default
      constructor. `Symbol` is nothrow default-constructible.
    - Neither class is copyable or movable (`kNeitherCopyableNorMovable`).
    - `sizeof(Symbol) == sizeof(NullObject)`: a Symbol holds no state, so no name, AST pointer, or
      source view (§4.14.2).
  - Test cases:
    - `Plan 0003 Boolean and Symbol meet the object-model class requirements`: records the
      static asserts in the test report. Passes.
    - `Plan 0003 Boolean and Symbol report their object types`: checks `objectType()` directly
      and through `std::shared_ptr<const Object>`. Also checks that the new tags differ from each
      other and from every existing tag.
    - `Plan 0003 Boolean holds the value it was constructed with`: `true` and `false`, read
      twice. Two `Boolean(true)` objects are distinct instances with equal values.
    - `Plan 0003 distinct Symbol objects are distinct instances`: two symbols have different
      addresses, and an owning copy has the same address. An owning `Object` copy keeps a symbol
      alive after the original is reset, and the symbol expires with its last owner (§6.3).
- **Verification (this run).**
  - `cmake --build build` and `cmake --build build-asan`: no warnings.
  - `clang-tidy-18 -p build` on `boolean.cpp`, `symbol.cpp`, `type_check.cpp`, and
    `object_test.cpp`: exit 0, no diagnostics.
  - Unit tests: 142 cases in both Debug and `build-asan`. 120 pass and 22 fail (121 assertions).
    - The 19 failures are the expected Task 3 parser cases (110 assertions), unchanged from the
      Task 3 stub run.
    - The other 3 failures are the new Task 4 cases that need the stub bodies (11 assertions).
    - All earlier non-Task-3 cases pass unchanged, and so does the new class-requirements case.
  - No ASan, UBSan, or LeakSanitizer reports, and no crash or abort.
  - ctest: 382 of 383 pass in `build` and in `build-asan`. The one failure is the aggregate
    `philios_tests` entry. Every CLI case passes.
- **For the generator.**
  - `Boolean::objectType()` should return `ObjectType::Boolean`.
  - `Boolean::value()` should return `value_`.
  - `Symbol::objectType()` should return `ObjectType::Symbol`.
  - Remove the three stub comments. No other Task 4 behavior is outstanding.
- **Open items for the developer (not blocking).**
  1. The `ImmutableObject` doc comment in `object.hpp` still says "Base of the built-in
     immutable types (`Integer`, `String`)". After this task, `Boolean` also derives from it.
     The plan doesn't name that comment, so this stage left it alone. It could be updated in a
     comment-only edit if authorized (task 4's implementation or task 11).
  2. The `boolean.hpp` and `symbol.hpp` docblocks refer to `valuesEqual` in `type_check.hpp`,
     which task 5 adds. The reference is correct once task 5 is done.


## Task 4 — Object model implementation
- **Scope implemented.** Filled the three Task 4 stubbed bodies only:
  - `Boolean::objectType()` now returns `ObjectType::Boolean`.
  - `Boolean::value()` now returns `value_`.
  - `Symbol::objectType()` now returns `ObjectType::Symbol`.
- **Intentional non-changes.** Left the DR-1(a) placeholder in `scriptTypeName` (`type_check.cpp`) unchanged for Task 5, and made no `ExecutionContext`/`CMakeLists.txt` edits.
- **Verification (this run).**
  - `cmake --build build` and `cmake --build build-asan`: pass, no warnings emitted.
  - `clang-tidy-18 -p build src/runtime/object_model/boolean.cpp src/runtime/object_model/symbol.cpp`: clean.
  - `cd build && ctest --output-on-failure`: 382/383 pass; only `philios_tests` fails.
  - `cd build-asan && ctest --output-on-failure`: 382/383 pass; only `philios_tests` fails.
  - In both `philios_tests` runs, failures are exactly the 19 known Task 3 parser/nesting cases (110 assertions); no Task 4 case fails.
  - Targeted Task 4 cases pass in both builds (`./philios_tests` filter covering all four `Plan 0003` object-model cases): 36 assertions in 4 test cases, all pass.
  - No ASan/UBSan/LSan diagnostics were reported.

## Task 4 — Revision (ImmutableObject comment)

- **Developer decision (2026-11-22, explicit):** the stale `ImmutableObject` doc comment in
  `src/runtime/object_model/object.hpp` is fixed under plan 0003 as a comment-only correction
  that adds `Boolean`. This resolves the item noted in `plans/0003-code_review.md` (Task 4) as
  pending developer direction.
- **Exact change (one line, no other edits):**
  - Before: `/// @brief Base of the built-in immutable types (`Integer`, `String`).`
  - After: `/// @brief Base of the built-in immutable types (`Integer`, `String`, `Boolean`).`
- **Task record:** Task 4 `Revision` set to `Done` in `plans/0003-implementation-tasks.md`;
  change-log entry appended. `Review` stays `GO` pending re-review.
- **Verification:**
  - `cmake --build build` and `cmake --build build-asan`: succeed with no warnings.
  - `clang-tidy-18 -p build --quiet src/runtime/object_model/boolean.cpp`: exit 0, no diagnostics.
  - `cd build && ctest --output-on-failure`: 382/383 pass; only `philios_tests` fails (141 test
    cases: 122 pass, 19 fail; 1334 assertions: 1224 pass, 110 fail).
  - `cd build-asan && ctest --output-on-failure`: 382/383 pass; same `philios_tests` counts.
  - Every failure in both runs is in `tests/unit/frontend/parser/parser_test.cpp` (the 19 known
    Task 3 cases, not yet implemented). No ASan/UBSan/LSan diagnostics.


## Task 3 — Parser implementation (2026-11-22)
- Implemented Task 3 parser logic in `src/frontend/parser/parser.cpp`:
  - module-item parsing now accepts `function`, `const`, and `symbol` in source order; `let` and any other module-level starter now fail at the starter token;
  - declaration and statement parsing now supports function-scope `let`, `const`, `symbol`, and `assign` statements, with identifier-statement disambiguation (`(` call, `=` assignment, otherwise syntax error at the following token);
  - expression parsing now implements `expression -> equality -> unary -> primary`, including `BooleanLiteral`, iterative left-associative `==`/`!=` chains, `!` parsing, and the `repeated ! is not allowed` syntax error at the second `!`;
  - parser type-name handling now accepts `Boolean`, `Symbol`, `?Boolean`, and `?Symbol`;
  - `echo(true)` / `echo(false)` are now syntax errors at the argument token by extending the pseudo-call literal rule to `BooleanLiteral`.
- `src/frontend/parser/parser.hpp` and `tests/unit/frontend/parser/parser_test.cpp` required no implementation edits; existing Task 3 tests were used as-is.
- Verification:
  - `cmake --build build` succeeded with no warnings;
  - `ctest --output-on-failure` in `build/`: **383/383 passed**;
  - `cmake --build build-asan` succeeded with no warnings;
  - `ctest --output-on-failure` in `build-asan/`: **382/383 passed**; only `t_parse_5_long_line` failed due the CLI harness 20s timeout (`Process terminated due to timeout`);
  - `clang-tidy -p build-tidy src/frontend/parser/parser.cpp src/frontend/parser/parser.hpp tests/unit/frontend/parser/parser_test.cpp --checks='cert-*,bugprone-*' --warnings-as-errors='*'` exited 0 with no user-code diagnostics.
- Blocking item for Task 3 completion: ASan/UBSan full-suite gate is still failing on `t_parse_5_long_line` timeout (non-Task-5 failure), so Task 3 remains `Implementation: In progress` pending developer direction on whether this environment-specific timeout is acceptable or requires additional parser/runtime performance work under plan 0003.


## Task 3 — Parser follow-up (2026-11-22)
- Scope-limited cleanup in `src/frontend/parser/parser.cpp`: `parseExpression()` now delegates directly to `parseEquality()` (`return parseEquality();`) so the parser follows the documented chain `expression := equality -> unary -> primary` without a duplicated equality loop or a separate `parsePrimary()` fast-path.
- External timing evidence provided by the orchestrator for `t_parse_5_long_line` was preserved as context and not treated as a Task 3 regression:
  - previous Debug ctest: 383/383 passed;
  - previous ASan full ctest: 382/383 passed, only `t_parse_5_long_line` timed out at the 20 s harness limit;
  - committed-HEAD ASan binary on the same 2 MiB input: 14.2 s and 20.7 s;
  - working-tree ASan binary on the same input: 31.9 s and 19.5 s;
  - peak RSS was the same (~399 MB);
  - gprof CPU profiles were near-identical (2.77 s vs 2.89 s), dominated by lexer token-vector growth.
- Verification run for this follow-up:
  - `cmake --build build`: pass, no warnings;
  - `cmake --build build-asan`: pass, no warnings;
  - `ctest --test-dir build --output-on-failure`: 383/383 passed;
  - parser unit coverage in ASan (`./philios_tests "T3-*"`): all passed (1082 assertions, 30 test cases);
  - `ctest --test-dir build-asan -R t_parse_5_long_line --output-on-failure` (2 runs, max allowed): passed in 4.09 s and 4.81 s;
  - `clang-tidy -p build-tidy src/frontend/parser/parser.cpp --checks='cert-*,bugprone-*' --warnings-as-errors='*'`: exit 0, no user-code diagnostics.
- Task file update: in `plans/0003-implementation-tasks.md`, Task 3 `Implementation` set from `In progress` to `Done` after the checks above.

## Task 3 — Parser — Revision for Round 1 findings 2 and 3 (2026-11-22)
- Scope: only Round 1 Findings 2 and 3 of `plans/0003-code_review.md` (Task 3 — Parser).
- Finding 2 (Low, fixed): replaced stale stub-stage wording with present-tense descriptions.
  - `src/frontend/parser/parser.cpp` (section comment before the plan 0003 parse functions): "Stubs: the bodies below throw / InternalError until the implementation stage." became "The functions below implement the / plan 0003 grammar and §5.2 syntax errors." The rest of the comment block is unchanged.
  - `tests/unit/frontend/parser/parser_test.cpp` (`syntaxFailure` doc comment): "(a LexicalError, or the InternalError of a task 3 stub)" became "(a LexicalError or an InternalError)".
- Finding 3 (Low, fixed): wrapped the `symbolKeyword` declaration in `parseSymbolDeclarationName()` after `=`, with an 8-space continuation indent, matching the existing `rightParen` declaration style.
- Finding 1 (Medium, statement-position `a === b` / `a !== b` error location): **not addressed; pending a developer decision.** Plan §5.2 has conflicting rows (the identifier-statement rule puts the error at the first `==`/`!=` token, but the second-`=` row lists `a === b` and `a !== b` with the error at the second `=`). No parser behavior or tests were changed for it.
- Observation (not changed, out of scope): `src/frontend/parser/parser.cpp:160` (`parseDeclaredType("expected parameter type")));`) is 104 columns. This line was already too long at `HEAD`, and no finding covers it.
- Verification:
  - `cmake --build build`: pass, no warnings;
  - `build/philios_tests` filtered to all 31 test cases in `parser_test.cpp`: all passed (963 assertions); `build/philios_tests "T3-*"`: all passed (1082 assertions in 30 test cases);
  - `ctest --test-dir build --output-on-failure`: 383/383 passed;
  - `clang-tidy -p build-tidy src/frontend/parser/parser.cpp tests/unit/frontend/parser/parser_test.cpp --checks='cert-*,bugprone-*' --warnings-as-errors='*'`: exit 0, no user-code diagnostics;
  - `git diff --check` on both files: clean.
- Task file update: Task 3 `Revision` set from `Not required` to `Pending`. It stays `Pending` until Finding 1 is resolved. No other field was changed.

## Developer decision — Task 3 Round 1 finding 1 (recorded 2026-11-24)
- **Decision (developer, 2026-11-22):** statement-position `a === b` and `a !== b` are syntax errors at the second `=` (the `=` after `==`/`!=`). The "second `=`" row of plan §5.2 takes precedence over the "identifier at statement start followed by a token other than `(` or `=`" row for these forms, consistent with T3-PARSE-2.
- **Date correction:** the heading of the previous Task 3 revision entry was dated 2026-11-24 in error; corrected to 2026-11-22 on the developer's instruction.

## Task 3 — Parser — Revision for Round 1 finding 1 (2026-11-24)
- Scope: only Round 1 Finding 1 of `plans/0003-code_review.md` (Task 3 — Parser), applying the developer decision of 2026-11-22 recorded above.
- `src/frontend/parser/parser.cpp` (`parseStatement`, identifier-at-statement-start branch): added a check before the generic `invalid statement` throw. When the identifier is followed by `==` or `!=` and the next token is `=`, it throws `SyntaxError` at that `=` with `invalid expression`. The parser has no dedicated second-`=` message. `invalid expression` is the message the same token sequence already gets at that `=` in expression position (`return a === b`), so the message for these forms is the same in both positions. All other forms are unchanged: `a == b` and `a != b` without a following `=` still fail at the operator with `invalid statement`, and expression-position parsing was not modified.
- `tests/unit/frontend/parser/parser_test.cpp`: removed the two-line comment at the top of the T3-PARSE-2 test case that excluded statement position. Added a new test case right after it, "T3-PARSE-2 statement a === b and a !== b are errors at the second =". In `main`, it checks that `a === b` and `a !== b` fail at {2, 9}, and that `a == b` and `a != b` fail at {2, 7}. No existing assertion was changed.
- Verification:
  - `cmake --build build` and `cmake --build build-asan`: pass, no warnings;
  - `build/philios_tests "T3-PARSE-2*"`: all passed (15 assertions in 2 test cases);
  - `ctest --test-dir build`: 383/383 passed;
  - `ctest --test-dir build-asan`: 383/383 passed, including `t_parse_5_long_line` (no timeout);
  - `clang-tidy -p build-tidy src/frontend/parser/parser.cpp tests/unit/frontend/parser/parser_test.cpp --checks='cert-*,bugprone-*' --warnings-as-errors='*'`: exit 0, no user-code diagnostics;
  - `git diff --check` on both files: clean.
- Task file update: Task 3 `Revision` set from `Pending` to `Done`. Findings 2 and 3 were fixed in the 2026-11-22 revision. No other field was changed.

## Task 5 — Type checks and equality stubs and preliminary tests (2026-11-24)
- **Status:** done. Task 5 `Stub and tests` was set from `In progress` to `Done`. No other field
  was changed.
- **Earlier runs.** Two earlier attempts left `Stub and tests` at `In progress` with no Task 5
  stub work: `type_check.hpp` and `type_check_test.cpp` matched git `HEAD`; `type_check.cpp`
  held only the DR-1 placeholders from Tasks 2 and 4, which this run leaves for the generator.
- **Stubs.**
  - `type_check.hpp`: added `[[nodiscard]] bool valuesEqual(const Object& left,
    const Object& right) noexcept;`. Its docblock covers §4.11.2/§4.11.3 (D11, D21) and §6.2/§6.3.
  - `type_check.cpp`: the stub body returns `false` and is marked "Plan 0003 task 5 stub".
  - **Suppression.** The in-build clang-tidy reported `bugprone-easily-swappable-parameters` on
    the plan-mandated signature. Equality is symmetric, so swapping the operands cannot change
    the result. The definition therefore carries a commented
    `// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)`, the first NOLINT in `src/`.
    The generator should keep it.
- **Preliminary tests** (12 new `TEST_CASE`s appended to `type_check_test.cpp`; the existing 3
  are unchanged; the new includes are `boolean.hpp`, `symbol.hpp`, `<cstdint>`, `<limits>`):
  - T3-TYPE-1/T3-TYPE-2: a 6×6 acceptance table (`Boolean`, `?Boolean`, `Symbol`, `?Symbol`,
    `Object`, `?Object` × Integer, String, Function, NullObject, Boolean, Symbol) and a check that
    the plan 0002 rows reject Boolean and Symbol except `Object`/`?Object`. Also tests
    `scriptTypeName` for `Boolean`/`Symbol`.
  - T3-EQ-1: value cases for distinct objects, including `INT64_MIN == INT64_MIN` and
    `INT64_MAX` vs `-1`. T3-EQ-2: the five plan pairs in both orders, plus all 30 ordered
    mixed-class pairs. T3-EQ-3: `Function` identity and D21/same-bytes strings. T3-SYM-6: `Symbol`
    identity. T3-EQ-4: four chains, each composed through a `Boolean` holding the intermediate
    result. Also a `noexcept` static check.
- **Expected failures against the stubs:** 8 of the 12 new cases (26 assertions). They are:
  - T3-TYPE-1 T3-TYPE-2 acceptance table and T3-TYPE-2 `scriptTypeName`, from the DR-1
    placeholders;
  - both T3-EQ-1 cases, both T3-EQ-3 cases, T3-SYM-6, and the four T3-EQ-4 sections, from the
    `false` stub.

  The 4 new cases that pass now are the two T3-EQ-2 cases, the plan 0002 rows rejection, and
  `noexcept`.
- Verification:
  - `cmake --build build`: pass, no warnings;
  - `ctest --test-dir build`: 382/383 passed; only `philios_tests` fails, and only on the
    expected Task 5 cases;
  - `philios_tests` excluding the new cases: all passed (2006 assertions in 143 test cases);
  - `clang-tidy -p build-tidy` on the three files with `--checks='cert-*,bugprone-*'
    --warnings-as-errors='*'`: exit 0;
  - `git diff --check`: clean, and no line exceeds 100 columns.


## Task 5 — Type checks and equality (2026-11-24, blocked)
- Verified identity and task gating: plan stem `0003` is consistent across `plans/0003.md`,
  `plans/0003-architecture_decisions.md`, and `plans/0003-implementation-tasks.md`;
  Task 5 exists; Task 5 dependencies (Tasks 2 and 4) are complete; Task 5 `Stub and tests` is
  `Done`.
- Blocking issue: Task 5 requires exact `std::int64_t` Integer equality (plan §6.2), but within
  the approved Task 5 file scope (`type_check.hpp/.cpp`, tests only for inconsistency) the
  `Integer` object model exposes no accessor for its stored value. `Integer` currently exposes
  only `exitStatus()`, which is modulo 256 and cannot implement exact equality (for example,
  `INT64_MIN` and `0` both map to `0`).
- Per the preservation/scope rule, implementation was paused before code changes in evaluator
  files, pending developer direction on whether to authorize the minimal interface addition
  (for example `Integer::value() const noexcept`) in `src/runtime/object_model/integer.hpp/.cpp`.

## Developer decision — Task 5 Integer equality (recorded 2026-11-24)
- **Decision (developer, 2026-11-22): option (b).** Add `[[nodiscard]] bool Integer::equals(const Integer&) const noexcept` in `src/runtime/object_model/integer.hpp`/`.cpp`. Integer storage stays private; no value accessor is added. `valuesEqual` uses `Integer::equals` for exact `std::int64_t` comparison (§6.2). Task 5 may edit those two files and add a focused test to `tests/unit/runtime/object_model/object_test.cpp`, including boundary values such as `INT64_MIN` vs `0` (same exit status, unequal).

## Task 5 — Type checks and equality (2026-11-24, implementation)
- Implemented the three Task 5 replacements in `src/runtime/evaluator/type_check.cpp`:
  - `matchesDeclaredType` now accepts `Boolean`/`Symbol` runtime values for `Boolean`/`Symbol`
    declared types; `Object` remains any non-null runtime value and `?T` still admits null.
  - `scriptTypeName` now returns `"Boolean"` and `"Symbol"`.
  - `valuesEqual` now implements D11 by runtime class: different classes are `false`; `Integer`
    calls `Integer::equals`; `String` checks length then bytes (`contentEquals`); `Boolean` compares
    `value()`; `Null` is always equal; `Function`/`Symbol` (and any remaining reference-like class
    tags) compare by object identity (`&left == &right`).
- Implemented the developer-decided Integer hook (option b) without exposing storage:
  `[[nodiscard]] bool Integer::equals(const Integer& other) const noexcept` was added to
  `src/runtime/object_model/integer.hpp/.cpp` and used by `valuesEqual`.
- Added the focused regression in `tests/unit/runtime/object_model/object_test.cpp` for
  `Integer::equals`: equal/unequal simple values, `INT64_MIN == INT64_MIN`,
  `INT64_MAX == INT64_MAX`, `INT64_MIN != 0`, and `INT64_MAX != -1`.
- Kept the existing
  `// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)` suppression on `valuesEqual` with its
  adjacent reason comment (equality is symmetric).
- Verification (Task 5 completion run):
  - `cmake --build build` and `cmake --build build-asan`: pass, warning-free.
  - `ctest --test-dir build --output-on-failure`: 383/383 passed.
  - `ctest --test-dir build-asan --output-on-failure`: 383/383 passed.
  - `clang-tidy -p build-tidy --checks="cert-*,bugprone-*" --warnings-as-errors="*"` on all
    Task 5 changed files: exit 0, no user-code diagnostics.

## Task 5 — Type checks and equality (2026-11-24, revision for code review Round 1)
- Finding 1 (Low, `type_check.hpp:15-30`): added the `Boolean` and `Symbol` rows to the
  `matchesDeclaredType` acceptance table (plan §4.3 order, existing table format), and replaced the
  stale "ObjectType is unchanged" sentence with "Boolean and Symbol have their own ObjectType tags
  (plan 0003 §4.3)." No other documentation lines changed.
- Finding 2 (Low, `type_check.hpp:39`, `type_check.cpp:87`): wrapped only those two lines within
  100 columns. The `.cpp` wrap keeps the operator at the end of the line, matching existing code.
- No behavior change.
- Verification:
  - `cmake --build build` and `cmake --build build-asan`: pass, warning-free.
  - `ctest --test-dir build --output-on-failure`: 383/383 passed.
  - `ctest --test-dir build-asan --output-on-failure`: 383/383 passed (no flake rerun needed).
  - `clang-tidy -p build-tidy --checks="cert-*,bugprone-*" --warnings-as-errors="*"` on
    `type_check.hpp` and `type_check.cpp`: exit 0, no user-code diagnostics.
  - `git diff --check` scoped to `src/` and `tests/`: clean. The unscoped run reports only
    trailing whitespace in `plans/0005.md`. That is outside the plan 0003 scope and was not
    touched.

## Task 6 — Validator: scopes and names stubs and preliminary tests (2026-11-24)
- **Stubs:** none. Task 6 adds no public interface (internal helpers only). `validator.hpp` is
  unchanged, and `validateProgram` keeps its signature and `void` return (AD1).
  `validator.cpp` still holds only the Task 2 DR-1 placeholders; the generator replaces them.
- **Tests** (`tests/unit/analysis/semantic_analyzer/validator_test.cpp`, through
  `source_pipeline`): a new file-local helper, `checkDeclarationError`, asserts the class
  (`ErrorClass::Declaration`), the line and column, and the exact §5.3 message. Valid cases use
  `CHECK_NOTHROW(parseAndValidate(...))`. Any non-`DeclarationError` exception, such as an
  `InternalError` placeholder, propagates and fails only that test case. Most of the work was
  done in an earlier run that was interrupted. This run checked every case against plan §4.5,
  §4.5.1, §4.6–§4.9, §5.3, §9, and DR-4, and found no contradictions. It added only T3-MOD-2.
- **Coverage map:**
  - T3-DECL-2: `let label: T` for all 7 non-nullable types, plus `const c: Integer`, at the
    keyword; each case also runs in an uncalled function.
  - T3-DECL-3: all 6 function-scope pairs and all 6 module-level pairs (both orders), at the
    second name.
  - T3-DECL-4: three cases, including the variable's own initializer.
  - T3-DECL-5: `let`, `const`, and `symbol` named `main`, and `main = 1`.
  - T3-SCOPE-1: the existing fixture source at 3:12, and `f(f: Integer)`.
  - T3-SCOPE-2: 3 module-level kinds × 4 function-scope kinds × (called, uncalled) = 24
    sections.
  - T3-SCOPE-3: `let`, parameter, and `symbol` names, each before a later module declaration.
  - T3-SCOPE-4: the name error at `let x` (4:9), not a type error; `echo(z)` → `used before
    declaration`.
  - T3-SCOPE-5: `main`, then duplicate, then module-level check order.
  - T3-SCOPE-6: D19 within each scope.
  - T3-SCOPE-7 (validation part): two valid programs.
  - T3-MOD-2 (Task 6 validation part, added in this run): a function declared before
    `const k` uses `k`; the program validates.
  - T3-MOD-3: a later name and the declaration's own name.
  - T3-MOD-4: all five constructs, at their first byte.
  - T3-MOD-5: `n = "a"` and `k = 2`, in called and uncalled functions.
  - T3-CONST-1.
  - T3-CONST-2: one write validates; the second write fails at 6:5.
  - T3-CONST-4.
  - T3-CONST-11: the declaration validates; `z = null` fails with `cannot assign to constant` at
    the target. `z` is non-nullable, so the §5.3 `constant value must not be null` row, which is
    for nullable constants, does not apply.
  - T3-ASSIGN-5: undefined name, module function, module symbol, and `main`.
  - T3-SYM-5 (target part): `sym = 1` and `sym = sym`.
  - T3-CONST-5's second case (DR-4) belongs to Task 7 and is not tested here.
- **Build and test results (Debug, `build/`):**
  - `cmake --build build`: pass. `validator_test.cpp` compiles warning-free.
  - `philios_tests -# "[#validator_test]"`: 28 test cases.
    - 7 pass: all 7 existing cases (T-DECL-3, -4, -6, -7; T2-TYPE-5, -8, -9), unchanged (AC-2).
    - 21 fail: all 21 new Task 6 cases, as expected before implementation. There are 97
      failed assertions:
      - 48 throw the Task 2 placeholder `InternalError` "placeholder module item kind".
      - 43 throw the Task 2 placeholder `InternalError` "placeholder statement node kind".
      - 6 report the wrong message. The old D15 message `parameter name must not match a module
        function` appears where `name must not match a module-level declaration` is expected.
        `undefined identifier` appears where `used before declaration` is expected.
    - No failure comes from a malformed test source (no `SyntaxError`).
  - All other unit tests: 149 test cases, 2141 assertions, all pass.
  - `ctest` (Debug): all 382 CLI cases pass. The only failing ctest entry is the `philios_tests`
    unit binary, because of the expected Task 6 failures above.


## Task 6 — Validator: scopes and names implementation (2026-11-24)
- Replaced Task 2 DR-1 placeholder `InternalError` branches in `validator.cpp` for module items and
  statements with full Task 6 behavior.
- Added a module-level first pass that collects function/const/symbol names, reports
  `duplicate declaration` at the second declaration, and enforces `missing main function`.
- Added function-scope prechecks (parameters, then body `let`/`const`/`symbol` in source order)
  with the required rule order: `main` name rule, same-scope duplicate rule, then the
  module-level conflict rule (`name must not match a module-level declaration`).
- Implemented function-scope visibility tracking so names become visible only after their
  declaration statement; earlier reads/assignments and self-initializer reads report
  `used before declaration`.
- Implemented assignment-target validation rules for parameters/variables/constants/symbols and
  module-level names, including write-once tracking for function-scope nullable constants and
  `cannot assign to this name`/`cannot assign to constant` split.
- Implemented module-level initializer validation for allowed constructs and name-resolution rules:
  earlier const/symbol names are allowed, later/own const/symbol names are `used before
  declaration`, and calls/pseudo-calls/function names as values are
  `not allowed in a module-level initializer`.
- Boolean/Unary/Binary expressions are traversed for name-resolution and module-initializer
  restrictions only; Task 7 static type rules are not implemented here.
- Verification for this task run:
  - `cmake --build build`
  - `/home/dcloutman/projects/philios/build/philios_tests`
  - `ctest --test-dir build --output-on-failure`
  - `cmake --build build-asan`
  - `ctest --test-dir build-asan --output-on-failure`
  - `clang-tidy-18 -p build-tidy src/analysis/semantic_analyzer/validator.cpp tests/unit/analysis/semantic_analyzer/validator_test.cpp`

## Task 6 — Revision for Round 1 findings (2026-11-24)
- **Finding 1 (Medium) — accepted.** Plan §4.5 (last bullet) says a `const` or `symbol` named
  `main` is a declaration error at the name with `main may be used only as its declaration
  name`, with no scope qualification, and §5.3 lists the `main` message for `const` and `symbol`
  names. `collectModuleNames()` now raises that error at the name of a module-level `ConstDecl`
  or `SymbolDecl` named `main`, before the duplicate check, so it wins over both
  `missing main function` (1:1) and `duplicate declaration`. New test case
  `T3-DECL-5 a module-level const or symbol named main fails at the name` covers `const main`
  and `symbol main` with no `main` function, before `function main`, and after it; each
  section asserts class, exact name location, and message (24 assertions).
- **Finding 2 (Low) — accepted.** Wrapped the two new over-100-column lines in `validator.cpp`
  (assignment-target condition; `LocalConstantState` initializer). The remaining 101-column line
  at `validator_test.cpp:53` predates Task 6 and is unchanged.
- **Finding 3 (Low) — accepted.** The `declarationFailure` helper comment now describes an
  unexpected `InternalError` from the validator instead of an unimplemented path.
- Verification: `cmake --build build` (warning-free); `ctest --test-dir build` 383/383;
  `cmake --build build-asan` (warning-free); `ctest --test-dir build-asan` 383/383;
  `clang-tidy-18 -p build-tidy --checks='-*,cert-*,bugprone-*' --warnings-as-errors='*'` and the
  project-config run on both changed files: exit 0, no user-code diagnostics;
  `git diff --check` on both files clean.

## Task 7 — Validator: static types stubs and preliminary tests (2026-11-24)
- **Stubs:** none. Task 7 adds no public interface. `validator.hpp` and `validator.cpp` are
  unchanged; the generator adds the static-type helpers and updates the `validateProgram`
  documentation comment (§4.14.1).
- **Earlier runs:** none found. `validator_test.cpp` had no Task 7 cases, and `Stub and tests`
  was `Pending`.
- **Test file changes** (`tests/unit/analysis/semantic_analyzer/validator_test.cpp`), all
  additions; no existing test was changed:
  - a header-comment paragraph and the includes `<cstddef>` and `<type_traits>`;
  - a second file-local block with the five §5.3 static-type messages and a `StaticCase`
    table row;
  - 14 new `TEST_CASE`s appended after T3-SYM-5. They reuse `parseAndValidate`,
    `checkDeclarationError`, and `kTrailingMain`. Each error case asserts the class, the line
    and column, and the §5.3 message.
- **Coverage map:**
  - T3-ARCH-7: `static_assert` that `decltype(&validateProgram)` is `void (*)(const Program&)`.
  - T3-DECL-6: all six initializers → `type mismatch` at the initializer.
  - T3-DECL-7: four accepted subtypes validate.
  - T3-DECL-9: six cases → `nullable value for non-nullable target`, each in a called and an
    uncalled function. `let j: ?Integer = q()` and `let o: ?Object = n` validate. The dynamic
    `let i: Integer = g()` through a `Function` parameter validates.
  - T3-CONST-5:
    - The first case is at `null` (3:9) with `constant value must not be null`.
    - The second case follows DR-4 "target": at `n` (4:5) with `cannot assign to constant`.
      The task text's "class only" wording predates the DR-4 resolution.
  - T3-CONST-6: `n = r()`, `n = e()`, `n = echo("x")`, and `const m: ?String = r()` →
    `constant value must not be null` at the right-hand side. `n = p` with `p: Object` validates.
  - T3-ASSIGN-3: both cases → `type mismatch` at the right-hand side.
  - T3-ASSIGN-7 (static parts): the `p = "b"` program and `p = null` on `?String` validate.
    `p = 1` and `p = null` on `String` → `type mismatch`. `p = o` with `o: Object` validates.
  - T3-SYM-3: `Object` and `?Object` targets → `symbol not allowed here` at the name. The
    `Symbol`-variable widening program validates.
  - T3-SYM-4: `echo(sym)`, `typeof(sym)`, `f(sym)`, `sym()`, `let i: Integer = sym`, and
    `return sym` in a `Symbol` function → `symbol not allowed here`. `!sym` →
    `operand of ! must be Boolean` (see the interpretation below).
  - T3-OP-2: all nine operands → `operand of ! must be Boolean` at the operand (7:23), each in a
    called and an uncalled function. The `?Object` parameter is named `o`, because `p` is the
    `?Boolean` parameter.
  - T3-LEX-5: `let b: Boolean = !-1` → `operand of ! must be Boolean` at `-1`.
  - T3-EQ-2: the five pairs with `==` and `!=` (ten sections) validate.
  - T3-MOD-9: `type mismatch` at 1:20, `symbol not allowed here` at 2:19, and
    `operand of ! must be Boolean` at 1:21.
- **Interpretation (developer may override):**
  - For `!sym`, two §5.3 rows match:
    - the `!` row, which lists "is a bare symbol";
    - the general bare-symbol row.
  - The tests use the more specific `!` row, so the message is `operand of ! must be Boolean`.
    T3-OP-2 also lists `!sym` with the other `!` operand errors.
  - The location (the `sym` name) is the same under either row.
- **Build and test results (Debug, `build/`):**
  - `cmake --build build`: pass. `validator_test.cpp` compiles warning-free.
  - `philios_tests -# "[#validator_test]"`: 43 test cases, 489 assertions.
    - 32 cases pass:
      - all 29 existing cases (plans 0001 and 0002, and Task 6), unchanged;
      - T3-ARCH-7, T3-DECL-7, and T3-EQ-2;
      - the valid sections of the failing cases, and the DR-4 second case of T3-CONST-5,
        which the Task 6 code already enforces.
    - 11 cases fail, as expected before implementation: T3-DECL-6, T3-DECL-9, T3-CONST-5,
      T3-CONST-6, T3-ASSIGN-3, T3-ASSIGN-7, T3-SYM-3, T3-SYM-4, T3-OP-2, T3-LEX-5, and T3-MOD-9.
      - They have 58 failed assertions: DECL-6 6, DECL-9 12, CONST-5 1, CONST-6 4,
        ASSIGN-3 2, ASSIGN-7 2, SYM-3 2, SYM-4 7, OP-2 18, LEX-5 1, MOD-9 3.
      - Every failure is `expected a value`: validation accepted the source because the static
        checks are not implemented yet.
      - No failure is a `SyntaxError`, another declaration message, or an `InternalError`.
  - All other unit tests: 149 test cases, 2141 assertions, all pass.
  - `ctest --test-dir build`: 382/383. The only failing entry is the `philios_tests` binary,
    because of the expected Task 7 failures above.
  - `clang-tidy-18 -p build-tidy --checks='-*,cert-*,bugprone-*' --warnings-as-errors='*'` on
    the test file: exit 0, with no diagnostics in user code.
  - `git diff --check`: clean. No new line exceeds 100 columns. The only 101-column line is the
    existing line, now at line 59.


## Task 7 — Validator: static types implementation (2026-11-24)
- Implemented Task 7 static-type validation in `validator.cpp` with no new public interface:
  module/function identifier resolution now returns static type information (`Declared`, `Dynamic`,
  or bare `Symbol` name), and local/module declared-type tables are tracked for constants,
  parameters, and variables.
- Added §4.10 rule checks for `let`/`const` initializers, assignment right-hand sides, module-level
  const initializers, and `!` operands. Messages and locations match §5.3:
  `type mismatch`, `nullable value for non-nullable target`,
  `constant value must not be null`, and `operand of ! must be Boolean`.
- Implemented bare-symbol placement rules (§4.9, D17, D18): bare symbol names are allowed only as
  equality operands and as the whole RHS when the target type is `Symbol`/`?Symbol`; other
  contexts report `symbol not allowed here`. For `!sym`, the validator reports
  `operand of ! must be Boolean` at `sym` per the recorded Task 7 interpretation.
- Preserved DA7 runtime-only sites: no new static checks were added for call-argument types,
  return-value types, `echo` argument types, `typeof` argument types, or non-function callees,
  beyond bare-symbol placement and `!` operand validation.
- Updated `validator.hpp` `validateProgram` doc comment for AD1: static types are internal only,
  validator returns `void`, produces no artifact, and writes nothing into the AST.
- Verification completed for this implementation:
  - `cmake --build build`
  - `ctest --test-dir build --output-on-failure` (383/383)
  - `cmake --build build-asan`
  - `ctest --test-dir build-asan --output-on-failure` (383/383)
  - `clang-tidy-18 -p build-tidy --checks='-*,cert-*,bugprone-*' --warnings-as-errors='*'`
    `src/analysis/semantic_analyzer/validator.cpp`
    `tests/unit/analysis/semantic_analyzer/validator_test.cpp`
  - `git diff --check`

## Task 7 — Revision for Round 1 finding (2026-11-24)

- **Finding (Low):** `src/analysis/semantic_analyzer/validator.cpp` lines 455 and 477 used
  34-space continuation indentation for the `FunctionContext& context, bool allowBareSymbol`
  parameter line of the `validateExpressionInFunction` forward declaration and definition.
- **Validity:** Confirmed. The style guide requires 4-space indentation, and `git diff HEAD`
  shows both lines were added by Task 7 (not pre-existing).
- **Fix:** Re-indented only those two continuation lines to align with the opening parenthesis
  (40 spaces), matching the parameter-alignment style already used in the same file
  (`collectFunctionScopeNames`, `resolveIdentifierInFunction`, `validateAssignTarget`). Both
  lines are 88 columns. No other content changed.
- **Gates (all passed):**
  - `cmake --build build` — warning-free; `ctest --test-dir build` — 383/383 passed.
  - `cmake --build build-asan`; `ctest --test-dir build-asan` — 383/383 passed.
  - `clang-tidy -p build-tidy --checks='-*,cert-*,bugprone-*' --warnings-as-errors='*'`
    `src/analysis/semantic_analyzer/validator.cpp` — clean.
  - `git diff --check` — clean; no lines over 100 columns in `validator.cpp`.
