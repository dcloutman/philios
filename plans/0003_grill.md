# Grill Record for Plan 0003

## Session 2026-11-20

The `plan-grill-me` agent ran this session through the `planning-loop` orchestrator, with the developer answering. The developer confirmed the final decision summary with the reply "confirmed". The developer also confirmed one addition, the declaration scope rule in D14, which the agent's summary had left out. The agent could not write this record because it lacked a working file-write tool, so the parent session wrote it at the developer's request. The decisions below are taken from the confirmed summary and the developer's answers. Answers given in the developer's own words are quoted exactly.

Twenty questions were asked. Q13 and Q17 repeated earlier questions. The parent session answered them from the developer's earlier answers, without new developer input. Q12 needed a follow-up confirmation.

### Decisions
| ID | Question | Decision | Rationale | Plan section affected |
|----|----------|----------|-----------|-----------------------|
| D1 | Q1: Should plan 0003 add to the language structure of plans 0001 and 0002, or replace parts of it? | Plan 0003 is the source of truth for the behavior it defines. Earlier plans do not limit it. Developer's words: "As per the instructions in the file, plans change. Old plans are not a source of truth. Documentation to some extent and code above all are indicators of current state. Focus on implementing the new plan. Expect change." | Developer choice. The current code and documentation describe the current state. | Additional Information (Old Plans Superceded) |
| D2 | Q2: Which documents take precedence when they conflict? | `AGENTS.md` needs revision. `docs/language_rules.md` is out of date. The documentation linked from `docs/index.md` is the better guide to the current language rules. Developer's words: "The AGENTS.md file needs revision. The language rules document was created to bootstrap plan 0001. It is likely out of date. Refer to the documentation that is linked from `docs/index.md` as a better indicator of current language rules." | Developer choice. | Additional Information; documentation tasks |
| D3 | Q3: Section 4 requires a `const` initializer, but "Additional Information" lets a nullable `const` start as `null`. Which rule applies? | A nullable `const` may omit its initializer. It may then receive exactly one later assignment. D8, D12, and D16 refine this rule. Developer's answer: "2". | Developer choice. | Constants (section 4); Uninitialized nullable variables |
| D4 | Q4: When are type mismatches, assignments to constants, and `!` on a value that is not a `Boolean` detected? | Typing is mandatory. The interpreter detects as much as possible before execution, using declared types, including the declared return types of functions. Checks that cannot be decided before execution are enforced at runtime. Developer's words: "Detect as much as possible at parse time. Enforce what couldn't be caught at runtime. Correct typing is non-optional." and "All functions have return values, you dolt. If `foo()` only returns non-integers, it's detectable. If `bar()` returns a `?Integer`, then you can't use the `!` operator on its results. Obviously, if `foo()` returns an Object, then the runtime will have to detect an error if the Object is anything but an `Integer`." Interpretation: an expression whose static type cannot match the expected type is an error before execution. An expression whose static type is a supertype of the expected type, such as `Object`, is checked at runtime. `!` applied to an expression of static type `?Boolean` is an error before execution. | Reports errors as early as possible. | Additional Information (Error Detection); sections 3, 4, and 6 |
| D5 | Q5 and Q13: What evaluation order is a language rule? | Operands are evaluated strictly left to right. The left operand is fully evaluated before the right operand. If the left operand fails, the right operand is not evaluated. For `name = expr`, the interpreter first checks that the target is assignable, then evaluates the right-hand side, then stores the value. Developer's answer: "1". The developer also supplied the long-term operator precedence table reproduced under "Long-Term Operator Precedence" below. | Predictable side effects and stable diagnostics. Avoids unsequenced evaluation in the C++ evaluator. | Sections 3, 6, 7, and 8 |
| D6 | Q6: Which operators does plan 0003 implement? | Plan 0003 implements only the operators in the plan: `!`, `==`, `!=`, and the assignment statement. The long-term precedence table guides the design so that the parser can be extended to the other operators later. Developer's words: "Implement the operators in the plan document. The table is to help you make the implementation extensible." | Keeps the plan small while preparing for later operators. | Sections 3, 6, 7, and 8; parser design |
| D7 | Q7: Is assignment a statement or an expression? | Assignment is a statement only. It produces no value. Chained assignment, such as `a = b = 1`, is not allowed. An assignment inside an expression, such as `echo(a = 1)`, is a syntax error. The `=` row of the precedence table remains future work. Developer's answer: "1". | Keeps the semantics small and the static checks simple. | Assignment (section 3) |
| D8 | Q8: Does an explicit assignment of `null` count as the one write to a nullable constant? | No. `null` means that a constant is unassigned. Assigning `null` to a nullable constant after its declaration is an error, including when the constant is still `null`. The single write must be a non-null value. Developer's words: "No. Null indicates an unassigned constant. Assigning null to a nullable constant that is already null should result in an error." | Developer choice. | Constants (section 4); Uninitialized nullable variables |
| D9 | Q9: Which words are reserved? | `symbol` and `Boolean` are reserved now. `class` stays reserved. The plan's list of new reserved words is not complete. `Boolean` is a type that extends `Object`. `Symbol` is a type that extends `Object`, and a variable of type `Symbol` can reference a symbol. Developer's answer: "1". Developer's words: "Boolean is a type that extends Object. The reserved word list is not comprehensive. A variable can reference a symbol if it has the Symbol type, which extends Object. `symbol` is a reserved word." | Fewer ambiguous parses and fewer breaking changes later. | Boolean Type (section 0); Symbols (section 5); New reserved words |
| D10 | Q10: How fully is `Boolean` supported in type annotations? | `Boolean` and `?Boolean` are both declarable. The literals are `true` and `false`. Only `?Boolean` accepts `null`. Developer's answer: "1". | Consistent with D9. Gives the strongest static checking. | Boolean Type (section 0); type grammar |
| D11 | Q11 and Q17: How does `==` compare values? | Immutable values, namely `Integer`, `String`, `Boolean`, and `null`, compare by value. Reference-like values, such as functions, objects, future class instances, and symbols, are equal only when they are the same instance. Developer's answer: "1". Developer's words: "If they reference the same point in memory, they are equal, like in Java or most object-oriented languages." | Standard object equality. Avoids deep recursion and cycle handling. | Equality Operator (section 7); Value equality |
| D12 | Q12 and its follow-up: How is a declaration such as `const x: ?T = null` treated? | It is allowed for any nullable type. A declaration-time `= null` does nothing and is the same as omitting the initializer, so the constant is still unassigned. Once a constant holds a non-null value, that value is permanent, and any later assignment is an error. Assigning `null` after the declaration is an error under D8. Developer's words: "For any nullable, it's alllowed. It is the same as not assigning a value. `null` indicates that the constant is unassigned. As a matter of practice, null constants are a bad thing. Developers are better off using symbols in most cases." Follow-up answer: "A, but the simpler way to explain the rule is that once a constant is assigned a non-null value, the value is permanent. So, practically, the assignment of a non-null constant can only happen once. If a value is assigned to a constant with a non-null value, it is an error." and "A null assignment at the declaration can essentially be ignored. It does nothing." | Developer choice. | Constants (section 4); Uninitialized nullable variables |
| D13 | Q14: Where may declarations appear? | `let` declarations are allowed only in block and function scope. Module scope allows only `const`, `symbol`, function, and class declarations. Variable declarations always begin with `let`, and constants always begin with `const`, wherever they are declared. Developer's answer: "1". Developer's words: "Variable declarations begin with `let` wherever they are declared. The same is true of `const` and constants." The developer confirmed the scope rule in the final summary. | Developer choice. Matches the plan's Module Scope note. | Variable Declarations (section 2); Constants (section 4); Module Scope |
| D14 | Q15: What runtime model do symbols have? | `symbol name` declares a unique, immutable symbol value bound to `name`. Symbols compare by identity. Assigning a value to a symbol name is an error. A variable of type `Symbol` can hold a symbol. Developer's answer: "1". Developer's example: "symbol philiosIsRad / let foo: Symbol = philiosIsRad / ... / if foo == philiosIsRad: doSomething()". D18 and D19 restrict where a bare symbol name may appear. | Consistent with D9. Gives precise semantics for checking and tests. | Symbols (section 5) |
| D15 | Q16: What happens when the right-hand side of the one write to a nullable constant is statically nullable? | It is an error before execution. The right-hand side must have a static type that is a non-null `T` or a subtype of `T`. A `?T` expression is rejected even if its runtime value might be non-null. Developer's answer: "1". Developer's words: "A null result will potentially result in a runtime error. I say we forbid it when detected by static analysis, since developers should gate such an assignment with a type check." | Consistent with D4 and D8. | Constants (section 4) |
| D16 | Q18: How are chains such as `a == b == c` parsed? | `==` and `!=` chains are allowed and left-associative. `a == b == c` is parsed as `((a == b) == c)`. For plan 0003, this replaces the "Non-associative" entry for `==` and `!=` in the long-term precedence table. Developer's answer: "2". | Developer choice. | Equality and Inequality Operators (sections 7 and 8) |
| D17 | Q18 clarification: Can a symbol name be assigned directly to `Object`? | No. A declared symbol name can be assigned directly only to a `Symbol`-typed target. A `Symbol` variable that holds the symbol may then widen to `Object`. Developer's words: "If I declare `symbol foo`, I can do `let bar: Symbol = foo`, and then I can to `let baz: Object = bar`, but I cannot legally do `let baz: Object = foo`, because that would be insanity." | Developer choice. | Symbols (section 5) |
| D18 | Q19: Where is a bare symbol name legal? | In plan 0003, a bare symbol name is legal only in an assignment or initializer whose target has type `Symbol`, and as an operand of `==` or `!=`. Any other use is an error before execution. Function arguments, return values, conditionals, and other contexts are future work. Developer's answer: "1". Developer's words: "For now, in assignments to symbol, or comparisons. Later, in conditionals and a variety of other places." | Developer choice. | Symbols (section 5) |
| D19 | Q20: Do all declarations share one namespace per scope? | Yes. Function names, parameters, and `let`, `const`, and `symbol` declarations share one namespace in each scope. A duplicate name in the same scope is an error, whatever kind of declaration it is. Developer's answer: "1". Developer's words: "Yes." | Maximizes static detection and avoids ambiguous lookups. | Variable Declarations (section 2); Symbols (section 5) |
| D20 | Developer clarification after the session, 2026-11-21: are `true` and `false` global symbols? | No. `true` and `false` are reserved literal keywords. They are not global symbols or global state. Developer's words: "Treat `true` and `false` as reserved literal keywords, then." | Avoids a conflict with the rule in `AGENTS.md` against global state. | Boolean Type (section 0); New reserved words |
| D21 | Developer clarification after the session, 2026-11-21: does "A subtype cannot be equal to a supertype" apply? | No. The sentence is removed from the plan. Equality follows D11 only. Developer's words: "Junk the subtype cannot be equal to a supertype rubbish. That's not always true." | The sentence conflicts with D11. | Value equality; Equality Operator (section 7) |

