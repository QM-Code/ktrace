# Demo Build Flow Consolidation (Single Root kbuild.py)

## Overview
- Consolidate demo orchestration into the root `kbuild.py` using a new `--build-demos` mode.
- Do not create per-demo `kbuild.py` scripts.
- Remove demo-local build scripts and keep all logic in one place.
- No backward compatibility: no wrappers, no shims, no dual-script support.
- Execution is deferred until core `kbuild.py` fixes are completed.

## Requested Behavior
- Add `--build-demos` to root `kbuild.py`.
- `--build-demos` accepts one or more positional demo directories.
- Each demo directory argument must begin with `./demo/`; otherwise error.
- If no demo directories are provided, print brief error/usage indicating one or more demo directories are required.
- Integrate behavior currently handled by `tests/build-all.sh` into `kbuild.py --build-demos`.

## Script Migration Requirements
- Keep only the root `kbuild.py` as the build entrypoint.
- Remove demo `abuild.py` scripts after root orchestration supports their behavior.
- Do not add compatibility wrappers or fallback code paths.

## Paths in Scope
- Root orchestrator:
  - `kbuild.py`
- Demo scripts to remove after migration into root:
  - `demo/libraries/alpha/abuild.py`
  - `demo/libraries/beta/abuild.py`
  - `demo/libraries/delta/abuild.py`
  - `demo/executable/abuild.py`
- Related docs/scripts to update after migration:
  - `tests/build-all.sh`
  - `README.md`
  - `demo/README.md`
  - `demo/libraries/alpha/README.md`
  - `demo/libraries/beta/README.md`
  - `demo/libraries/delta/README.md`
  - `demo/executable/README.md`

## Implementation Notes for Follow-up Agent
- Preserve each demo script's local behavior/options in root `kbuild.py`:
  - libraries: `--ktrace-sdk`, optional install SDK prefix, local SDK component install
  - executable: `--ktrace-sdk`, `--alpha-sdk`, `--beta-sdk`, `--delta-sdk`
- In `--build-demos` mode, respect the provided demo order.
- Use shared repo-root vcpkg (`vcpkg/src`, `vcpkg/build`) for both core and demo builds.
- Avoid duplicated build logic across directories.
- Keep this project blocked until the pending core `kbuild.py` fixes are completed.
