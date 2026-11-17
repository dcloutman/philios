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
- The Unicode bidirectional formatting characters U+202A to U+202E and U+2066 to U+2069 are errors everywhere. This rule prevents source code that displays differently from how it runs.

Error messages report line and column numbers. Both start at 1. A column counts bytes, not characters, so a multi-byte character advances the column by more than 1.

## Line Breaks
- A line break ends a statement.
- The interpreter accepts LF, CR LF, and a lone CR as line breaks. A file may mix these styles.
- The last line of a file does not need a line break.

## Statement Continuation
A statement can span several lines. End a line with `;` to continue the statement on the next line.

```philios
function main (): Integer:
    return pick(1,;
        2)

function pick (first: Integer, second: Integer): Integer:
    return second
```

- The `;` must be the last character on the line. Only whitespace and comments may follow it.
- The indentation of a continuation line has no meaning.
- A `;` anywhere else is an error. A `;` does not end a statement as it does in C or JavaScript.
- A continuation line must contain code. A `;` followed by the end of the file, a blank line, or a comment-only line is an error.

## Comments
- `//` starts a comment that runs to the end of the line. It may follow code on the same line.
- `/*` starts a comment that ends at the next `*/`. This kind of comment may span several lines.
- When a `/* */` comment spans several lines, only whitespace and comments may follow the closing `*/` on its last line.
- A comment separates tokens, like a space. For example, `fo/*x*/o` is the two names `fo` and `o`, not the name `foo`.
- Comment markers inside a string literal are part of the string.

```philios
/* This program
   returns 1. */
function main (): Integer:
    return 1  // The exit status.
```

## Indentation
Indentation defines blocks. A function body is a block.

- Each level of indentation is exactly 4 spaces.
- Tabs are not allowed in indentation. Tabs are allowed between tokens.
- A line may be indented by at most one level more than the line before it. Indentation must open only where a block starts, such as after a function header.
- A dedent must return to the indentation of an enclosing block.
- Blank lines and comment-only lines do not affect indentation.
- Indentation is measured before the first comment or token on a line. Spaces after a leading comment are not indentation.

## Names
A name, or identifier, starts with an ASCII letter or an underscore. The remaining characters may be ASCII letters, digits, or underscores. Names are case-sensitive, so `total` and `Total` are different names.

## Reserved Words
These 23 words are reserved. A reserved word cannot be used as a function name or a parameter name.

| | | | | |
|---|---|---|---|---|
| `function` | `return` | `null` | `pass` | `Integer` |
| `String` | `Function` | `Object` | `ImmutableObject` | `NullObject` |
| `UnsignedInteger` | `Float` | `if` | `elseif` | `else` |
| `class` | `extends` | `implements` | `interface` | `mixin` |
| `with` | `export` | `import` | | |

Only `function`, `return`, `null`, `pass`, `Integer`, `String`, and `Function` currently have a meaning. The others are reserved for future features.

Reserved words match whole names only, and the match is case-sensitive. For example, `integer`, `Integer2`, and `_if` are ordinary names.

## Literals

### Integer Literals
- An integer literal is a sequence of decimal digits, such as `0` or `42`.
- A literal may not have a leading zero. For example, `007` and `00` are errors. The literal `0` is allowed.
- The largest literal is `9223372036854775807`. A larger literal is an error.
- Negative literals are not supported. The `-` character is an error.
- A literal cannot be directly followed by a letter or underscore. For example, `12abc` is an error.

### String Literals
- A string literal is enclosed in double quotes, such as `"hello"`.
- A string must end on the line where it starts. A line break inside a string is an error.
- A string supports exactly these escape sequences:

  | Escape | Meaning |
  |---|---|
  | `\\` | Backslash |
  | `\"` | Double quote |
  | `\n` | Line feed |
  | `\t` | Tab |

- Any other escape sequence is an error.
- The sequence `${` inside a string is reserved for future use and is an error.

### The `null` Literal
`null` is the single value of the `null` type. See [Types](types.md).

## Other Characters
The only punctuation characters are `(`, `)`, `:`, and `,`, plus `;` for continuation. Any other character outside a string or comment is an error.
