# Karma Demo Runtime (Client/Server)

## Project Snapshot
- Current owner: `specialist-demo-s1`
- Status: `in progress (DEMO-S2/S3/S4/S5/S6/S7/S8A complete)`
- Immediate next task: collect Linux CI telemetry trend and trigger `DEMO-S8` only when escalation criteria match telemetry fields.
- Validation gate:
  - `m-overseer`: `./agent/scripts/lint-projects.sh`
  - `m-karma`: `./abuild.py -c -d <build-dir>` and demo smoke command packet defined in this doc

## Mission
Create first-class `m-karma` demo runtime binaries, `client` and `server`, that:
- use only `m-karma` internals,
- demonstrate real client/server interaction end-to-end (bind, connect, pre-auth/join handshake, minimal command exchange, disconnect),
- read and layer config/data via the existing bootstrap path (`data/client/config.json`, `data/server/config.json`, `--data-dir`, `--user-config`, `KARMA_DATA_DIR`),
- become the primary SDK-user confidence path while preserving necessary low-level deterministic tests.

## Foundation References
- `m-karma/src/app/client/bootstrap.cpp`
- `m-karma/src/app/server/bootstrap.cpp`
- `m-karma/src/app/shared/bootstrap.cpp`
- `m-karma/src/common/data/directory_override.cpp`
- `m-karma/src/cli/client/app_options.cpp`
- `m-karma/src/cli/server/app_options.cpp`
- `m-karma/include/karma/network/transport/client.hpp`
- `m-karma/include/karma/network/transport/server.hpp`
- `m-karma/include/karma/network/server/auth/preauth.hpp`
- `m-karma/include/karma/network/server/session/hooks.hpp`
- `m-karma/include/karma/network/server/session/join_runtime.hpp`
- `m-karma/include/karma/network/server/session/leave_runtime.hpp`
- `m-karma/src/network/tests/contracts/client_transport_contract_test.cpp`
- `m-karma/src/network/tests/contracts/server_transport_contract_test.cpp`
- `m-bz3/src/client/main.cpp`
- `m-bz3/src/server/main.cpp`

## Why This Is Separate
This work is cross-cutting and intentionally product-facing:
- it defines how SDK users validate real runtime integration quickly,
- it affects build target policy, runtime packaging confidence, and test strategy,
- it creates a reference runtime surface likely to grow into a dedicated `src/demo/*` tree,
- it should not be mixed into unrelated gameplay/UI/cleanup slices.

## Direction Lock
1. `m-karma` must emit `client` and `server` binaries in normal build outputs, analogous to `m-bz3` emitting `bz3` and `bz3-server`.
2. Demo runtime code remains private implementation code and must not bloat exported SDK library APIs.
3. Demo binaries must use existing bootstrap/config/data layering behavior instead of custom loaders.
4. CLI behavior should track `m-karma` parser behavior by default; demo-only options must be additive and explicit.
5. End-to-end runtime traces become the primary integration confidence tool; low-level deterministic tests remain where they protect tight contracts.

## Comprehensive Requirements

### R1: Runtime Binary Outputs
- Add build targets that produce:
  - `<build-dir>/client`
  - `<build-dir>/server`
- These are `m-karma` binaries, not `m-bz3` wrappers.

### R2: Internal-Only Implementation
- Use only `m-karma` internals for runtime logic (transport/auth/session/config/bootstrap).
- No dependency on `m-bz3` runtime/domain/protocol code.

### R3: Data/Config Integration
- `server` uses bootstrap path requiring `server/config.json` and layered from `data/server/config.json`.
- `client` uses bootstrap path requiring `client/config.json` and layered from `data/client/config.json`.
- Respect `--data-dir`, `--user-config` (where applicable), and `KARMA_DATA_DIR`.
- Demonstrate canonical fixture usage under `demo/` for deterministic local runs.