### Long-Term Operator Precedence
The developer supplied this table in answer to Q5. It describes the long-term plan. Plan 0003 implements only `!`, `==`, `!=`, and the assignment statement (D6, D7). D16 replaces the associativity of row 9 for plan 0003.

| Precedence | Operator | Description | Associativity |
| :--- | :--- | :--- | :--- |
| **1 (Highest)** | `new` | Object instantiation | Non-associative |
| **2** | `**` | Exponentiation | Right |
| **3** | `++`, `--` | Increment/decrement | Non-associative |
| **4** | `isa` | Type check | Non-associative |
| **5** | `!` | Logical NOT | Non-associative |
| **6** | `*`, `/`, `%` | Multiplication, division, modulo | Left |
| **7** | `+`, `-`, `.` | Addition, subtraction, string concatenation | Left |
| **8** | `<`, `<=`, `>`, `>=` | Less than, greater than comparisons | Non-associative |
| **9** | `==`, `!=` | Equality, and non-equality | Non-associative |
| **10** | `&&` | Logical AND | Left |
| **11** | `\|\|` | Logical OR | Left |
| **12** | `if <epression> then <value> else <value>` | Ternary conditional | Non-associative  |
| **13** | `=` | Assignment operator | Right |

### Open Decisions from AGENTS.md
| AGENTS.md entry | Decision, or "still open" | Requires an AGENTS.md update? |
|-----------------|---------------------------|-------------------------------|
| Evaluation order of operands and assignment targets | Decided by D5. Operands are evaluated strictly left to right. An assignment checks its target, then evaluates the right-hand side, then stores the value. | Yes |
| Remaining numeric semantics | Still open. Plan 0003 adds no arithmetic (D6). | No |
| Field and map iteration order visible to scripts | Still open. Plan 0003 exposes no iteration. | No |
| Document Precedence section | D1 and D2 change the sources of truth. `docs/language_rules.md` is out of date, and the documentation linked from `docs/index.md` is preferred. | Yes |

