# AGENTS.md

This file provides quick, repo-specific instructions for coding agents.

## Alignment Tenets (must follow)
### Debugging core tenets (must follow)
- **Do not rely on "eureka" moments from reading code.** Use the logging system to isolate problems.
- **Always compare against working builds.** We routinely have multiple working build targets (bgfx/diligent). Use differences in output and behavior to narrow the fault domain.
- **Prefer trace-first debugging once a quick fix fails.** If one or two targeted changes don't solve it, switch to trace instrumentation and comparison across builds.

### Trace logging system (how to use)
- **Enable debug:** `-v` (debug level). Use this for general diagnostics.
- **Enable trace channels:** `-t <comma-separated>` (e.g., `-t ui.rmlui,render.bgfx`). Trace is opt-in by channel.
- **No implicit trace:** `-t` requires an explicit list. Use `-t ui` or `-t ui.rmlui` etc.
- **Channel behavior:** Only trace lines in enabled channels print. Uncategorized trace should be avoided; add categories.
- **Use macros:** Prefer `KARMA_TRACE` / `KARMA_TRACE_CHANGED` over `spdlog::trace` so channels work consistently.
- **Runtime gating:** Expensive trace logic must be wrapped in `ShouldTraceChannel()` or `KARMA_TRACE_CHANGED()` so it doesn't run when disabled.
- **Change-only helpers:** Use `KARMA_TRACE_CHANGED(cat, key, ...)` at noisy callsites (render loops, per-frame systems).
- **Debug vs trace:** Use **[debug]** for temporary, problem-specific logging that should be removed once fixed. Use **[trace]** for recurring, broadly useful diagnostics (most debugging should be trace). When unsure, prefer trace and categorize it.

### Comparing build targets (recommended workflow)
- Reproduce the issue in the failing build.
- Run a known-working build with the same trace channels.
- Diff the trace output to see where behavior diverges (first missing/extra action, different IDs, sizes, or sequence).
- Make the smallest change necessary to align the behavior, then re-test across the matrix.

## Architecture Decision
- **Render loop is engine-owned.** The EngineApp drives the frame lifecycle (beginFrame/render/endFrame). Games submit draw items during update; they do not own the render loop. This aligns with Unity/Unreal/Godot.
- **Render layer order:** lower layer IDs render first. Default constants: `kLayerWorld = 0`, `kLayerUI = 1000` (see `include/karma/renderer/layers.hpp`).