### R4: End-to-End Interaction Coverage
- Required minimum interaction path:
  - server bind/listen,
  - client connect,
  - pre-auth + join decision,
  - minimal command exchange (for example ping/pong or equivalent authoritative roundtrip),
  - orderly disconnect/leave.

### R5: SDK Consumer Validation
- Add an in-tree consumer path that compiles/runs demo client/server using installed SDK package artifacts (`find_package(KarmaSDK CONFIG REQUIRED)`), so SDK contract health is testable in-tree.

### R6: Test Strategy Realignment
- Add process-level demo smoke gates as primary user confidence checks.
- Keep deterministic contract tests where they protect parser/codec/auth/config semantics.
- Retire overlapping tests only after equivalence is proven and documented.

## Proposed Source Layout
Target evolution path (locked as preferred structure):
- `m-karma/src/demo/client/*`
- `m-karma/src/demo/server/*`
- `m-karma/src/demo/net/*` (demo wire protocol and message adapters)
- `m-karma/src/demo/shared/*`

Initial pragmatic bootstrap may land as thin files before full decomposition, but end-state ownership should converge here.

## CMake/Build Plan

### Build Graph
- Add `m-karma/cmake/sdk/apps.cmake`.
- Include it from `m-karma/cmake/40_sdk_subdir.cmake`.
- Create executable targets (internal names allowed) with output names set to:
  - `client`
  - `server`
- Link against existing SDK libs (`karma_sdk_core`, `karma_sdk_client`) and existing internal dependencies.

### Anti-Bloat Constraints
- Do not add demo runtime sources into `karma_sdk_core` or `karma_sdk_client` source lists.
- Do not add new public headers unless a genuinely reusable engine contract is discovered.
- Keep demo protocol/runtime headers private to demo target include paths.

## Runtime/CLI Behavior Plan

### Compatibility Goal
- Match `m-karma` client/server parser behavior for shared options by default:
  - common options (`--trace`, `--data-dir`, `--language`, `--strict-config`, `--timestamp-logging`, `--user-config` where supported),
  - server options (`--server-config`, `--listen-port`, `--community`, backend overrides),
  - client options (`--server`, `--username`, `--password`, backend overrides).

### Caveat
- Runtime semantics can only match what `m-karma` implements.
- `bz3`-specific gameplay semantics are explicitly out of scope for demo binaries.

### Demo-Only Options
- If needed, add additive demo options with explicit namespacing (for example `--demo-script`, `--net-smoke`) so base parser compatibility remains clear.

## Data/Demo Integration Plan
- Keep `data/` as default runtime contract root for required config files.
- Use `demo/` as canonical reproducible fixture space:
  - `demo/communities/*`
  - `demo/users/*`
  - `demo/worlds/*`
- Ensure docs and command examples show deterministic runs with either:
  - `--data-dir <repo>/data` + `HOME=<repo>/demo/users/<fixture>`,
  - or fixture overlays as needed for scenario tests.

## SDK Consumer Build/Test Plan
- Add an in-tree SDK consumer example folder (for example `m-karma/examples/demo-sdk-consumer/`).
- Consumer must use package-based integration:
  - `find_package(KarmaSDK CONFIG REQUIRED)`
  - link to `karma::core` and/or `karma::client`.
- Add script wrappers that:
  1. build/install SDK from `m-karma`,
  2. configure/build consumer demo binaries against installed SDK,
  3. run consumer server/client smoke interaction.

## Validation Strategy

### Required Gates (project)
```bash
# project-doc hygiene
cd m-overseer
./agent/scripts/lint-projects.sh

# build m-karma runtime targets + existing transport contracts
cd ../m-karma
export ABUILD_AGENT_NAME=demo-runtime
./abuild.py --claim-lock -d <build-dir>
./abuild.py -c -d <build-dir>
ctest --test-dir <build-dir> -R "client_transport_contract_test|server_transport_contract_test" --output-on-failure

# demo process smoke
./scripts/test-demo-client-server-smoke.sh <build-dir>

# sdk consumer smoke
./scripts/test-sdk-demo-consumer.sh <build-dir> <sdk-prefix>

# trace-first matrix (DEMO-S5 primary gate)
./scripts/test-demo-trace-matrix.sh <build-dir> <sdk-prefix>

./abuild.py --release-lock -d <build-dir>
```

