---
name: phase-execution
description: Execute the autonomous implementation roadmap one tested, useful, committed phase at a time with deterministic failure handling.
---

# Phase execution skill

## Before a phase

1. Read `AGENTS.md`, this skill, and the phase in `docs/roadmap.md`.
2. Verify repository identity and current branch.
3. Run `git status --short`.
4. Do not destroy pre-existing user changes.
5. Verify all previous phase tests pass.
6. Mark only the current phase `in-progress`.

## During a phase

- Implement only the stated scope plus necessary fixes.
- Keep the artifact usable at phase end.
- Add tests before or with implementation.
- Update documentation as architecture becomes concrete.
- Run focused tests frequently and full phase gates before commit.

## Completion

1. Run all required native tests.
2. Run WASM tests once introduced.
3. Run coverage/sanitizer gates required by the phase.
4. Update `docs/phase-status.md` with exact commands and result.
5. Mark the phase `completed`.
6. Commit with `phase N: <outcome>`.
7. Confirm a clean working tree.
8. Start the next phase automatically.

## Failure

Attempt at most three materially different repairs. Do not count rerunning the same command as a repair. After three unsuccessful approaches:

- save useful diagnostics in `docs/phase-status.md`;
- return the tree to the last successful phase commit without rewriting it;
- keep user-owned pre-existing work intact;
- stop and report the blocker without asking a question.

Do not push, publish, or create CI/CD files.
