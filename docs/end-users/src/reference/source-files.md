# Language Reference: Source Files

This page describes how the interpreter reads a `main.phil` file. These rules describe the current interpreter.

## File Requirements
- The file must be named `main.phil` and must be in the current working directory.
- The file must be a regular file. The interpreter rejects symbolic links, directories, and device files.
- The file may be at most 2 MiB (2,097,152 bytes).

## Character Encoding
- A source file must be valid UTF-8.
- Outside string literals and comments, only ASCII characters are allowed. A UTF-8 byte order mark (BOM) at the start of the file is an error.
- Strings and comments may contain any Unicode character, with the exceptions below.
- Control characters are errors everywhere, including inside strings and comments. The exceptions are tab, line feed, and carriage return.
- The Unicode bidirectional formatting characters U+202A to U+202E and U+2066 to U+2069 are errors everywhere.

Error messages report line and column numbers. Both start at 1. A column counts bytes, not characters.

## Line Breaks
- A line break ends a statement.
- The interpreter accepts LF, CR LF, and a lone CR as line breaks. A file may mix these styles.
- The last line of a file does not need a line break.

## Statement Continuation
A statement can span several lines. End a line with `;` to continue the statement on the next line.

- The `;` must be the last character on the line. Only whitespace and comments may follow it.
- The indentation of a continuation line has no meaning.
- A `;` anywhere else is an error.
- A continuation line must contain code. A `;` followed by end of file, a blank line, or a comment-only line is an error.

## Comments
- `//` starts a comment that runs to the end of the line. It may follow code on the same line.
- `/*` starts a comment that ends at the next `*/`. This kind of comment may span several lines.
- A comment separates tokens, like a space. For example, `fo/*x*/o` is the two names `fo` and `o`, not the name `foo`.

## Indentation
Indentation defines blocks.

- Each level of indentation is exactly 4 spaces.
- Tabs are not allowed in indentation. Tabs are allowed between tokens.
- A line may be indented by at most one level more than the line before it.
- A dedent must return to the indentation of an enclosing block.
- Blank lines and comment-only lines do not affect indentation.

## Names
A name, or identifier, starts with an ASCII letter or underscore. The remaining characters may be ASCII letters, digits, or underscores. Names are case-sensitive.

## Reserved Words
These 28 words are reserved. A reserved word cannot be used as a function name or parameter name.

| | | | | |
|---|---|---|---|---|
| `function` | `return` | `null` | `pass` | `Integer` |
| `String` | `Function` | `Object` | `ImmutableObject` | `NullObject` |
| `UnsignedInteger` | `Float` | `if` | `elseif` | `else` |
| `class` | `extends` | `implements` | `interface` | `mixin` |
| `with` | `export` | `import` | `not` | `goto` |
| `isa` | `echo` | `typeof` | | |

These words currently have language meaning: `function`, `return`, `null`, `pass`, `Integer`, `String`, `Function`, `Object`, `echo`, and `typeof`.

`not`, `goto`, and `isa` are reserved but have no meaning yet. `ImmutableObject`, `NullObject`, `UnsignedInteger`, and `Float` are also reserved and not valid as declared types.

Reserved words match whole names only, and matching is case-sensitive.

## Literals

### Integer Literals
- A positive integer literal is a sequence of decimal digits, such as `0` or `42`.
- Positive literals may not have a leading zero (`007` is an error).
- The largest positive literal is `9223372036854775807`.
- A negative integer literal is allowed only where an expression starts: after `return`, after `(`, or after `,`.
- In those contexts, `-` must be immediately followed by a digit from `1` to `9`.
- `-0`, `-00`, `-007`, and `-0abc` are lexical errors.
- The smallest literal is `-9223372036854775808`; a larger magnitude is a lexical error.
- A literal cannot be directly followed by a letter or underscore. For example, `12abc` and `-1abc` are errors.
- Every other use of `-` is a syntax error in this version.

### String Literals
- A string literal is enclosed in double quotes, such as `"hello"`.
- A string must end on the line where it starts.
- A string supports exactly these escape sequences:

  | Escape | Meaning |
  |---|---|
  | `\\` | Backslash |
  | `\"` | Double quote |
  | `\n` | Line feed |
  | `\t` | Tab |

- Any other escape sequence is an error.
- `${` inside a string is reserved for future use and is an error.

### The `null` Literal
`null` is the single value of the `null` type. See [Types](types.md).

## Punctuation
The only punctuation characters outside strings and comments are `(`, `)`, `:`, `,`, `;`, `-`, and `?`.

`?` is valid only as part of a nullable type name (`?Integer`, `?String`, `?Function`, or `?Object`). Any other use of `?` is a syntax error.