### Test Rationalization Policy
- Keep:
  - low-level parser/config/auth/transport contract tests that are deterministic and narrow.
- Shift primary confidence to:
  - process-level client/server smoke scenarios with trace evidence.
- Retire overlap only after:
  - equivalent behavior coverage is proven by scenario scripts and documented in this project.

## Milestones

### DEMO-S0: Project Baseline (this slice)
- Create this project doc and assignment row.
- Lock direction and constraints.
- Acceptance:
  - docs committed; ownership and next task recorded.

### DEMO-S1: Runtime Target Bring-Up
- Add `client` and `server` build targets in `m-karma`.
- Use existing app runner/bootstrap paths.
- Acceptance:
  - both binaries build and print help.

### DEMO-S2: Minimal Interaction Loop
- Implement bind/connect/pre-auth+join/minimal command/disconnect flow.
- Acceptance:
  - deterministic local smoke run succeeds and is scriptable.

### DEMO-S3: Data/Demo Fixture Integration
- Align runtime defaults and docs with `data/` + `demo/` fixture policy.
- Acceptance:
  - one reproducible fixture-backed command packet documented and validated.

### DEMO-S4: SDK Consumer In-Tree Gate
- Add package-based consumer build/test for demo runtime path.
- Acceptance:
  - scripted consumer smoke passes against installed SDK payload.

## Milestone Progress
- [x] `DEMO-S1`: runtime target bring-up.
- [x] `DEMO-S2`: deterministic runtime interaction loop (connect/join/ping-pong/leave/disconnect).
- [x] `DEMO-S3`: fixture-backed deterministic smoke integration under `demo/`.
- [x] `DEMO-S4` bootstrap scaffold: in-tree package-based consumer skeleton under `examples/demo-sdk-consumer/`.
- [x] `DEMO-S4` full gate: scripted installed-SDK consumer smoke + validation evidence.
- [x] `DEMO-S5`: trace-first matrix harness + keep/retire decision mapping with no test removal.
- [x] `DEMO-S6`: Linux CI-required wiring for trace matrix gate with bounded retry and diagnostics retention.
- [x] `DEMO-S7`: timeout-race hardening + phase/timestamp diagnostics + repeat stability gate.
- [x] `DEMO-S8A`: post-S7 telemetry automation for evidence-based DEMO-S8 escalation decisions.

### DEMO-S5: Test Strategy Convergence
- Add trace-first scenario harness and map overlap with existing integration tests.
- Acceptance:
  - documented keep/retire decision list with no contract coverage regression.

### DEMO-S6: CI Required Gate Integration
- Wire trace-first demo matrix into required Linux SDK desktop CI validation.
- Acceptance:
  - `sdk-desktop` Linux job runs `./scripts/test-demo-trace-matrix.sh build-sdk out/karma-sdk`,
  - one bounded retry is applied for transient client transport timeout-race,
  - first-attempt logs are retained and uploaded,
  - matrix artifacts are uploaded from `build-sdk/demo-trace-matrix-artifacts/**`.

### DEMO-S7: Post-S6 Reliability Hardening
- Root-cause and harden sustained timeout-race reconnect flake path without reducing coverage.
- Acceptance:
  - deterministic synchronization replaces fixed restart timing in timeout-race test path,
  - contract failures include explicit phase/timestamp diagnostics,
  - repeatable stability script (`scripts/test-demo-transport-stability.sh`) runs contract set multiple times with fail-fast matrix summary,
  - DEMO-S6 trace-matrix behavior remains intact.

