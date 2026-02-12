# Engine Network Foundation

## Project Snapshot
- Current owner: `unassigned`
- Status: `completed (slices 1-29 accepted; closeout accepted)`
- Immediate next task: none; reopen only for concrete transport contract regressions requiring a new bounded slice.
- Validation gate: `./scripts/test-server-net.sh <assigned-build-dir>` must pass after `./bzbuild.py -c` in both assigned build dirs.

## Mission
Move reusable networking boilerplate into engine-owned contracts so game code stops owning transport/session plumbing.

## Why This Is Separate
Network protocol semantics and gameplay rules can continue evolving in game tracks, while engine-level transport/session defaults are developed once and reused.

## Owned Paths
- `m-rewrite/include/karma/network/*`
- `m-rewrite/src/engine/network/*`
- integration touchpoints in:
  - `m-rewrite/src/game/client/net/*`
  - `m-rewrite/src/game/server/net/*`
- `docs/projects/engine-network-foundation.md`

## Interface Boundaries
- Engine owns:
  - transport abstraction,
  - connection/session lifecycle defaults,
  - retry/timeout/pump policy hooks,
  - backend wiring/factory selection.
- Game owns:
  - message schema and protocol payload semantics,
  - gameplay authority/state rules,
  - game-specific event interpretation.
- Coordinate before changing:
  - `m-rewrite/src/game/net/protocol.hpp`
  - `m-rewrite/src/game/net/protocol_codec.cpp`
  - `m-rewrite/src/game/protos/messages.proto`
  - `docs/projects/gameplay-netcode.md`

Current first-slice boundary (`2026-02-10`):
- Engine now owns ENet server transport/session lifecycle (`create/listen/poll/send/disconnect`) behind `karma::network::ServerTransport`.
- Game server code (`src/game/server/net/enet_event_source.cpp`) still owns protocol decode/encode usage and gameplay event interpretation.
- No protocol schema/payload semantics were moved from game to engine.

Current second-slice boundary (`2026-02-10`):
- Engine now owns ENet client transport/session lifecycle (`create/connect/poll/send/disconnect`) behind `karma::network::ClientTransport`.
- Game client code (`src/game/client/net/client_connection.cpp`) still owns protocol decode/encode usage and gameplay event interpretation.
- No protocol schema/payload semantics were moved from game to engine.

Current third-slice boundary (`2026-02-10`):
- Engine now owns bounded reconnect/backoff policy in `karma::network::ClientTransport` with config-driven reconnect attempts/backoff/timeout controls.
- Game client code consumes reconnect settings and replays join bootstrap on transport reconnect while keeping protocol payload semantics unchanged.
- No protocol schema/payload semantics were moved from game to engine.

Current fourth-slice boundary (`2026-02-10`):
- Engine now exposes transport registration hooks (`RegisterClientTransportFactory`, `RegisterServerTransportFactory`) and resolves transports by backend ID with ENet as the default registration.
- Engine transport configs now carry explicit `backend_name` for extensibility while preserving `auto|enet` behavior.
- Game client transport consumption now passes backend ID (`network.ClientTransportBackend`) through to engine transport creation without changing protocol payload semantics.
- No protocol schema/payload semantics were moved from game to engine.

Current fifth-slice boundary (`2026-02-10`):
- Engine now applies a shared transport-pump event normalizer across client/server poll paths (`Connected -> Received -> Disconnected`) per pump cycle.
- Client reconnect-generated lifecycle events are normalized through the same ordering path.
- Disconnect events are emitted terminally in cycle order after payload events.
- No protocol schema/payload semantics were moved from game to engine.

Current sixth-slice boundary (`2026-02-10`):
- Engine now has explicit client/server transport contract tests that lock normalized pump ordering (`Connected -> Received -> Disconnected`) and disconnect-terminal semantics.
- Client transport contract tests also lock reconnect-cycle ordering behavior (reconnect `Connected` lifecycle event emitted before payload events when staged in the same pump cycle).
- Server-net validation wrapper now builds/runs these transport contract tests as part of the required engine-network foundation gate.
- No protocol schema/payload semantics were moved from game to engine.

Current seventh-slice boundary (`2026-02-10`):
- Server transport consumption now mirrors client backend-ID plumbing by explicitly reading and passing `network.ServerTransportBackend` via `ServerTransportConfig.backend_name`.
- Known built-ins (`auto|enet`) still map through enum parsing, while custom backend IDs can now flow through to engine transport registration resolution.
- Server transport creation failure logging now reports the configured backend ID string for parity with client-side observability.
- No protocol schema/payload semantics were moved from game to engine.

