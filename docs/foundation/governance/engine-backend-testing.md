# Engine Backend Testing Governance

This is the long-lived governance reference for physics/audio backend test coverage.

## Scope
Use this when touching:
- `src/engine/physics/*`
- `src/engine/audio/*`
- `include/karma/physics/*`
- `include/karma/audio/*`
- backend test registration in `src/engine/CMakeLists.txt`

## Canonical Wrapper Gate
From `m-rewrite/`:

```bash
./scripts/test-engine-backends.sh <build-dir>
```

Rules:
- In parallel work, always pass explicit `<build-dir>`.
- `build-dev` default is allowed for serialized local checks and CI baseline.
- Delegated operator flows remain `./abuild.py -c -d <build-dir>`-only for configure/build/test (omit `-c` only when intentionally reusing an already configured build dir).

## Covered Test Targets
1. `physics_backend_parity_jolt`
2. `physics_backend_parity_physx`
3. `audio_backend_smoke_sdl3audio`
4. `audio_backend_smoke_miniaudio`

## Result Interpretation
1. Any registered backend test failure is actionable and blocks slice acceptance.
2. If zero backend tests are registered for a profile, treat as profile-specific skip and call it out explicitly in handoff notes.

## Maintenance Rules
1. Keep backend tests deterministic and headless-safe.
2. Update tests before changing backend lifecycle/contract semantics.
3. Keep wrapper/docs/CI alignment updated in the same change when test registration or invocation requirements change.

## Related Docs
- `docs/foundation/governance/testing-ci-governance.md`
- `docs/foundation/policy/execution-policy.md`
- `docs/projects/physics-backend.md`