### DEMO-S8A: Telemetry Automation (Post-S7 Monitoring)
- Add deterministic CI telemetry summarization for Linux demo trace-matrix runs.
- Acceptance:
  - `scripts/test-demo-telemetry-summary.sh <ci-log-root> <matrix-artifact-root>` emits:
    - `ci-logs/demo-trace-matrix-telemetry.json`,
    - `ci-logs/demo-trace-matrix-telemetry.log`,
  - telemetry records `attempt_count`, `retry_used`, `final_matrix_status`,
    `wait_drop_seen_in_failure_output`, `wait_terminal_seen_in_failure_output`,
    and `recommendation` (`monitor` vs `escalate_demo_s8`),
  - Linux `sdk-desktop` workflow runs telemetry summary after trace matrix execution and uploads telemetry artifacts,
  - demo trace-matrix pass/fail semantics remain unchanged.

## Owned Paths
- `m-overseer/agent/projects/demo.md`
- `m-overseer/agent/projects/ASSIGNMENTS.md`
- `m-karma/cmake/sdk/apps.cmake` (new)
- `m-karma/src/demo/*` (new)
- `m-karma/scripts/test-demo-client-server-smoke.sh` (new)
- `m-karma/examples/demo-sdk-consumer/*` (new)
- `m-karma/scripts/test-sdk-demo-consumer.sh` (new)
- `m-karma/scripts/test-demo-trace-matrix.sh` (new)
- `m-karma/scripts/test-demo-transport-stability.sh` (new)
- `m-karma/scripts/test-demo-telemetry-summary.sh` (new)

## Interface Boundaries
- Inputs consumed:
  - existing `m-karma` CLI/app/bootstrap/data/config/network internals.
- Outputs exposed:
  - two runtime binaries (`client`, `server`) and reproducible smoke gates.
- Coordination required before changing:
  - `m-karma/cmake/40_sdk_subdir.cmake`
  - `m-karma/cmake/sdk/targets.cmake`
  - `m-karma/cmake/50_sdk_install_export.cmake`
  - `m-karma/scripts/test-server-net.sh`
  - `m-karma/.github/workflows/*` (as gates are integrated)

## Non-Goals
- Do not import `m-bz3` gameplay/runtime code into `m-karma`.
- Do not expand exported SDK API surface for demo convenience.
- Do not remove deterministic low-level tests without explicit replacement evidence.
- Do not treat this as a full game implementation track.

## Trace Channels
- `config`
- `engine.server`
- `engine.app`
- `net.server`
- `net.client`
- `engine.sim`

## Build/Run Commands
```bash
cd m-karma
./abuild.py -c -d build-demo

# server
./build-demo/server --data-dir ./data --listen-port 11899 --trace net.server,engine.server,config

# client
./build-demo/client --data-dir ./data --server 127.0.0.1:11899 --trace net.client,engine.app,config
```

## First Session Checklist
1. Add runtime CMake target wiring without touching exported SDK library source sets.
2. Add thin `main` entry points that use existing `karma::app::{client,server}::Run`.
3. Validate help output and basic startup config resolution for both binaries.
4. Land first process smoke script and make it deterministic.
5. Update docs/assignments with outcomes and blockers.