Current eighth-slice boundary (`2026-02-10`):
- Server-net contract coverage now explicitly validates server backend-ID config plumbing:
  - built-in `auto` and `enet` selection behavior,
  - custom backend-ID pass-through behavior,
  - unregistered backend-ID failure behavior.
- Coverage uses registered fake server transport factories to assert server config handoff deterministically.
- No protocol schema/payload semantics were moved from game to engine.

Current ninth-slice boundary (`2026-02-10`):
- Engine transport creation now emits warning-level observability for unregistered transport backend IDs with client/server parity:
  - client warning: `unregistered client transport backend='...'`,
  - server warning: `unregistered server transport backend='...'`.
- Failure semantics are preserved: unregistered backend IDs still fail transport creation (`nullptr`).
- Client/server transport contract tests now explicitly assert both failure behavior and warning emission for unregistered backend IDs.
- No protocol schema/payload semantics were moved from game to engine.

Current tenth-slice boundary (`2026-02-10`):
- Shared transport-pump normalization now supports per-key normalization (`NormalizePumpEventsPerKey`) and is applied in server transport poll flow keyed by peer token.
- Per-peer lifecycle-edge ordering semantics are explicitly locked in transport contract tests:
  - per-peer deterministic ordering (`Connected -> Received -> Disconnected`),
  - disconnect-terminal behavior preserved per peer,
  - cross-peer group ordering follows first appearance in pump cycle.
- Client transport poll normalization now consistently routes through the same per-key normalization path (single-key parity path).
- No protocol schema/payload semantics were moved from game to engine.

Current eleventh-slice boundary (`2026-02-10`):
- Added high-volume multi-peer transport-ordering stress coverage in `server_transport_contract_test` using a large deterministic interleaved staged-event set.
- Coverage locks:
  - per-peer normalized ordering (`Connected -> Received... -> Disconnected`),
  - disconnect-terminal semantics per peer under high event volume,
  - current contiguous per-peer grouping semantics ordered by first peer appearance.
- Scope is contract-test-only; transport implementation behavior and protocol/schema remain unchanged.

Current twelfth-slice boundary (`2026-02-10`):
- Added live integration-level loopback multi-peer ordering stress coverage in `server_transport_contract_test` (`TestLiveLoopbackMultiPeerOrderingStress`).
- Coverage locks, under runtime poll cycles with burst sends:
  - per-peer contiguous block ordering with monotonic phase progression (`Connected -> Received -> Disconnected`),
  - terminal disconnect semantics per peer,
  - full bounded per-peer receive-burst delivery assertions.
- Scope is coverage-only; transport runtime behavior and protocol/schema remain unchanged.

Current thirteenth-slice boundary (`2026-02-10`):
- Added live integration-level reconnect/payload-interleave stress coverage in `client_transport_contract_test` (`TestLiveReconnectPayloadInterleaveStress`).
- Coverage locks, under runtime reconnect cycles with concurrent server pumping:
  - reconnect-cycle ordering contract (`Connected` emitted before `Received` for each cycle),
  - bounded post-reconnect payload-burst delivery assertions,
  - no terminal `Disconnected` emission during active reconnect-cycle recovery.
- Scope is coverage-only; transport runtime behavior and protocol/schema remain unchanged.

Current fourteenth-slice boundary (`2026-02-10`):
- Added live integration-level timeout/disconnect edge-race coverage in `client_transport_contract_test` (`TestLiveTimeoutDisconnectRaceTerminalOrdering`).
- Coverage locks, under runtime timeout/drop/reconnect race scenarios:
  - reconnect recovery ordering before terminal disconnect (no premature terminal `Disconnected`),
  - single terminal `Disconnected` emission when reconnect attempts are exhausted,
  - no `Connected`/`Received` events after terminal disconnect.
- Scope is coverage-only; transport runtime behavior and protocol/schema remain unchanged.

Current fifteenth-slice boundary (`2026-02-10`):
- Added live integration-level multi-peer fairness-under-churn coverage in `server_transport_contract_test` (`TestLiveLoopbackMultiPeerFairnessUnderReconnectChurn`).
- Coverage locks, under runtime reconnect/disconnect churn with burst payloads:
  - per-peer contiguous/monotonic lifecycle ordering invariants remain intact during churn,
  - bounded fairness expectation for received-event attribution across active peers/rounds,
  - token-reuse restart handling is explicitly allowed only in churn-specific validation mode.
- Scope is coverage-only; transport runtime behavior and protocol/schema remain unchanged.

