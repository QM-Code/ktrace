# Content Mount

## Project Snapshot
- Current owner: `unassigned`
- Status: `handoff-ready (queued for reassignment; shared unblocker slices landed)`
- Immediate next task: harden delta-selection policy with trace-backed tuning and one bounded regression.
- Validation gate: `./scripts/test-server-net.sh <build-dir>` plus manual client/server world-override smoke.

## Mission
Implement the engine-facing content mount abstraction so default shipped content is always available and world-specific packages can override it safely.

This project enables:
- default runtime data from configured data root,
- server-selected world override via `--server-config <world-config.json>`,
- client application of received world package overlays.

## Locked Behavior Contract
1. No `--server-config`: default mode, use shipped/default content only.
2. `--server-config <world-config.json>`: world override mode on top of defaults.
3. Server package behavior:
- server uses world files/config as overrides,
- server sends world package metadata/payload to client.
4. Client package behavior:
- client keeps local defaults,
- client applies received world package as higher-priority overlay.

## Owned Paths
- `m-rewrite/src/engine/common/data_path_resolver.*`
- `m-rewrite/include/karma/common/data_path_resolver.hpp`
- `m-rewrite/include/karma/common/content/archive.hpp`
- `m-rewrite/src/engine/common/content/archive.cpp`
- `m-rewrite/src/game/net/protocol_codec.*`
- `m-rewrite/src/game/server/net/transport_event_source.cpp`
- `m-rewrite/src/game/server/net/transport_event_source/*`
- `m-rewrite/src/game/client/net/connection.hpp`
- `m-rewrite/src/game/client/net/connection/*`
- `m-rewrite/src/game/client/net/world_package/*`

## Interface Boundaries
- Inputs: data-dir selection, server world selection, protocol package metadata.
- Outputs: deterministic content resolution order and safe package apply behavior.
- Coordinate before changing:
  - `m-rewrite/src/game/protos/messages.proto`
  - `m-rewrite/src/game/net/protocol.hpp`

## Current State (Implemented)
1. Data-root override pipeline exists (`--data-dir`, user config `DataDir`, env var).
2. World archive helpers exist (`BuildWorldArchive`, `ExtractWorldArchive`).
3. Server world config runtime layering from filesystem exists.
4. Mount precedence and mount-point matching behavior are implemented.
5. Network world package payload path is wired end-to-end.
6. Join/init path includes package hash metadata and client cache hinting.
7. World identity metadata (`world_id`, `world_revision`) exists with client-side identity validation.
8. Manifest summary and entry metadata are exchanged; cache-hit no-payload init and manifest reuse paths are implemented.
9. Runtime transfer supports `chunked_full` and manifest-driven `chunked_delta` with server-side mode selection and fallback to full payload.
10. Client world-package apply is staged, verified, and atomically promoted (full and delta), with rollback-safe behavior.
11. Client cache uses revisioned package paths plus retention pruning for stale revisions/packages.
12. Targeted failure-path coverage exists (`client_world_package_safety_integration_test`) and is included in `./scripts/test-server-net.sh`.
13. Chunk-transfer retry/resume semantics are implemented for world-package streaming:
- server retries and resumes from first unsent chunk with bounded attempts,
- client accepts compatible transfer restarts and idempotent duplicate chunks,
- interrupted transfer recovery is covered in integration test flow.
14. Chunked transfer integrity is now hard-gated before apply/promotion:
- begin/end transfer metadata hash/content-hash must bind to init metadata,
- streamed full-payload hash is verified at transfer-end before package apply.
15. Targeted regression coverage now rejects tampered transfer-end hash metadata before world package promotion.
16. Client now hard-rejects incompatible or invalid init world metadata before transfer/apply:
- protocol version mismatch,
- missing world identity fields when metadata is present,
- manifest count/hash contract mismatches.

## Current Gaps
1. Delta policy hardening
- delta applicability heuristics are functional but need tuning/telemetry hardening plus one bounded regression coverage addition.
2. Deferred boundary-hygiene candidate tracking
- Deferred extraction candidate from `engine-game-boundary-hygiene` (world/package transfer assembler path in `src/game/client/net/connection/inbound/world_transfer.cpp`) remains intentionally game-owned.
- Revisit via a new scoped follow-up using `docs/archive/engine-game-boundary-hygiene-retired-2026-02-18.md` as baseline context, with protocol-boundary review entry criteria and acceptance gates; do not silently fold this into generic engineization work.

## Execution Plan
1. Harden delta-selection policy with trace-backed tuning and regression tests.
2. Track deferred boundary-hygiene candidate via separately scoped project review only.

## Validation
From `m-rewrite/`:

```bash
./scripts/test-server-net.sh <build-dir>
```

Manual smoke:

```bash
./<build-dir>/bz3-server --data-dir /home/karmak/dev/bz3-rewrite/m-rewrite/data --listen-port 11911 --server-config /home/karmak/dev/bz3-rewrite/m-rewrite/demo/worlds/Default/config.json
./<build-dir>/bz3 --data-dir /home/karmak/dev/bz3-rewrite/m-rewrite/data --server 127.0.0.1:11911 --username tester
```

## Trace Channels
- `config`
- `engine.server`
- `net.server`
- `net.client`

## Risks
- Delta selection may be suboptimal under some content profiles without additional policy tuning.
- Transfer integrity now binds transfer metadata and streamed full-payload hash, but still relies on non-cryptographic FNV identities (no dedicated cryptographic per-chunk token yet).

## Handoff Checklist
- [x] Mount precedence contract preserved.
- [x] Package apply safety constraints enforced.
- [x] Cache/revision behavior validated with trace evidence.
- [x] Tests and docs updated for current protocol/semantic changes.
- [x] Resume/retry semantics implemented and validated.
- [x] Chunk-transfer integrity mismatch is hard-rejected before package apply/promotion.
- [x] Explicit compatibility gating policy implemented.
- [ ] Delta policy tuning validated across representative world-update cases.