## Current Status
- `2026-02-22`: project created from operator direction to make `m-karma` ship first-class demo `client`/`server` runtime binaries.
- `2026-02-22`: direction locked to use existing bootstrap/data-layering pipeline (`data/client/config.json`, `data/server/config.json`) instead of ad-hoc loaders.
- `2026-02-22`: long-term structure preference recorded: `src/demo/{client,server,net,shared}` with in-tree SDK consumer validation.
- `2026-02-22`: test strategy intent recorded: process-level trace-rich runtime scenarios become primary confidence, with deterministic contract tests retained for narrow guarantees.
- `2026-02-22`: `DEMO-S1` landed runtime target wiring via `cmake/sdk/apps.cmake` + `cmake/40_sdk_subdir.cmake`, producing build outputs `<build-dir>/client` and `<build-dir>/server`.
- `2026-02-22`: thin runtime entry points added under `src/demo/client/main.cpp` and `src/demo/server/main.cpp`, both delegating CLI/bootstrap handling through `karma::app::{client,server}::Run`.
- `2026-02-22`: `DEMO-S1` acceptance gates passed in `build-demo` (`./abuild.py -c`, `./build-demo/client --help`, `./build-demo/server --help`, and transport contract tests).
- `2026-02-22`: `DEMO-S2` landed private runtime flow (`src/demo/client/runtime.cpp`, `src/demo/server/runtime.cpp`) with deterministic connect/pre-auth+join/ping-pong/leave/disconnect semantics.
- `2026-02-22`: `DEMO-S3` landed private demo wire protocol (`src/demo/net/protocol.*`), deterministic protocol contract test (`demo_protocol_contract_test`), fixture overlay (`demo/worlds/demo-runtime-smoke/config.json`), and process smoke gate script (`scripts/test-demo-client-server-smoke.sh`).
- `2026-02-22`: required validation packet passed in `build-demo`: `./abuild.py -c`, `./scripts/test-server-net.sh build-demo`, regex `ctest` including `demo_.*`, and positive/negative smoke cases.
- `2026-02-22`: `DEMO-S4` bootstrap scaffold landed under `examples/demo-sdk-consumer/` with package-based `find_package(KarmaSDK CONFIG REQUIRED)` consumer targets and `--help` runner entry points.
- `2026-02-22`: `DEMO-S4` full gate landed via `scripts/test-sdk-demo-consumer.sh <build-dir> <sdk-prefix>` with deterministic SDK install (`abuild`), consumer configure/build in `build-demo/demo-sdk-consumer`, explicit `demo_sdk_client --help` + `demo_sdk_server --help` smoke, and Linux runtime audit chaining through `scripts/test-sdk-runtime-linux.sh`.
- `2026-02-22`: full DEMO validation packet passed after S4 completion: `./abuild.py -c -d build-demo`, `./scripts/test-demo-client-server-smoke.sh build-demo`, `./scripts/test-sdk-demo-consumer.sh build-demo out/karma-sdk`, `ctest --test-dir build-demo -R "demo_protocol_contract_test" --output-on-failure`, and `m-overseer` project lint.
- `2026-02-22`: `DEMO-S5` landed `scripts/test-demo-trace-matrix.sh <build-dir> <sdk-prefix>` to orchestrate smoke + SDK consumer gate + transport/protocol contract tests and print deterministic pass/fail matrix summary lines per gate plus overall status.
- `2026-02-22`: `DEMO-S5` added optional artifact retention to `scripts/test-demo-client-server-smoke.sh` (`--keep-artifacts`, `--artifacts-dir`) for trace-first evidence while preserving default behavior.
- `2026-02-22`: `DEMO-S5` keep/retire strategy documented as retain-now with candidate-only retirements and explicit preconditions; no test/gate removals performed.
- `2026-02-22`: `DEMO-S6` wired Linux `sdk-desktop` CI in `.github/workflows/core-test-suite.yml` to run `./scripts/test-demo-trace-matrix.sh build-sdk out/karma-sdk` after existing SDK packaging/consumer/runtime gates, preserving non-Linux behavior.
- `2026-02-22`: `DEMO-S6` added bounded retry policy for Linux CI demo matrix step (maximum one retry) with transparent diagnostics: `ci-logs/demo-trace-matrix-attempt1.log`, `ci-logs/demo-trace-matrix-attempt2.log`, `ci-logs/demo-trace-matrix-summary.log`, and preserved first-attempt artifacts under `ci-logs/demo-trace-matrix-attempt1-artifacts/` when retry is triggered.
- `2026-02-22`: `DEMO-S6` extended diagnostics upload paths to include `build-sdk/demo-trace-matrix-artifacts/**` so final matrix artifacts are retained in CI artifact uploads.
- `2026-02-22`: `DEMO-S7` sustained flake root-cause confirmed in `client_transport_contract_test` phase2 timeout-race path: terminal wait could time out before `Disconnected` when probe sends continued during offline reconnect-exhaustion polling.
- `2026-02-22`: `DEMO-S7` hardened phase2 with deterministic synchronization: split into explicit `wait-drop` then `wait-terminal` subphases, require offline drop signal before terminal gate, and stop probe sends during terminal wait to avoid timing interference while preserving strict assertions.
- `2026-02-22`: `DEMO-S7` diagnostics now emit stage/timestamp evidence for `timeout-race.phase2.wait-drop` and `timeout-race.phase2.wait-terminal`; stability gate evidence passed at `15/15` using `scripts/test-demo-transport-stability.sh build-demo 15`.
- `2026-02-22`: `DEMO-S8A` added telemetry summarizer `scripts/test-demo-telemetry-summary.sh` that emits JSON + concise log under `ci-logs/` with fields for attempt count/retry usage/final status/timeout-race marker detection/recommendation.
- `2026-02-22`: `DEMO-S8A` wired Linux `sdk-desktop` CI to run telemetry summary after matrix execution with `if: always()`, preserving matrix failure semantics while guaranteeing telemetry artifact generation for both pass and fail outcomes.

