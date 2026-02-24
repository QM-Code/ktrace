# Config Store Generalization (`m-karma` with `m-bz3` downstream)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (new track; requirements and migration plan forming)`
- Immediate next task: produce a callsite inventory for `Defaults/User/Merged` and `ReplaceUserConfig` usage, then draft a compatibility-preserving API sketch for named layers/views.
- Validation gate: `cd m-overseer && ./agent/scripts/lint-projects.sh`

## Project Overview
This project exists because the current public config API still thinks in a fixed three-layer world: defaults, user, and merged. That shape was useful for a long time, but the ongoing JSON/config refactor is pushing us toward more explicit, composable configuration graphs.

The target direction is not fully settled yet, and that is fine. We are still in the planning phase where the important thing is to make the end-state legible, keep risk bounded, and avoid breaking downstream systems while the API evolves.

## First-Read Orientation (for a new overseer)
1. Today, public API names in `karma/common/config/store.hpp` are specific to one model: `Defaults()`, `User()`, `Merged()`, `ReplaceUserConfig(...)`.
2. We increasingly need named, role-specific config sets (core, overrides, CLI overlays, generated/language merges) without forcing everything through one global "merged" slot.
3. We want to support an arbitrary number of stored serializations, identified by namespace-like names.
4. We need to preserve compatibility while migrating, because many callsites currently assume the old API model.
5. The immediate job is design + inventory, not a blind code rewrite.

## Current State (what is real right now)
- The API is globally scoped and hardcodes conceptual roles.
- Runtime layers exist, but they are add/remove overlays on top of the old model, not a true named-store system.
- Persistence semantics are coupled to the "user" concept (`SaveUser`, `ReplaceUserConfig`).
- Asset lookup behavior depends on merged layer ordering and base-dir tracking.

## Where We Are Going (example target)
The following pseudocode is the current intent sketch from operator planning. It is not final API, but it captures the shape we are aiming for:

```cpp
karma::common::config::AddConfig('server-core', <serialization>); // data/server/config.json
karma::common::config::AddConfig('server-override', <serialization>); // demo/servers/<server>/config.json
karma::common::config::AddConfig('server-command-line', <serialization>); // passed from --config
karma::common::config::AddConfig(
    'server-merged',
    karma::common::config::Merge({'server-core', 'server-override', 'server-command-line'}));

karma::common::config::AddConfig('server-merged-languages', null); // empty store

if (karma::common::config::Get('server-core', 'server.Language')) {
    karma::common::config::Merge(
        'server-merged-languages',
        karma::common::i18n::GetSerialization(
            karma::common::config::Get('server-core', 'server.Language')));
}
if (karma::common::config::Get('server-override', 'server.Language')) {
    karma::common::config::Merge(
        'server-merged-languages',
        karma::common::i18n::GetSerialization(
            karma::common::config::Get('server-override', 'server.Language')));
}
if (karma::common::config::Get('server-command-line', 'server.Language')) {
    karma::common::config::Merge(
        'server-merged-languages',
        karma::common::i18n::GetSerialization(
            karma::common::config::Get('server-command-line', 'server.Language')));
}
```

## Practical Example (how this helps)
Instead of one mutable global merged object, server startup can keep each source explicit:
- `server-core` loaded from `data/server/config.json`
- `server-override` loaded from `demo/servers/<server>/config.json`
- `server-command-line` generated from `--config`
- `server-effective` as the deterministic merge result

That model makes provenance clearer during debugging and lets us inspect or validate each source independently before composing final runtime config.

## Open Design Questions (still ambiguous by design)
- Should merged views be materialized JSON snapshots, lazy views, or both?
- Which named configs are writable/persistent, and what replaces `SaveUser` semantics?
- How should asset resolution select base directories when a view is composed from many layers?
- Should `Get(name, path)` throw, return optional, or support both API styles?
- How much backward-compat wrapper surface should remain while migration is underway?

## Brainstorm Notes from Operator Session
These notes capture where planning stands right now, including open uncertainty:

