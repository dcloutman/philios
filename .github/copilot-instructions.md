# Copilot Instructions

## Agents are Not Authors
- Agents **are not** co-authors of the code. Do not claim authorship in commit messages or code.
- Do not include agent names or identifiers in commit messages or code.
- Do not reference agents as authors or contributors in any project documentation.
- Do not include agent names or identifiers in project documentation.
- Do not attribute code or documentation to agents in any form.

## Line Endings
- Always use LF (`\n`) for line endings in all generated text.
- Never use CRLF (`\r\n`) line endings.

## English Language Guidelines
- Every English statement has a subject, a verb, and terminates in a period.
- Use lower ASCII characters for text where possible. Do not use smart quotes, em dashes, en dashes, or other non-ASCII punctuation. Use `-` to represent hyphens.
- Minimize the use of abbreviations and acronyms. Spell out terms on first use.
- Never write run-on sentences. Keep sentences clear and concise.
- Do not use a comma (`,`), semicolon (`;`), colon (`:`), or other punctuation to join two independent clauses. Use separate sentences instead.
- Use commas to delineate dependent clauses; not hyphens, dashes, brackets, parentheses, or any other punctuation.
- Minimize the use of parenthetical statements. If an independent sentence, consider letting it stand alone rather than embedding it within parentheses.
- Avoid using colloquial expressions and idioms that may not be universally understood.
- Avoid gamer slang and programmer idioms. Use professional language. Do not use the following terms:
  - "footgun"
  - "spash damage"
- Use proper grammar, punctuation, and spelling.

## Commit Messages
- **ALWAYS** run `git diff --staged` before composing commit messages.
- **NEVER** claim authorship via an attribution line in commit messages or in the resulting code. Agents **are not** co-authors!
- **NEVER** place an email address in commit messages or code. Flag changes that publish email addresses to the user.- Keep commit messages concise.
- Focus on giving a high-level summary of the changes made to the affected files. Avoid including implementation details or unrelated information.
- Limit the subject line to 120 characters
- Separate the subject line from the body with a blank line.
- Write commit messages as a subject-omitted past-tense declarative sentence.
- Use past tense for the content of the commit message **including the subject line**.
- Subject lines must be complete sentences and end with correct punctuation.