## DEMO-S1 Handoff Notes
- Slice owner: `specialist-demo-s1`.
- Runtime target outcome: `m-karma` now builds `client` and `server` executables at the build dir root with output names set exactly to `client` and `server`.
- Boundary compliance: demo runtime source remained outside `karma_sdk_core` and `karma_sdk_client` source lists; no public SDK headers were added; no `m-bz3` runtime/domain/protocol code was introduced.
- Runtime behavior scope in this slice: entrypoints are intentionally thin and use existing parser/bootstrap internals; full interaction loop remains queued for `DEMO-S2`.

## DEMO-S2/S3 Handoff Notes
- Runtime interaction path is now deterministic and traceable: server bind/listen, client connect, pre-auth + join decision, ping/pong roundtrip, leave event application, and disconnect lifecycle.
- Server runtime uses required existing internals:
  - `CreateServerTransport` + `ResolveServerTransportConfigFromConfig`,
  - `ReadServerPreAuthConfig`,
  - `ResolveServerSessionJoinDecision`,
  - `ApplyServerSessionLeaveEvent`.
- Client runtime uses required existing internals:
  - `ResolveServerEndpoint` (explicit CLI case),
  - `CreateClientTransport` + reconnect policy mapping,
  - CLI username/password precedence with config fallback defaults when not explicit.
- Protocol layer is private-only under `src/demo/net/*` with explicit encode/decode helpers and bounded validation.
- Smoke evidence is now scriptable and deterministic via `scripts/test-demo-client-server-smoke.sh` using fixture overlay `demo/worlds/demo-runtime-smoke/config.json`.
- Boundary compliance maintained: demo code remains private to demo targets; no additions to SDK library source sets or public headers.

## DEMO-S4 Handoff Notes
- Added minimal in-tree package-based consumer skeleton:
  - `examples/demo-sdk-consumer/CMakeLists.txt`
  - `examples/demo-sdk-consumer/src/client_help_main.cpp`
  - `examples/demo-sdk-consumer/src/server_help_main.cpp`
  - `examples/demo-sdk-consumer/README.md`
- Added full scripted gate:
  - `scripts/test-sdk-demo-consumer.sh <build-dir> <sdk-prefix>`
  - validates inputs/artifacts, performs `abuild` SDK install, configures/builds consumer with repo-local dependency resolution (`build-demo/vcpkg_installed/<triplet>`), runs `--help` smoke, and executes Linux runtime dependency audit.
- `DEMO-S4` acceptance achieved: installed-SDK consumer configure/build/`--help` now passes deterministically on this host.