Current sixteenth-slice boundary (`2026-02-11`):
- Hardened live loopback churn/fairness and ordering stress coverage against timing/port-contention flake in `server_transport_contract_test`:
  - widened loopback server endpoint scan/retry strategy with rotated start offsets,
  - added loopback client endpoint creation retries,
  - added active-client settle gating and widened timing budgets in live churn/fairness paths.
- Scope is coverage-only; transport runtime behavior and protocol/schema remain unchanged.

Current seventeenth-slice boundary (`2026-02-11`):
- Made loopback live-stress endpoint allocation deterministic across client/server helpers used by transport contract tests:
  - client loopback server endpoint allocation now uses deterministic multi-pass sequential scan/retry,
  - server loopback transport allocation now uses deterministic multi-pass sequential scan/retry (time-based offset removed).
- Preserved accepted ordering/disconnect contracts and protocol/schema boundaries.
- Scope remains network foundation tests + docs only; transport runtime behavior and protocol/schema remain unchanged.

Current eighteenth-slice boundary (`2026-02-11`):
- Centralized deterministic loopback endpoint-allocation helper logic shared by both client/server transport contract tests:
  - added shared helper `src/engine/network/tests/loopback_endpoint_alloc.hpp`
    (`BindLoopbackEndpointDeterministic`),
  - migrated both `CreateLoopbackServerEndpoint` and `CreateLoopbackServerTransport` to the shared helper.
- Preserved accepted ordering/disconnect contracts and protocol/schema boundaries.
- Scope remains network foundation tests + docs only; transport runtime behavior and protocol/schema remain unchanged.

Current nineteenth-slice boundary (`2026-02-11`):
- Centralized remaining duplicated live-loopback client fixture helpers shared across client/server transport contract tests:
  - added shared helper `src/engine/network/tests/loopback_enet_fixture.hpp`,
  - migrated loopback endpoint create/retry, pump, send, teardown, and payload decode helpers in
    `client_transport_contract_test` and `server_transport_contract_test` to the shared helper.
- Preserved accepted ordering/disconnect contracts and protocol/schema boundaries.
- Scope remains network foundation tests + docs only; transport runtime behavior and protocol/schema remain unchanged.

Current twentieth-slice boundary (`2026-02-11`):
- Converted shared loopback ENet fixture implementation from header-only duplication to a compiled test utility module:
  - added compiled source `src/engine/network/tests/loopback_enet_fixture.cpp`,
  - changed `src/engine/network/tests/loopback_enet_fixture.hpp` to stable declarations-only API,
  - added `network_test_loopback_fixture` target in `src/engine/CMakeLists.txt` and linked it into both transport contract tests.
- Preserved accepted ordering/disconnect/reconnect contracts and protocol/schema boundaries.
- Scope remains network foundation tests + docs only; transport runtime behavior and protocol/schema remain unchanged.

Current twenty-first-slice boundary (`2026-02-11`):
- Converted deterministic endpoint-allocation helper implementation from header-only to a compiled test utility module:
  - added compiled source `src/engine/network/tests/loopback_endpoint_alloc.cpp`,
  - changed `src/engine/network/tests/loopback_endpoint_alloc.hpp` to stable declaration-only API,
  - added `network_test_endpoint_alloc` target in `src/engine/CMakeLists.txt` and linked it into both transport contract tests.
- Preserved accepted ordering/disconnect/reconnect contracts and protocol/schema boundaries.
- Scope remains network foundation tests + docs only; transport runtime behavior and protocol/schema remain unchanged.

Current twenty-second-slice boundary (`2026-02-11`):
- Hardened warning-observability assertions to use structured sink/event contracts (not message substrings):
  - added shared structured sink helper `src/engine/network/tests/structured_log_event_sink.hpp`,
  - migrated unregistered-backend warning checks in client/server transport contract tests to structured warning/error event-count assertions keyed by logger identity.
- Preserved accepted ordering/disconnect/reconnect contracts and protocol/schema boundaries.
- Scope remains network foundation tests + docs only; transport runtime behavior and protocol/schema remain unchanged.

Current twenty-third-slice boundary (`2026-02-11`):
- Hardened live timeout-race coverage stability in `client_transport_contract_test` (`TestLiveTimeoutDisconnectRaceTerminalOrdering`) to reduce reconnect-timeout flakes under wrapper load:
  - widened reconnect/terminal timing budgets used only by this coverage path,
  - switched reconnect payload-burst send gating to wait for observed reconnect `Connected` plus server peer readiness,
  - strengthened loopback restart retry budget at fixed port to reduce transient post-drop bind failures.
