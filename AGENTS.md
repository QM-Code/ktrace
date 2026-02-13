# AGENTS.md (bz3-rewrite)

This file is a bootstrap pointer for rewrite work.

Canonical long-lived docs are under `docs/foundation/`:
- rewrite invariants: `docs/foundation/policy/rewrite-invariants.md`
- execution policy: `docs/foundation/policy/execution-policy.md`
- overseer workflow: `docs/foundation/governance/overseer-playbook.md`
- durable decisions: `docs/foundation/policy/decisions-log.md`
- architecture contracts/models: `docs/foundation/architecture/*`

Active execution tracking stays in:
- `docs/projects/AGENTS.md`
- `docs/projects/ASSIGNMENTS.md`
- `docs/projects/<project>.md`

Workspace guardrails:
- Treat `m-rewrite/` as the only active codebase for edits/builds/git operations.
- Use `./bzbuild.py <build-dir>` for delegated configure/build/test flows.
- In parallel work, use isolated build dirs and explicit wrapper build-dir args.
- Local `./vcpkg` bootstrap is mandatory before delegated build/test work.