## DEMO-S5 Handoff Notes
- Added trace-first orchestrator:
  - `scripts/test-demo-trace-matrix.sh <build-dir> <sdk-prefix>`
  - Gate order: `test-demo-client-server-smoke.sh`, `test-sdk-demo-consumer.sh`, then contract regex `ctest` for `client_transport_contract_test|server_transport_contract_test|demo_protocol_contract_test`.
  - Emits per-gate summary lines and final overall PASS/FAIL.
- Extended smoke gate with backward-compatible trace artifact options:
  - `--keep-artifacts`
  - `--artifacts-dir <dir>`
  - default behavior unchanged (temporary artifacts auto-cleaned).
- Boundary compliance preserved:
  - no demo/runtime source movement into `karma_sdk_core` or `karma_sdk_client`,
  - no public SDK surface expansion,
  - no `m-bz3` runtime/domain/protocol imports.

## DEMO-S5 Keep/Retire Mapping (Current)
Retention policy now:
- All existing demo/runtime gates and contract tests remain active.
- Candidate retirements below are planning-only; no removals are authorized in this slice.

| Gate/Test | Behavior Covered | Decision | Rationale | Retire Preconditions |
|---|---|---|---|---|
| `scripts/test-demo-trace-matrix.sh` | Trace-first end-to-end orchestration across smoke + SDK consumer + transport/protocol contracts | `Keep (primary wrapper)` | Gives one deterministic top-level pass/fail matrix and artifact path for triage | `N/A` |
| `scripts/test-demo-client-server-smoke.sh` | Runtime bind/connect/pre-auth+join/roundtrip/leave/disconnect including reject path traces | `Keep` | Primary process-level runtime behavior evidence for DEMO-S2/S3 | Replace only after an equivalent trace-asserted scenario gate proves parity for positive+negative flows across hosts |
| `scripts/test-sdk-demo-consumer.sh` | Installed-SDK consumer configure/build/help + Linux runtime dependency audit | `Keep` | Protects DEMO-S4 package-based consumer contract and transitive dependency resolution | Replace only with an equivalent package-based gate that preserves abuild install and runtime audit semantics |
| `client_transport_contract_test` | Deterministic low-level client transport contract behavior | `Keep` | Guards narrow transport invariants that process smoke does not exhaustively isolate | Candidate retire only after equivalent low-level assertions are proven in replacement harness with stable multi-run history |
| `server_transport_contract_test` | Deterministic low-level server transport contract behavior | `Keep` | Isolates server transport invariants independent of broader runtime flow | Candidate retire only after replacement coverage proves parity for error and lifecycle edges with no regression |
| `demo_protocol_contract_test` | Private demo protocol encode/decode validation and rejection semantics | `Keep` | Guarantees wire-format correctness and robust parser validation | Candidate retire only if protocol layer is removed/replaced and equivalent deterministic parser validation exists |
| Standalone manual packet invocation of smoke/SDK gates outside matrix | Duplicate command-level invocation coverage | `Candidate retire (packet duplication only)` | Matrix already invokes both gates deterministically with summary output | Retire duplicates only after CI and runbook adopt matrix gate as canonical required path |

## DEMO-S6 Handoff Notes
- CI wiring added in `m-karma/.github/workflows/core-test-suite.yml` (`sdk-desktop` job):
  - new Linux-only step `Demo trace matrix gate (Linux)` runs:
    - `./scripts/test-demo-trace-matrix.sh build-sdk out/karma-sdk`
  - runs after existing SDK runtime packaging gate and before diagnostics upload.
- Retry policy (bounded/transparent):
  - if attempt 1 fails, preserve first-attempt matrix artifacts into `ci-logs/demo-trace-matrix-attempt1-artifacts/`,
  - rerun exactly once (attempt 2),
  - fail job if attempt 2 also fails.
- Diagnostics retention:
  - logs: `ci-logs/demo-trace-matrix-attempt1.log`, `ci-logs/demo-trace-matrix-attempt2.log`, `ci-logs/demo-trace-matrix-summary.log`,
  - matrix artifacts: `build-sdk/demo-trace-matrix-artifacts/**` uploaded by existing diagnostics artifact step.