- Preserved accepted ordering/disconnect/reconnect contracts and protocol/schema boundaries.
- Scope remains network foundation tests + docs only; transport runtime behavior and protocol/schema remain unchanged.

Current twenty-fourth-slice boundary (`2026-02-11`):
- Hardened live timeout-race coverage against flake sensitivity and wall-time variance in `client_transport_contract_test` (`TestLiveTimeoutDisconnectRaceTerminalOrdering`) with deterministic reconnect readiness gating and bounded retry instrumentation:
  - split reconnect handling into deterministic readiness and delivery phases,
  - gated reconnect burst send behind stable reconnect readiness (`Connected` observed + ready server peer across bounded polls),
  - added bounded probe retry counters and interval-based send pacing in reconnect and terminal phases with explicit failure diagnostics.
- Preserved accepted ordering/disconnect/reconnect contracts and protocol/schema boundaries.
- Scope remains network foundation tests + docs only; transport runtime behavior and protocol/schema remain unchanged.

Current twenty-fifth-slice boundary (`2026-02-11`):
- Hardened timeout-race stability follow-up in `client_transport_contract_test` (`TestLiveTimeoutDisconnectRaceTerminalOrdering`) by replacing per-poll thread churn with deterministic bounded server-pump lifecycle control:
  - added scoped `start_server_pump` / `stop_server_pump` lifecycle helpers local to timeout-race coverage,
  - switched reconnect-phase client polling from per-poll pump-thread helper to direct poll with background bounded pump lifecycle,
  - preserved existing deterministic reconnect-readiness gating and bounded retry instrumentation while reducing per-iteration thread setup jitter.
- Preserved accepted ordering/disconnect/reconnect contracts and protocol/schema boundaries.
- Scope remains network foundation tests + docs only; transport runtime behavior and protocol/schema remain unchanged.

Current twenty-sixth-slice boundary (`2026-02-11`):
- Hardened timeout-race harness stability follow-up by centralizing bounded timeout-race controls/diagnostics in shared network test helpers, while keeping transport runtime behavior unchanged:
  - added shared loopback pump lifecycle helpers in `src/engine/network/tests/loopback_enet_fixture.hpp/.cpp` (`StartLoopbackEndpointPumpThread`, `StopLoopbackEndpointPumpThread`),
  - added shared bounded probe-loop harness helper + diagnostics counters in `src/engine/network/tests/loopback_enet_fixture.hpp/.cpp` (`RunBoundedProbeLoop`, `BoundedProbeLoopOptions`, `BoundedProbeLoopDiagnostics`),
  - refactored `TestLiveTimeoutDisconnectRaceTerminalOrdering()` in `src/engine/network/tests/client_transport_contract_test.cpp` to consume shared helpers for reconnect-readiness, reconnect-delivery, and terminal-disconnect probe loops with deterministic diagnostics.
- Preserved accepted ordering/disconnect/reconnect contracts and protocol/schema boundaries.
- Scope remains network foundation tests + docs only; transport runtime behavior and protocol/schema remain unchanged.

Current twenty-seventh-slice boundary (`2026-02-11`):
- Consolidated network-test helper modules into a single shared compiled support target for transport contract tests:
  - converted structured sink helper from header-only to compiled implementation by adding
    `src/engine/network/tests/structured_log_event_sink.cpp`,
  - consolidated `loopback_endpoint_alloc.cpp`, `loopback_enet_fixture.cpp`, and
    `structured_log_event_sink.cpp` into `network_test_support` in `src/engine/CMakeLists.txt`,
  - updated `client_transport_contract_test` and `server_transport_contract_test` linkage to consume
    `network_test_support` only.
- Preserved accepted ordering/disconnect/reconnect contracts and protocol/schema boundaries.
- Scope remains network foundation tests + docs only; transport runtime behavior and protocol/schema remain unchanged.

Current twenty-eighth-slice boundary (`2026-02-11`):
- Reduced timeout-race harness drift by extracting reusable bounded phase-driver scaffolding into
  `network_test_support`:
  - added shared `RunBoundedProbePhase` helper and `BoundedProbePhaseResult` in
    `src/engine/network/tests/loopback_enet_fixture.hpp/.cpp`,
  - refactored `TestLiveTimeoutDisconnectRaceTerminalOrdering()` in
    `src/engine/network/tests/client_transport_contract_test.cpp` to use shared phase-driver helper for reconnect-readiness, reconnect-delivery, and terminal-disconnect phases while preserving existing assertions and diagnostics behavior.
