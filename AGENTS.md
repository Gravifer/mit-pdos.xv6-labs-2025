# Agent Instructions

This file is the repository-wide entrypoint for coding agents.

## Scope and precedence

1. Follow system/developer/user instructions from the active client.
2. Follow repository guidance in `.github/copilot-instructions.md`.
3. If present, also follow `.agent/lab-agent-context.md` for **branch-local, temporary lab guidance**.

`AGENTS.md` must stay branch-portable and should not contain lab-specific implementation details.

## Collaboration defaults

- Prefer minimal, focused changes.
- Keep edits consistent with xv6 style and existing lab structure.
- Validate with the most specific test first, then broader tests.
- Do not introduce cross-lab coupling unless explicitly requested.
- Ask concise clarifying questions when requirements are ambiguous.
- **Do not remove comments** that are not of your task's concern!
- You'll need `make qemu` and run tests inside the vm to confirm changes to be valid; hand those over to the user to do.

## Lab overlay contract

`.github/lab-agent-context.md` is optional and intended for short-term coaching or branch-specific constraints.
When absent, proceed using only the general repository instructions.
