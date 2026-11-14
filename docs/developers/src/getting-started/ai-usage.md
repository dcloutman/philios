# AI Usage
Development of the `philios` language is highly AI driven. However, it is **not** vibe coded. While AI usually excels at following guidelines and avoiding common pitfalls, non-deterministic algorithms are known to throw in experiments that are erroneous, hallucinate, be lazy, be undertrained in C++, and to make mistakes in coding. They also will skip important instructions due to limited context. As a result, it's impossible to "Ralph loop" our way to a high-quality, extensible, maintainable, and secure interpreter. Human oversight is mandatory. Key human checkpoints are:
- Writing the initial plan
- Writing code scaffolding and stubs and drafting core functionality as needed
- Reviewing the AI augmented plan before code generation
- Reviewing and revising AI generated source code. This may include manual revisions
- A human peer review of source code before merging

## Quick Start
The steps for implementing a feature are:
- Create feature branch off of main
- Write an initial plan in the `/plans` directory
- Optionally, create code scaffolding, function or class stubs with documenation, or draft implementation to improve AI context
- Run `/planning-loop <exact-plan-stem>` (for example, `/planning-loop 0004`) or select the `planning-loop` custom agent from the VS Code Agent picker. Preserve leading zeroes.
- Answer each `plan-grill-me` question relayed by the `planning-loop` orchestrator
- Before implementation, confirm the latest primary plan, architecture, and security review rounds
  all say **Go**, the review document's overall verdict is **Go**, no blocking questions remain,
  and you explicitly authorize implementation after reviewing the approved plan.
- Only then run `/implementation-loop <exact-plan-stem>` or select the `implementation-loop` custom agent
- Review and revise the generated code as necessary
- Commit your feature branch
- Create a pull request on GitHub
- Address both human and AI-created code comments on your feature branch
- Once a peer approves your pull request, merge into main

## Tickets
Tickets are not yet in use. Work with David Cloutman to add numbered plans to the `/plans` directory for your feature(s). We will shift to using a ticketing system when we begin work on v0.0.3.

## Plans
Write an initial draft for each feature in `plans/` using a zero-padded numeric filename such as
`plans/0004.md`. Treat its filename stem as an opaque string, including all leading zeroes: for
example, `0001` resolves to `plans/0001.md` and `00010` resolves to `plans/00010.md`. Never parse,
strip, or normalize the stem. The planning subagents use the matching `_grill.md` and `_review.md`
files to keep developer decisions and reviewer findings with that plan.

## Planning Loop

### Invocation

In a chat using the **Copilot** harness, run `/planning-loop <exact-plan-stem>` to invoke the planning-loop skill (`.github/skills/planning-loop/SKILL.md`), which forwards the exact filename stem to the `planning-loop` custom agent. For `plans/00010.md`, enter `00010`, not `10`. You can also select the custom agent directly from the VS Code Agent picker. If the custom agent is already selected, the skill uses that agent rather than invoking it recursively. The skill is only an entry point; the custom agent defines and runs the workflow.

The custom agents and skills target the Copilot harness. Add new slash commands as skills in `.github/skills/`, not as `.prompt.md` files: prompt files run only in VS Code's deprecated **Local** harness, which does not load these custom agents.

### Workflow

1. The `planning-loop` orchestrator relays questions from `plan-grill-me` to the developer one at
   a time and stops for each answer. On the next turn, it invokes `plan-grill-me` again with the
   same plan ID, the previous handoff state, and the answer verbatim. After the last question, the
   orchestrator relays a decision summary for confirmation; only after confirmation does the grill
   agent write or append `plans/<plan-id>_grill.md`. The developer makes the decisions; unresolved
   questions remain open.
2. The `plan-composer` revises the draft using the grill record and existing review feedback. It
   then calls `plan-reviewer`, which records the primary review and a Go or No-go verdict in
   `plans/<plan-id>_review.md`.
3. Only after a primary Go, the planning loop calls `plan-reviewer-architecture`. Only after an
   architecture Go does it call `plan-reviewer-security`, which gives the final verdict. These
   reviewers run in sequence and write to their own sections of the same review document.
4. On a No-go, the composer addresses findings and the reviewers repeat their gates in order.
   Review rounds and earlier records are retained. A blocking developer question pauses the loop.
5. The loop ends after the final security Go. The developer reviews the approved plan and must
   explicitly confirm before implementation begins; the planning loop does not implement code.

Every handoff carries the exact plan ID string and expected `plans/<plan-id>.md` path. Each agent
verifies that identity before reading, reviewing, or editing and reports the confirmed ID and path.
The orchestrator stops if any handoff or response refers to a different plan.

## Implementation Loop

In a Copilot harness chat, run `/implementation-loop <exact-plan-stem>` (for example, `/implementation-loop 0004`), preserving leading zeroes. The implementation-loop skill (`.github/skills/implementation-loop/SKILL.md`) forwards the stem and your message to the `implementation-loop` custom agent. You can also select the custom agent from the VS Code Agent picker and supply the stem. Before delegating any implementation work, it checks that:

- The plan is not marked `Blocked on developer`.
- The latest primary, architecture, and security review rounds each say **Go**, and the review
  document's overall verdict is **Go**.
- The plan and latest review rounds have no unresolved blocking questions.
- The developer explicitly authorizes implementation after reviewing the approved plan. A Go
  verdict or a plan ID alone is not authorization.

If any check fails, the loop stops before invoking implementation agents and reports what is needed.

Every implementation handoff carries the exact plan ID string and expected `plans/<plan-id>.md`
path. Each agent verifies that identity before reading, reviewing, or editing and reports the
confirmed ID and path. The orchestrator stops if any handoff or response refers to a different
plan.

1. `implementation-architect` creates plan-scoped architecture stubs, preliminary tests, and an
   architecture decisions record. It follows the approved plan and existing source layout, and
   documents only decisions already approved by the developer.
2. `implementation-generator` implements the approved scope and runs the project's configured
   build and test checks in Ubuntu/WSL Bash. It stops rather than guessing about unresolved
   language or architecture decisions.
3. `implementation-reviewer` and `implementation-security-review` independently review the result
   and record their findings in `plans/<plan-id>-code_review.md` and
   `plans/<plan-id>-security_review.md`, respectively.
4. If either reviewer identifies actionable findings, `implementation-reviser` makes plan-aligned fixes or explains why a suggested change conflicts with the approved plan. If a fix requires an unresolved language or architecture decision, it stops and asks rather than guessing. After any code change, **both** reviewers rerun against the final code; the cycle ends only when both latest review rounds say **GO**. Build and test commands use Ubuntu/WSL Bash.
5. The loop reports its results and stops for human review. A human must review the final code before committing or opening a pull request; the agents do not commit, create a pull request, or merge changes.

Implementation artifacts are stored in `plans/` using
`<plan-id>-<document_name>.md`, where `<plan-id>` is the exact filename stem of the plan and
`<document_name>` is lower case. Never convert the stem to a number or remove leading zeroes. The
standard documents are `<plan-id>-architecture_decisions.md`,
`<plan-id>-implementation_notes.md`, `<plan-id>-code_review.md`, and
`<plan-id>-security_review.md`. Keep earlier review rounds in the same plan-specific report.
