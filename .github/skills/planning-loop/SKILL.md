---
name: planning-loop
description: "Start or resume the Philios planning loop for a plan in plans/. Use for /planning-loop, plan review orchestration, or planning a numbered feature."
argument-hint: "Exact plan filename stem, e.g. 0001 or 00010"
user-invocable: true
disable-model-invocation: true
---

# Planning Loop Entry Point

This skill only forwards the request to the `planning-loop` custom agent. The custom agent is the source of truth for the workflow; do not reproduce its process here.

1. Use the supplied plan filename stem exactly as written, including leading zeroes. Do not parse it as a number or normalize it (`0001` means `plans/0001.md`; `00010` means `plans/00010.md`).
2. If the plan stem is missing, ask the developer for it before invoking the agent.
3. If the active agent is already `planning-loop`, continue under its existing instructions; do not invoke it recursively.
4. Otherwise, invoke the `planning-loop` custom agent with the exact `Plan ID` string and corresponding expected path, `plans/<stem>.md`.
5. Return the custom agent's report. If the custom agent cannot be invoked, stop and tell the developer to select it from the VS Code Agent picker. Do not run the planning workflow yourself as a fallback.
