# Install RPATH Packaging

## Scope
- Review install-time RPATH handling for SDK and demo artifacts.

## Problem
- `ktools_apply_runtime_rpath(...)` currently sets both `BUILD_RPATH` and `INSTALL_RPATH` from `KTOOLS_RUNTIME_RPATH_DIRS`.
- In local kbuild flows, those directories are absolute build-tree paths under sibling repos.
- Installed artifacts under `build/latest/sdk/` and demo `build/latest/sdk/` trees therefore embed host-specific runtime search paths.

## Why This Matters
- Local development works, but the produced SDK trees are not cleanly relocatable.
- Moving artifacts to a different machine or directory can break runtime dependency lookup.
- Absolute install RPATHs can also hide missing dependency-packaging problems during development.

## Suggested Follow-up
- Keep `BUILD_RPATH` support for local demo execution if needed.
- Stop copying the same absolute directories into `INSTALL_RPATH`.
- If installed artifacts are meant to be self-contained, use a relative `$ORIGIN`-style install layout instead.
- Add a packaging verification step (for example `readelf -d`) if relocatable SDK output is a goal.
