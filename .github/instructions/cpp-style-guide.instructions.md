---
applyTo: "**/*.{cpp,h,hpp,cc}"
---
# C++ Coding Standards

Standard: **C++20**. This guide governs how C++ is written. Architecture, language semantics, and
open decisions are defined in [AGENTS.md](../../AGENTS.md), which takes precedence for those topics.
Priorities: security, then industry-standard practice, then performance. Enforce mechanical
formatting with `.clang-format` where configured.

All code must also comply with the [SEI CERT C++ Coding Standard](https://cmu-sei.github.io/secure-coding-standards/sei-cert-cpp-coding-standard/),
including the CERT C rules it lists as applying to C++. This guide adds project-specific rules
and restates the CERT rules most relevant to an interpreter; CERT applies to everything else. A
rule here that is stricter than CERT wins. Deviations from CERT are allowed only where this guide
names them explicitly.

## Naming & Layout
- PascalCase for classes, structs, enums, and type aliases; camelCase for functions, variables,
  and parameters; PascalCase for `enum class` enumerators.
- `kPascalCase` for named constants. Ordinary `const` locals and parameters stay camelCase.
- Trailing underscore for non-public data members (e.g. `scope_`). Public members of plain
  aggregate structs have no suffix.
- `snake_case` file names. Use `.hpp` and `.cpp` only; do not add `.h` or `.cc` files.
- 4-space indentation, same-line opening braces, 100-column limit. Always brace control-flow
  bodies, including single statements.
- `#pragma once` in every header. Include what you use. Include order: matching header, project
  headers, then standard library headers, each group sorted alphabetically. The project has one
  include root, `src/`: include project headers by their full path from it, in double quotes (for
  example `#include "frontend/lexer/token.hpp"`), and never use relative `..` paths. Use angle
  brackets only for standard library and third-party headers.
- Project code lives in namespace `philios` (or sub-namespaces); `main` is the only exception.
  File-local helpers in `.cpp` files go in an anonymous namespace; never use an anonymous
  namespace in a header (CERT DCL59). Headers define only inline, `constexpr`, or template
  functions. Never use `using namespace std;`.
- Never declare reserved identifiers: names beginning with an underscore and a capital letter,
  or containing a double underscore, anywhere, including macros (CERT DCL51). Never add
  declarations to `namespace std`; specializing `std::hash` and similar templates for project
  types is allowed (CERT DCL58).
- No mutable global or static state, including singletons. Namespace-scope, class `static`, and
  function-local `static` data must be immutable and constant-initialized (`constexpr` or
  `constinit const`); no dynamically initialized static objects.
- The single exception is an interrupt flag for Ctrl-C and other signals (CERT MSC54): one
  `constinit` variable of type `volatile std::sig_atomic_t` or lock-free `std::atomic<bool>`.
  The signal handler is a plain function that only sets the flag. The execution context polls it
  and handles cancellation outside the handler.

## Ownership & Lifetime
- Prefer values and automatic storage. Own dynamic memory with standard containers or smart
  pointers; never call `new`/`delete` directly, and never let a raw pointer own memory.
- Default to `std::unique_ptr`; use `std::shared_ptr` only for genuinely shared ownership. Create
  them with `std::make_unique`/`std::make_shared`.
- Never construct a smart pointer from a raw pointer that is already owned, such as one from
  `.get()`, `this`, or another smart pointer. It creates a second owner and a double free (CERT
  MEM56). Copy the existing smart pointer, or use `shared_from_this()`.
- Borrow with references when non-null and raw pointers only when nullable. A borrowed object must
  be kept alive by an owner for the whole time it is used.
- Use RAII for every resource (files, handles, buffers), not just memory.
- Destruction of linked structures (AST chains, environment chains, nested containers) must not
  recurse once per node. Write an iterative destructor that detaches children into a local
  worklist, so freeing a deeply nested structure cannot overflow the native stack.
- Do not weaken an owning edge to `std::weak_ptr` merely to break a cycle; cycle reclamation is an
  open decision in AGENTS.md. Use `std::weak_ptr` only for genuinely non-owning links, acquire it
  with `lock()`, and handle expiry. `expired()` alone does not keep the object alive.
- Do not rely on a moved-from object's value; only assign to it, destroy it, or use operations its
  type documents as valid after a move.
- Use `std::forward` only with forwarding references.
- A lambda must not outlive anything it captures by reference (CERT EXP61). Use `[&]` only for
  lambdas called before the enclosing scope ends. Lambdas that are stored, returned, or registered
  as callbacks or built-ins capture explicitly, by value or by owning smart pointer.

## Reentrancy (Script Code Running During a C++ Call)
Any call that can execute Philios code (`evaluate`, method and function calls, operator dispatch,
callbacks) may reassign variables, mutate objects, or release the last reference to an object.
Because there is no global state, such a function must take the execution context as an explicit
parameter. Treat every function with that parameter, and every caller of one, as reentrant.
- Never hold a reference, pointer, iterator, `std::string_view`, or `std::span` into a mutable
  runtime container (instance fields, environment tables, collections) across a reentrant call.
  Copy what you need, or re-look it up after the call.
- Keep the receiver, callee, and arguments alive for the entire reentrant call by holding owning
  `std::shared_ptr` copies. Use `shared_from_this()` when a member function needs to keep `this`
  alive. A `const std::shared_ptr&` parameter does not keep the object alive.
- Outside reentrant calls, pass `const std::shared_ptr<T>&` or `T&` to avoid reference-count
  traffic. Pass `std::shared_ptr<T>` by value only when the callee stores it or makes a reentrant
  call.
- When C++ code iterates over a runtime collection and runs script code for each element, it must
  not use C++ iterators across those calls. Use an index or a snapshot, and re-check bounds after
  every reentrant call. How mutation during iteration behaves for scripts is a language decision.
- An immutable object's contents can be shared without deep copies, but its lifetime still is not
  guaranteed. Hold an owning reference across reentrant calls exactly as for mutable objects.
  Immutable objects must not hold owning references to mutable objects.
- `shared_from_this()` requires the class to derive from `std::enable_shared_from_this` and the
  object to already be owned by a `std::shared_ptr`. Never call it from a constructor.
- C++ leaves the evaluation order of function arguments and of most operands unspecified (CERT
  EXP50). Never evaluate two script subexpressions within one C++ expression, such as
  `call(eval(a), eval(b))` or `eval(a) + eval(b)`. Evaluate each into a named local, in separate
  statements, in the order the language defines.
- Never pass a script-supplied comparator, hash, or predicate directly to a standard algorithm or
  container (`std::sort`, `std::set`, `std::unordered_map`, ...). Those require a consistent
  ordering and a predicate with no side effects; a script cannot guarantee either, and breaking
  them is undefined behavior (CERT CTR57, CTR58). Sort or search a snapshot with an algorithm
  that stays memory-safe for any comparator result, and treat every comparator call as
  reentrant.

## Types & Data
- Each built-in object type's storage representation is private. Only that class may read or
  depend on it; everything else goes through the class interface.
- `std::string` for owned text; `std::string_view` for borrowed text. A view must not outlive its
  buffer or survive mutation or reallocation of it.
- `std::vector` for dynamic arrays, `std::array` for fixed-size arrays, `std::span` for borrowed
  contiguous ranges. Convert C arrays and pointer/size pairs at external API boundaries only.
- Validate every index, length, and slice before access; `operator[]` on `span`, `string_view`,
  `vector`, and `string` is unchecked. Never form a pointer or iterator beyond one-past-the-end.
- `enum class` instead of plain `enum`. `nullptr`, never `NULL` or `0`.
- `std::optional` for expected absence, not errors. A missing identifier is a language error.
- Access `std::variant` with `std::visit` or `std::get_if`, not `std::get` inside try/catch.
- Initialize every variable at its declaration and every member with a default member initializer
  or in the constructor initializer list.
- Use `auto` where the type is obvious or verbose (iterators, `make_*` results); spell out types
  where they carry meaning.

## Integers, Floats & Conversions
These rules prevent undefined behavior in C++. They do not define Philios numeric semantics; see
AGENTS.md before implementing any language-level numeric operation.
- Signed integer overflow is undefined behavior in C++. Philios integers wrap, so implement signed
  `+`, `-`, `*`, and negation in centralized helpers: convert to the matching unsigned type,
  operate there (unsigned wrapping is defined), and convert back (well-defined in C++20). Never
  write a signed expression that can overflow, and do not use compiler-specific builtins.
- Check for division or remainder by zero and for `min / -1` and `min % -1` before dividing. Check
  shift counts are non-negative and less than the bit width before shifting.
- Never compare or combine signed and unsigned values implicitly. Use `std::cmp_less`,
  `std::cmp_equal`, `std::in_range` (`<utility>`), or explicitly checked conversions.
- Before converting a floating-point value to an integer, check that it is finite and in range.
  Beware: `static_cast<double>(INT64_MAX)` rounds up to 2^63, so write the upper bound as
  `value < 9223372036854775808.0`, not `value <= INT64_MAX`.
- Parse and format numbers with `std::from_chars`/`std::to_chars`. Require full consumption of the
  input and handle `std::errc::result_out_of_range`. Do not use `std::stoi`, `std::stod`, `atoi`,
  or `strtol` family functions, which are locale-dependent, throw, or fail silently.
- Use checked arithmetic for sizes, lengths, indexes, offsets, and allocation requests, and verify
  any narrowing conversion with `std::in_range` first.
- Pass only `EOF` or values representable as `unsigned char` to `<cctype>` functions.
- Range-check an integer before converting it to an enumeration type, for example when decoding
  token kinds or opcodes (CERT INT50).

## Casts
- Use `static_cast` for intentional conversions after their preconditions are checked. Never use
  `static_cast` to downcast a runtime object unless its dynamic type has already been verified.
- Prefer virtual dispatch. RTTI is enabled; use `dynamic_cast` when a downcast is required and
  the type is not otherwise verified.
- Use `std::bit_cast` only to reinterpret the representation of equal-size, trivially copyable
  types. It is not a numeric conversion or a substitute for validating input bytes.
- Do not type-pun through pointers or references. Any `reinterpret_cast` needs a comment
  justifying alignment, lifetime, and aliasing.
- No C-style casts or functional-style casts on non-class types. Avoid `const_cast`.
- Do not use `memcpy`/`memmove` on objects; use assignment or standard algorithms.

## Functions & Classes
- Be const-correct. A `const` pointer or smart pointer does not make its pointee immutable.
- Mark overrides with `override` only (not also `virtual`). Mark classes `final` unless they are
  designed for inheritance.
- A polymorphic base deleted through a base pointer (including through `std::unique_ptr<Base>`)
  needs a public virtual destructor; otherwise make its destructor protected and non-virtual.
- Never call a virtual function from a constructor or destructor; it does not dispatch to the
  derived class (CERT OOP50).
- Never copy or pass a polymorphic object by value, which slices it (CERT OOP51). Delete the
  copy and move operations of polymorphic bases such as `ASTNode` and `Object`; use references
  or smart pointers, and an explicit virtual `clone()` if copying is genuinely needed.
- Follow the Rule of Zero. If you declare any copy/move operation or the destructor, explicitly
  `= default`, `= delete`, or implement all five. Hand-written assignment operators must handle
  self-assignment (CERT OOP54), and copying must not modify the source object (CERT OOP58).
- Declare moves and swaps `noexcept` when they cannot throw; an exception escaping a `noexcept`
  function terminates the program. Destructors must never throw.
- `[[nodiscard]]` on functions whose result must be used, including those returning
  `std::optional`, error states, or newly created resources. `[[maybe_unused]]` instead of
  casting to `void`.
- Use `constexpr` functions and constants instead of macros. Macros are allowed only for platform
  and feature detection.
- Use `std::exchange` to replace a value and return the old one; use `std::swap` to swap.

## Errors & Diagnostics
- Report language errors with exceptions derived from a single Philios error base that carries
  the source location. Keep internal invariant failures, host I/O failures, and resource
  exhaustion as distinct types; never present an interpreter bug as a script error.
- Copying an exception object must not throw (CERT ERR60). Derive error types from a standard
  exception such as `std::runtime_error`, or store their message in a
  `std::shared_ptr<const std::string>`, not a plain `std::string` member.
- Check internal invariants in every build by throwing the internal-error type. Do not use
  `assert`, which disappears from release builds, or `std::abort`, `std::exit`, and
  `std::terminate`, which skip cleanup (CERT ERR50). Validate all script and file input in every
  build.
- `main` must catch every exception (CERT ERR51): the Philios error base, the internal-error
  type, `std::bad_alloc` (which a script can trigger by allocating too much, CERT MEM52), other
  `std::exception`s, and finally `catch (...)`. Report each one and exit with a defined non-zero
  status.
- Catch by `const&`, never by value or pointer (CERT ERR61). Order handlers from most derived to
  least derived (CERT ERR54).
- Catch only where an error can be handled, enriched with context, or reported at the CLI
  boundary. Never silently swallow an exception or return a fake success value.
- On failure, leave objects in a valid state and release resources. A module that fails to load
  must not be visible as loaded.
- Escape control characters and terminal escape sequences when echoing source text or script
  strings in diagnostics. Never pass script-provided text as a format string.

## Performance
- Correctness and the safety rules above take precedence over micro-optimizations.
- Avoid copying strings, containers, and `shared_ptr`s in hot paths, except where the reentrancy
  rules require an owning copy.
- Use `[[likely]]`/`[[unlikely]]` only when profiling justifies it.