### Contradictions Found
- **Constant initializers.** Section 4 requires an initializer, but "Uninitialized nullable variables" lets a nullable constant start as `null`. Resolved by D3, D8, D12, and D15.
- **Error detection timing.** "Error Detection" says every listed error is detectable at parse time, but some depend on runtime values. Resolved by D4.
- **Assignment form.** Section 3 makes assignment a statement, but the precedence table lists `=` as an operator. Resolved by D7.
- **Associativity of `==` and `!=`.** The precedence table marks them non-associative. Resolved by D16, which allows left-associative chains in plan 0003.
- **Symbol values.** Section 5 says a symbol "has no value", but D9 lets a `Symbol` variable reference a symbol. Resolved by D14, D17, and D18.
- **Reserved words.** The plan's list omits `symbol` and `Boolean`. Resolved by D9.
- **Earlier plans.** Plan 0001 made statements at module level syntax errors. Plan 0002 already reserved `class`. Resolved by D1 and D13.
- **Document precedence.** The plan's "Old Plans Superceded" note conflicts with the precedence rules in `AGENTS.md`. Resolved by D1 and D2. `AGENTS.md` needs revision.
- **`true` and `false` as "global symbols".** Section 0 calls `true` and `false` "global symbols". `AGENTS.md` forbids global variables. Resolved by D20.
- **"A subtype cannot be equal to a supertype."** Under D11, a reference-like value is equal to itself even when one operand is statically typed as a supertype, such as `Object`. Resolved by D21, which removes the sentence.