- Preserved accepted ordering/disconnect/reconnect contracts and protocol/schema boundaries.
- Scope remains network foundation tests + docs only; transport runtime behavior and protocol/schema remain unchanged.

## Non-Goals
- No packet schema redesign as part of the first slice.
- No gameplay netcode prediction/reconciliation work in this project.
- No backend API leakage into `src/game/*`.

## Validation
From `m-rewrite/`:

```bash
./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio
./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio
./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio
./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio
```

## Trace Channels
- `engine.server`
- `net.server`
- `net.client`

## Build/Run Commands
```bash
./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio
./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio
```

## First Session Checklist
1. Read `AGENTS.md`, then `docs/AGENTS.md`, then this file.
2. Document the engine/game boundary for transport/session responsibilities before code edits.
3. Land one vertical slice with no protocol schema changes.
4. Run required validation in both assigned build dirs.
5. Update this project file and `docs/projects/ASSIGNMENTS.md`.

## Current Status
- `2026-02-10`: Project created and prioritized; first slice not started.
- `2026-02-10`: Landed first vertical slice:
  - Added `karma::network::ServerTransport` contract and engine ENet implementation.
  - Migrated server accept/receive/send/disconnect lifecycle path in `src/game/server/net/enet_event_source.cpp` to use engine transport.
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Validation passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`).
- `2026-02-10`: Landed second vertical slice:
  - Added `karma::network::ClientTransport` contract and engine ENet implementation.
  - Migrated client connect/poll/disconnect + reliable send lifecycle path in `src/game/client/net/client_connection.cpp` to use engine transport.
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Validation passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`).
- `2026-02-10`: Landed third vertical slice:
  - Added bounded reconnect/backoff policy fields to client transport connect options and engine reconnect state machine behavior.
  - Added config-driven reconnect consumption and join-bootstrap replay on reconnect in `src/game/client/net/client_connection.cpp`.
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`).
- `2026-02-10`: Landed fourth vertical slice:
  - Added transport registration/extensibility hooks in engine transport contracts (`RegisterClientTransportFactory`, `RegisterServerTransportFactory`) and backend-ID resolution in transport creation paths.
  - Added backend-name transport config plumbing and default ENet registration in client/server engine transport implementations.
  - Updated client transport consumption to pass custom backend IDs while preserving existing `auto|enet` behavior.
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`).
- `2026-02-10`: Landed fifth vertical slice:
  - Added shared transport pump normalizer helper in engine network layer.
  - Updated client/server transport poll paths to stage ENet events and emit normalized cycle order (`Connected`, `Received`, `Disconnected`).
  - Normalized client reconnect-generated lifecycle events through the same ordering path.
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`).
- `2026-02-10`: Landed sixth vertical slice:
  - Added explicit engine transport contract tests in `src/engine/network/tests`:
    - `client_transport_contract_test` locks normalized ordering, disconnect-terminal semantics, and reconnect-cycle ordering behavior.
    - `server_transport_contract_test` locks normalized ordering and disconnect-terminal semantics.
  - Wired transport contract tests into `src/engine/CMakeLists.txt` and `scripts/test-server-net.sh`.
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`).
- `2026-02-10`: Landed seventh vertical slice:
  - Updated server transport consumption in `src/game/server/net/enet_event_source.cpp` to mirror client backend-ID plumbing:
    - reads `network.ServerTransportBackend` and passes it via `ServerTransportConfig.backend_name`;
    - keeps enum parsing for known built-ins while allowing custom backend IDs through engine transport creation.
  - Updated server transport creation failure logging to report the configured backend ID string.
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`).
- `2026-02-10`: Landed eighth vertical slice:
  - Added explicit server-side backend-ID config-plumbing contract coverage in `src/game/tests/server_net_contract_test.cpp`:
    - built-in `auto`/`enet` behavior checks;
    - custom backend-ID pass-through check;
    - unregistered backend-ID failure check.
  - Added fake server transport factory test harness to assert handed-off `ServerTransportConfig` fields deterministically.
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
- `2026-02-10`: Landed ninth vertical slice:
  - Added warning-level observability for unregistered transport backend IDs in engine transport creation paths (`client_transport.cpp`, `server_transport.cpp`) with client/server parity.
  - Added explicit warning+failure contract assertions for unregistered backend IDs in:
    - `src/engine/network/tests/client_transport_contract_test.cpp`
    - `src/engine/network/tests/server_transport_contract_test.cpp`
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
- `2026-02-10`: Landed tenth vertical slice:
  - Added per-key transport-pump normalization helper (`NormalizePumpEventsPerKey`) and applied it to server transport poll normalization keyed by peer token.
  - Updated client transport poll path to use the same per-key normalization helper with a single-key function for parity.
  - Added explicit lifecycle-edge ordering contract coverage:
    - `src/engine/network/tests/server_transport_contract_test.cpp` locks per-peer ordering and disconnect-terminal semantics with multi-peer staged events.
    - `src/engine/network/tests/client_transport_contract_test.cpp` locks single-peer lifecycle-edge ordering semantics.
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
- `2026-02-10`: Landed eleventh vertical slice:
  - Added `TestHighVolumeMultiPeerOrderingStress()` in `src/engine/network/tests/server_transport_contract_test.cpp` and wired it into test main.
  - Locked high-volume per-peer normalized ordering and disconnect-terminal behavior over a large deterministic interleaved staged-event set.
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
- `2026-02-10`: Landed twelfth vertical slice:
  - Added `TestLiveLoopbackMultiPeerOrderingStress()` in `src/engine/network/tests/server_transport_contract_test.cpp`.
  - Locked live loopback multi-peer burst behavior across runtime poll cycles:
    - per-peer contiguous/monotonic lifecycle-event ordering,
    - terminal disconnect checks per peer,
    - full bounded per-peer receive-burst assertions.
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
- `2026-02-10`: Landed thirteenth vertical slice:
  - Added `TestLiveReconnectPayloadInterleaveStress()` in `src/engine/network/tests/client_transport_contract_test.cpp`.
  - Locked live reconnect-cycle behavior across runtime loopback pumping:
    - no `Received` event before reconnect `Connected` in each cycle,
    - bounded full post-reconnect payload-burst reception per cycle,
    - no terminal `Disconnected` surfaced during reconnect-cycle recovery.
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
- `2026-02-10`: Landed fourteenth vertical slice:
  - Hardened live timeout/disconnect edge-race coverage in `TestLiveTimeoutDisconnectRaceTerminalOrdering()` in `src/engine/network/tests/client_transport_contract_test.cpp`.
  - Locked reconnect-race and terminal ordering behavior under live loopback timing pressure:
    - reconnect recovery must complete before terminal disconnect in recoverable phase,
    - terminal disconnect is emitted exactly once after bounded reconnect exhaustion,
    - no `Connected`/`Received` events are emitted after terminal disconnect.
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
- `2026-02-10`: Landed fifteenth vertical slice:
  - Added `TestLiveLoopbackMultiPeerFairnessUnderReconnectChurn()` plus churn helpers in `src/engine/network/tests/server_transport_contract_test.cpp` (payload decode attribution, loopback client churn hardening, optional token-reuse restart allowance for churn validation).
  - Locked live fairness-under-churn coverage while preserving existing per-peer normalized ordering/disconnect-terminal invariants.
  - Kept `messages.proto` and `src/game/net/protocol*` unchanged.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
- `2026-02-11`: Landed sixteenth vertical slice:
  - Hardened live loopback coverage resilience in `src/engine/network/tests/server_transport_contract_test.cpp` via wider/retried server endpoint allocation, client endpoint retry helper, active-client settle gating, and widened timing budgets for churn/ordering stress tests.
  - Preserved protocol/schema boundaries and transport runtime behavior (coverage-only slice).
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
- `2026-02-11`: Landed seventeenth vertical slice:
  - Made loopback endpoint allocation deterministic across live-stress client/server helpers:
    - `CreateLoopbackServerEndpoint` (client transport contract test) now uses deterministic multi-pass sequential scan/retry.
    - `CreateLoopbackServerTransport` (server transport contract test) now uses deterministic multi-pass sequential scan/retry with no time-based start offset.
  - Preserved protocol/schema boundaries and all accepted ordering contracts.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
- `2026-02-11`: Landed eighteenth vertical slice:
  - Added shared deterministic loopback endpoint-allocation helper in
    `src/engine/network/tests/loopback_endpoint_alloc.hpp`.
  - Updated both transport contract suites to consume the shared helper:
    - `src/engine/network/tests/client_transport_contract_test.cpp`
    - `src/engine/network/tests/server_transport_contract_test.cpp`
  - Preserved protocol/schema boundaries and all accepted ordering/disconnect contracts.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
- `2026-02-11`: Landed nineteenth vertical slice:
  - Added shared live-loopback ENet fixture helper in
    `src/engine/network/tests/loopback_enet_fixture.hpp` covering create/retry, pump, send, teardown, and payload decode.
  - Updated both transport contract suites to consume the shared helper:
    - `src/engine/network/tests/client_transport_contract_test.cpp`
    - `src/engine/network/tests/server_transport_contract_test.cpp`
  - Preserved protocol/schema boundaries and all accepted ordering/disconnect contracts.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
- `2026-02-11`: Landed twentieth vertical slice:
  - Moved shared loopback fixture implementation from header-only to compiled utility:
    - added `src/engine/network/tests/loopback_enet_fixture.cpp`,
    - retained stable helper API in `src/engine/network/tests/loopback_enet_fixture.hpp`,
    - linked new `network_test_loopback_fixture` into client/server transport contract tests via `src/engine/CMakeLists.txt`.
  - Preserved protocol/schema boundaries and all accepted ordering/disconnect/reconnect contracts.
  - Required validation rerun by overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
- `2026-02-11`: Landed twenty-first vertical slice:
  - Moved deterministic endpoint-allocation helper implementation from header-only to compiled utility:
    - added `src/engine/network/tests/loopback_endpoint_alloc.cpp`,
    - retained stable helper API in `src/engine/network/tests/loopback_endpoint_alloc.hpp`,
    - linked new `network_test_endpoint_alloc` into client/server transport contract tests via `src/engine/CMakeLists.txt`.
  - Preserved protocol/schema boundaries and all accepted ordering/disconnect/reconnect contracts.
  - Required validation rerun by specialist and overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
- `2026-02-11`: Landed twenty-second vertical slice:
  - Added structured sink helper in
    `src/engine/network/tests/structured_log_event_sink.hpp`.
  - Updated unregistered-backend warning observability assertions in:
    - `src/engine/network/tests/client_transport_contract_test.cpp`
    - `src/engine/network/tests/server_transport_contract_test.cpp`
    to assert structured warning/error event counts by logger identity instead of warning-message substring content.
  - Preserved protocol/schema boundaries and all accepted ordering/disconnect/reconnect contracts.
  - Required validation rerun by specialist and overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs); one transient overseer rerun failure (`client_transport_contract_test` reconnect timeout-race wait) reproduced once and passed on immediate rerun.
  - Risks/deferrals:
    - structured warning assertions now depend on per-test logger identity and warning-event cardinality; future additional warning emissions in the same code paths may require explicit contract updates,
    - no transport runtime behavior changes were made in this slice (network-test assertion hardening only).
- `2026-02-11`: Landed twenty-third vertical slice:
  - Hardened `TestLiveTimeoutDisconnectRaceTerminalOrdering()` in
    `src/engine/network/tests/client_transport_contract_test.cpp` with wider reconnect/terminal timing budgets, reconnect-burst send gating after observed reconnect `Connected`, and stronger fixed-port loopback restart retry.
  - Preserved protocol/schema boundaries and all accepted ordering/disconnect/reconnect contracts.
  - Required validation rerun by specialist passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
  - Overseer rerun results:
    - `test-server-net.sh` passed in both assigned build dirs (`9/9` each),
    - full `bzbuild.py -c` in both assigned build dirs was temporarily blocked at that overseer checkpoint by unrelated in-review renderer compile errors in `src/engine/renderer/tests/directional_shadow_contract_test.cpp` and `src/engine/renderer/backends/bgfx/backend_bgfx.cpp`.
  - Risks/deferrals:
    - live timeout-race coverage remains scheduler/load sensitive because it relies on runtime ENet timeout/reconnect progression; this slice reduces flake likelihood but cannot guarantee zero timing variance across all host-load conditions,
    - no transport runtime behavior changes were made in this slice (network-test-only hardening).
- `2026-02-11`: Landed twenty-fourth vertical slice:
  - Hardened `TestLiveTimeoutDisconnectRaceTerminalOrdering()` in
    `src/engine/network/tests/client_transport_contract_test.cpp` with deterministic reconnect readiness gating and bounded retry instrumentation (reconnect/terminal probe pacing + bounded counters + explicit retry diagnostics).
  - Reduced wall-time variance pressure by splitting reconnect handling into readiness and delivery phases with stable-ready gating before burst send.
  - Preserved protocol/schema boundaries and all accepted ordering/disconnect/reconnect contracts.
  - Required validation rerun by specialist and overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
  - Risks/deferrals:
    - live timeout-race coverage remains inherently scheduler/load sensitive under runtime ENet reconnect/timing races; deterministic gating and bounded retries reduce, but do not eliminate, host-load-dependent variance,
    - no transport runtime behavior changes were made in this slice (network-test-only hardening).
- `2026-02-11`: Landed twenty-fifth vertical slice (accepted):
  - Hardened `TestLiveTimeoutDisconnectRaceTerminalOrdering()` in
    `src/engine/network/tests/client_transport_contract_test.cpp` by replacing reconnect-phase per-poll pump-thread churn with deterministic bounded background server-pump lifecycle control (`start_server_pump`/`stop_server_pump`).
  - Kept deterministic reconnect-readiness gating, bounded retry instrumentation, and ordering/disconnect/reconnect assertions intact.
  - Preserved protocol/schema boundaries and all accepted ordering/disconnect/reconnect contracts.
  - Required validation rerun by specialist and overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
  - Risks/deferrals:
    - live timeout-race coverage remains scheduler/load sensitive because runtime ENet reconnect/timeout races are inherently timing-dependent; this slice reduces thread-churn-driven jitter but cannot eliminate host-load variance,
    - no transport runtime behavior changes were made in this slice (network-test-only hardening).
- `2026-02-11`: Landed twenty-sixth vertical slice (accepted):
  - Centralized bounded timeout-race harness controls/diagnostics into shared network test helpers (`loopback_enet_fixture.hpp/.cpp`) and refactored `TestLiveTimeoutDisconnectRaceTerminalOrdering()` in `client_transport_contract_test.cpp` to consume them.
  - Kept deterministic reconnect-readiness gating semantics and existing ordering/disconnect/reconnect assertions intact.
  - Required validation rerun by specialist and overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs); one transient overseer wrapper failure reproduced once during concurrent runs and then passed on immediate sequential rerun.
  - Risks/deferrals:
    - live timeout-race coverage remains scheduler/load sensitive because ENet timeout/reconnect progression is runtime timing-dependent; shared bounded diagnostics improve determinism and triage quality but cannot fully eliminate host-load variance,
    - no transport runtime behavior changes were made in this slice (network-test/docs-only hardening).
- `2026-02-11`: Landed twenty-seventh vertical slice (accepted):
  - Implemented a single shared compiled network-test support target by consolidating:
    - endpoint allocation helpers (`loopback_endpoint_alloc.cpp`),
    - loopback ENet fixture helpers (`loopback_enet_fixture.cpp`),
    - structured log sink helpers (`structured_log_event_sink.cpp`).
  - Updated `src/engine/CMakeLists.txt` so both `client_transport_contract_test` and
    `server_transport_contract_test` link `network_test_support` only.
  - Preserved existing transport contract semantics/behavior; no transport runtime changes.
  - Required validation rerun by specialist and overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
  - Risks/deferrals:
    - live timeout-race coverage remains scheduler/load sensitive due runtime ENet timing behavior; this slice is build/linkage consolidation only and does not alter runtime timing behavior.
- `2026-02-11`: Landed twenty-eighth vertical slice (accepted):
  - Extracted reusable timeout-race phase-driver scaffolding into shared test support:
    - added `RunBoundedProbePhase` + `BoundedProbePhaseResult` to
      `src/engine/network/tests/loopback_enet_fixture.hpp/.cpp`,
    - refactored timeout-race reconnect/terminal phases in
      `src/engine/network/tests/client_transport_contract_test.cpp` to consume the shared helper while keeping existing assertions/order checks intact.
  - Required validation rerun by specialist and overseer passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh`, `9/9` in both wrapper runs).
  - Risks/deferrals:
    - live timeout-race coverage remains scheduler/load sensitive because ENet timeout/reconnect progression is runtime timing-dependent; this slice reduces harness drift only and does not change runtime behavior.
- `2026-02-12`: Landed twenty-ninth vertical slice (accepted; closeout):
  - Reduced remaining timeout-race harness duplication in `src/engine/network/tests/client_transport_contract_test.cpp` by:
    - introducing a shared local reconnect-phase polling helper used by both reconnect readiness and reconnect delivery phases,
    - introducing a shared local bounded-probe option builder for reconnect phases.
  - Scope remained test-harness-only (no transport runtime behavior changes, no protocol/schema changes).
  - Required validation:
    - `./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio` -> success,
    - `./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio` -> success,
    - `./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio` -> PASS (`9/9`),
    - `./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio` -> transient `client_transport_contract_test` timeout-race/reconnect flakes observed on immediate retries, then PASS (`9/9`) in same session retry window.
  - Residual risk/deferral:
    - `client_transport_contract_test` live reconnect/timeout race coverage remains scheduler/load sensitive in high-contention runs; slice 29 reduced harness duplication only and does not change runtime ENet behavior.

## Open Questions
- No new open questions in this slice.

## Handoff Checklist
- [x] Code updated
- [x] Tests run and summarized
- [x] Docs updated
- [x] Risks/open questions listed
