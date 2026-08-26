# AGENTS.md — iox-cpp autonomous agent control

## Repository Identity

- **Name:** iox-cpp-codex
- **Purpose:** INTERLIS XTF 2.3/2.4 Reader/Writer Framework (Native C++17 + WebAssembly)
- **Spec:** `iox-cpp-llm-coding-spec.md`

## Agent Instructions

This file is the root agent control document for `iox-cpp`. Autonomous
coding agents must read this file first, then proceed phase by phase.

### Phase Order

Execute Phase 0 through Phase 11 sequentially. Do not skip or reorder phases.
Every phase produces a useful, tested artifact and its own Git commit.

### Critical Rules

1. Never destroy pre-existing user work.
2. Do not push or publish from an autonomous coding session. CI/CD definitions
   under `.github/workflows/` are part of the repository release contract and
   may be created or updated when the user explicitly requests release
   automation.
3. Read the relevant skill before each phase.
4. Never disable or weaken a valid test to complete a phase.
5. After three failed repair attempts, restore to last good commit and stop.
6. Commit messages use exact phase-oriented format: `phase N: <description>`.

### Pre-Flight (every session)

```sh
    git rev-parse --show-toplevel   # must identify this checkout
git branch --show-current
git status --short
```

### Quality Gates (every phase)

- All relevant CTest tests pass
- WASM tests pass once introduced
- No regular test requires Java or network
- Public API documented and tested
- `docs/phase-status.md` updated with exact commands and results
- Clean working tree after commit

## Skills

Explicitly read the relevant skill before each phase:

- `.agents/skills/architecture/SKILL.md`
- `.agents/skills/native-build/SKILL.md`
- `.agents/skills/wasm-build/SKILL.md`
- `.agents/skills/xtf-conformance/SKILL.md`
- `.agents/skills/testing/SKILL.md`
- `.agents/skills/phase-execution/SKILL.md`

## Release automation exception

GitHub is the canonical repository and `main` is the source of truth. GitHub
Actions run from this repository.
The workflows `ci.yml` and `publish-iox.yml` publish only the explicitly
documented Source/WASM artifacts. Native builds remain mandatory CI gates.

## Module Boundaries

```
iox-core  → no XML, Expat, XTF, JSON, or ilic dependency
iox-xtf   → no ilic-core dependency
iox-ilic  → links directly to concrete ilic-core types
```

No abstract model-provider framework. No dynamic plugins. No ITF.

## Key Documents

- `docs/architecture.md` — module design and invariants
- `docs/roadmap.md` — phase plan and status
- `docs/conformance.md` — pinned references and deviations
- `docs/phase-status.md` — per-phase build/test results
- `iox-cpp-llm-coding-spec.md` — authoritative specification