### Open Questions
- None.

### Deferred / Out of Scope
- Operators other than `!`, `==`, `!=`, and the assignment statement: future plans (D6).
- Assignment as an expression and chained assignment: future plan (D7).
- Bare symbol names in arguments, return values, conditionals, and other contexts: future plan (D18).
- Symbols that belong to a class, and class declarations: future plan, after classes are implemented.
- Compound assignment: out of scope, as the plan states.

### Answered From the Repository
- None.

### Recommended Changes for the plan-composer
- Replace "Old Plans Superceded" with the D1 and D2 rules. Add a task to revise the Document Precedence section and the Open Decisions table in `AGENTS.md`, covering D2 and D5.
- Rewrite section 4 to state D3, D8, D12, and D15 as one write-once rule for nullable constants. Keep the initializer required for constants with a non-nullable type.
- Rewrite "Error Detection" using D4. State which checks are made before execution and which at runtime.
- State the D5 evaluation order in sections 3, 7, and 8.
- State in section 3 that assignment is a statement only (D7), with examples that are syntax errors.
- Add `symbol` and `Boolean` to the reserved words, and keep `class` (D9). Specify the `Boolean`, `?Boolean`, `Symbol`, and `?Symbol` type names (D9, D10, D14).
- Rewrite section 5 using D14, D17, and D18, with valid and invalid examples.
- Rewrite section 7 and "Value equality" using D11 and D16.
- State the D13 scope rules and the D19 single namespace rule in sections 2, 4, and 5 and in "Module Scope".
- Record the long-term precedence table as design guidance only, and require a parser structure that can be extended to it (D6).
- Describe `true` and `false` in section 0 as reserved literal keywords, not global symbols (D20).
- Remove the sentence "A subtype cannot be equal to a supertype" from "Value equality" (D21).
- Add tests for each decision, including failure, boundary, and deeply nested input cases.