1. There should be symmetry between exposed `Read*` and `ReadRequired*` helpers. Today public `helpers.hpp` exposes only a narrow set of non-required reads.
2. `validation.hpp` currently models four required types, while helper surface already includes `uint32` reads. This mismatch should be explained or reconciled.
3. `store.hpp` already has file-level awareness (`ConfigFileSpec`, `ConfigLayer`, labels, base dirs), which suggests the core foundation for namespaced stores is largely present.

Concrete mismatch example to preserve:
- `ReadRequiredUInt32Config(...)` is publicly exposed in `karma/common/config/helpers.hpp`.
- `RequiredType` in `karma/common/config/validation.hpp` only exposes `Bool`, `UInt16`, `Float`, `String`.
- `ValidateRequiredKeys(...)` in `common/config/validation.cpp` therefore cannot express preflight validation for a required `uint32` key today.

Additional architecture intent captured from planning:
1. i18n functionality is expected to be rolled into config as part of this generalization. After namespaced/layered config becomes first-class, i18n is treated as one application of JSON layering rather than a separate standalone subsystem.
2. `ConfigLayer` should gain a `mutable` flag:
   - `mutable=false`: layer data cannot be edited in-memory after load (but file reload is still allowed).
   - `mutable=true`: layer data can be edited; if file-backed, mutations should trigger write-back/persist behavior.
3. `ConfigLayer` should track an explicit source file path (or null when file-less), not just a base directory.
   - Rationale: config storage should not mandate `config.json` naming conventions at this abstraction level.
   - Path-level identity also makes reload/save semantics clearer for namespaced stores.

### API Remap Sketch (tentative)
The following old-to-new map is intentionally direct and mechanical to guide first implementation drafts:

- `Initialize(...)` -> scrap and replace with `Merge(...)` style composition entrypoint.
  - Working assumption: `Initialize` mostly performs merge orchestration today, but with a chaotic argument shape.
- `Initialized()` -> replace or remove if no longer needed.
- `Revision()` -> keep in some form because caching mechanics depend on it; may need adaptation.
- `Defaults()`, `User()`, `Merged()` -> replace with `Get(<namespace>)`.
- `AddRuntimeLayer(...)` -> split into:
  - `Add(<namespace>, <serialization>)` for file-less stores.
  - `Load(<namespace>, <filename>)` for file-backed stores.
- `Set()`, `Erase()`, `Tick()` -> keep.
- `UserConfigPath()` -> `Path()` capable of returning the file path for any file-backed serialization.
- `RemoveRuntimeLayer(...)` -> `Remove(<label>)`.
- `LayerByLabel(...)` -> `Get(<label>)`.
- `SaveUser()` -> `Save(<label>)`.

### Listener Requirement (new capability)
Need a way to register "listeners": config layers that watch selected values, update their own internal config when source values change, and persist to their mapped file as needed.

### Private Surface Note
The private section in `store.hpp` has not been fully classified yet. This remains a required design follow-up before finalizing the migration contract.

## Proposed Work Plan
1. **Inventory pass**: map all direct uses of old API concepts (`Defaults/User/Merged/ReplaceUserConfig/SaveUser/AddRuntimeLayer`).
2. **API design pass**: define a minimal named-layer + named-view API that can coexist with legacy wrappers.
3. **Bridge pass**: implement wrappers so old API maps cleanly onto the new internals.
4. **Selective migration pass**: move one startup flow (server first) to named configs; keep behavior identical.
5. **Validation pass**: prove unchanged runtime behavior via traces and required-config checks before wider rollout.

## Boundaries
- This project is about config-store architecture and migration scaffolding.
- It is not an i18n rewrite, and not a full startup-flow rewrite in one pass.
- It must not quietly alter runtime behavior while the API is being restructured.

## Validation Notes
- `lint-projects.sh` must pass for documentation tracking.
- Every migration slice should include trace evidence showing same effective runtime values before/after for a chosen test path.
