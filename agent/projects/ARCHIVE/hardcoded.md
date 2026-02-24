# Hardcoded Literal Migration (`m-karma` + `m-bz3`)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (close-out; only Part (3) Issue A remains)`
- Immediate next task: implement Part (3) Issue A in `m-karma/src/common/i18n/i18n.cpp` (role-specific required key only; no cross-role fallback).
- Validation gate: `cd m-overseer && ./agent/scripts/lint-projects.sh`

## Scope
- Runtime/source scope: `m-karma/src/*` and `m-bz3/src/*`
- Exclusions:
  - trace logging strings (`KARMA_TRACE`, `KARMA_TRACE_CHANGED`) stay hardcoded.
  - test-only strings/values are not part of this migration track.

## Completion Summary

- Part (1) complete: non-trace feedback strings were migrated/triaged into i18n paths.
- Part (2) complete: non-feedback hardcoded values were migrated to required config keys.
- Part (3) partial:
  - Issue B complete (this session): `m-karma/src/demo/client/runtime.cpp`
  - Issue A remains open: `m-karma/src/common/i18n/i18n.cpp`
- Snapshot artifacts removed from `projects/` on close-out:
  - `m-overseer/agent/projects/hardcoded-feedback-strings.txt`
  - `m-overseer/agent/projects/hardcoded-nonfeedback-values.txt`

## Part 3: Remaining Fallback-Control-Flow Issues

### Issue A (open)
- File: `m-karma/src/common/i18n/i18n.cpp`
- Current behavior: `I18n::loadFromConfig()` attempts `client.Language` and falls back to `server.Language`.
- Target behavior:
  - client runtime requires `client.Language`
  - server runtime requires `server.Language`
  - no cross-role key fallback
  - keep normalization; only normalized-empty result falls back to language code `en`

### Issue B (complete)
- File: `m-karma/src/demo/client/runtime.cpp`
- Final behavior:
  - non-CLI startup requires `client.network.ServerEndpoint`
  - no probing of legacy keys (`network.ServerEndpoint`, `network.DefaultServer`, `network.Server`, `network.ServerHost`)
  - CLI `--server` override behavior unchanged

## Validation
```bash
cd m-overseer
./agent/scripts/lint-projects.sh
```

```bash
# fallback/default callsite guard
rg -n -P "\\b(ReadBoolConfig|ReadUInt16Config|ReadFloatConfig|ReadStringConfig)\\s*\\(" m-karma/src m-bz3/src \
  | rg -v "m-karma/src/common/config/helpers\\.(cpp|hpp)"
```

```bash
# issue-B path guard
rg -n "network\\.DefaultServer|network\\.ServerHost|network\\.Server\\b|ReadStringFallback" \
  m-karma/src/demo/client/runtime.cpp
```

## Current Status
- `2026-02-24`: project takeover complete under overseer ownership.
- `2026-02-24`: Part (3) Issue B completed in `m-karma/src/demo/client/runtime.cpp`.
- `2026-02-24`: inventory `.txt` snapshots removed from `m-overseer/agent/projects/`.
- `2026-02-24`: only Part (3) Issue A remains open.

## Handoff Checklist
- [ ] Part (3) Issue A implemented and validated.
- [x] Part (3) Issue B implemented and validated.
- [x] Part (1) non-trace feedback string migration completed.
- [x] Part (2) non-feedback required-config migration completed.
- [x] Project snapshot inventory `.txt` artifacts removed from `projects/`.
