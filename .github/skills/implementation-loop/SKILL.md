---
name: implementation-loop
description: "Start the Philios Implementation Loop for an approved, developer-authorized plan in plans/. Use for /implementation-loop."
argument-hint: "Exact plan filename stem, e.g. 0001 or 00010"
user-invocable: true
disable-model-invocation: true
---

# Implementation Loop Entry Point

This skill only forwards the request to the `implementation-loop` custom agent. The custom agent is the source of truth for the workflow, including its approval and authorization preflight; do not reproduce its process here.

1. Use the supplied plan filename stem exactly as written, including leading zeroes. Do not parse it as a number or normalize it (`0001` means `plans/0001.md`; `00010` means `plans/00010.md`).
2. If the plan stem is missing, ask the developer for it before invoking the agent.
3. If the active agent is already `implementation-loop`, continue under its existing instructions; do not invoke it recursively.
4. Otherwise, invoke the `implementation-loop` custom agent with the exact `Plan ID` string, the corresponding expected path `plans/<stem>.md`, and the developer's message verbatim, so the agent can judge whether the developer has explicitly authorized implementation. Do not state or imply authorization on the developer's behalf.
5. Return the custom agent's report. If the custom agent cannot be invoked, stop and tell the developer to select it from the VS Code Agent picker. Do not run the implementation workflow yourself as a fallback.
