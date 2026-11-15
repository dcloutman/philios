# Agents General Rules

## Planning and Implementation Agents
- `planning-loop` coordinates the examination and revision of plans in the `/plans` directory.
- `implementation-loop` coordinates the implementation of plans in the `/plans` directory by writing `C++` code.
  - Implementors should avoid making calls to other scripting languages such as Node.js, Perl, Ruby, or PHP. Use `bash` commands when possible. Fail-over to Python as a last resort. Do not run Python versions older than `3.11`.