## DEMO-S7 Handoff Notes
- Reliability hardening in `src/network/tests/contracts/client_transport_contract_test.cpp`:
  - root-cause fixed in phase2 timeout-race path by splitting terminal handling into deterministic `wait-drop` and `wait-terminal` subphases,
  - wait-terminal now polls reconnect exhaustion without continuing probe sends, preventing probe-induced timeout masking while server is intentionally offline,
  - preserved strict reconnect/delivery/terminal assertions (no skip/xfail/weakened checks),
  - improved diagnostics with explicit `phase=` markers and monotonic timestamp tags (`[t+...ms]`) for each phase/failure, including phase2 substage markers.
- Added repeat stability gate script:
  - `scripts/test-demo-transport-stability.sh <build-dir> [runs]`
  - default runs: `10`,
  - fail-fast on first failing run,
  - prints per-run matrix lines and final overall summary.
- Trace-matrix policy intact:
  - no behavior changes to `scripts/test-demo-trace-matrix.sh` orchestration semantics.

## DEMO-S8A Handoff Notes
- New telemetry helper:
  - `scripts/test-demo-telemetry-summary.sh <ci-log-root> <matrix-artifact-root>`
  - outputs:
    - `ci-logs/demo-trace-matrix-telemetry.json`
    - `ci-logs/demo-trace-matrix-telemetry.log`
  - emits telemetry fields:
    - `attempt_count` (`1|2`)
    - `retry_used` (`true|false`)
    - `final_matrix_status` (`PASS|FAIL|UNKNOWN`)
    - `wait_drop_seen_in_failure_output`
    - `wait_terminal_seen_in_failure_output`
    - `recommendation` (`monitor|escalate_demo_s8`)
- Linux workflow wiring:
  - `.github/workflows/core-test-suite.yml` runs telemetry step after demo trace matrix using:
    - `./scripts/test-demo-telemetry-summary.sh ci-logs build-sdk/demo-trace-matrix-artifacts`
  - step uses `if: always() && runner.os == 'Linux'`, so telemetry is captured even after matrix failure.
  - telemetry files are uploaded through diagnostics artifact paths.

### DEMO-S8 Escalation Criteria (Telemetry-Driven)
- Trigger `DEMO-S8` immediately when:
  - `final_matrix_status == FAIL` and
  - (`wait_drop_seen_in_failure_output == true` or `wait_terminal_seen_in_failure_output == true`) and
  - `recommendation == escalate_demo_s8`.
- Keep `monitor` when:
  - matrix final status is `PASS`, even if retry was used, unless repeated telemetry trends indicate sustained recurrence.
- Treat sustained recurrence as:
  - two consecutive required Linux CI runs with `recommendation == escalate_demo_s8`, or
  - three `retry_used == true` runs within the latest ten required Linux CI runs with timeout-race markers observed.

## Open Questions
- Should `client` default to full graphics runtime, `--net-smoke`, or auto-fallback based on environment?
- Should demo runtime binaries be installed with SDK artifacts or remain build-tree-only reference apps?
- Which current integration tests are first candidates for subsumption once demo scenario gates are stable?
- What minimum cross-platform smoke matrix is required before marking this project green?

## Handoff Checklist
- [x] Runtime targets (`client`, `server`) build in `m-karma`.
- [x] Minimal end-to-end interaction scenario passes locally.
- [x] Data/config layering behavior verified through bootstrap path.
- [x] SDK consumer in-tree full gate implemented and passing.
- [x] Test overlap keep/retire decisions documented (retain-now policy; no removals).
- [x] CI gate plan updated with Linux-required trace-first matrix wiring and retry/diagnostics policy.
- [x] Post-S6 transport timeout-race hardening validated with repeated stability gate runs.
- [x] Post-S7 telemetry automation landed with explicit DEMO-S8 escalation criteria and CI artifact capture.
