# Coding Standards

## Trace Logging System

The trace logging system allows you to use complex output logging with minimal runtime impact. Use it liberally. It is enabled in all client and server builds, and may also be incorporated into other testing binaries.

- Enable trace channels: `--trace <comma-separated>` (e.g., `--trace ui.rmlui,render.bgfx`). Trace is opt-in by channel.
- No implicit trace: `--trace` requires an explicit list. Use `--trace ui` or `--trace ui.rmlui` etc.
- Channel behavior: Only trace lines in enabled channels print. Uncategorized trace should be avoided; add categories.
- Use macros: Prefer `KARMA_TRACE` / `KARMA_TRACE_CHANGED` over `spdlog::trace` so channels work consistently.
- Runtime gating: Expensive trace logic must be wrapped in `ShouldTraceChannel()` or `KARMA_TRACE_CHANGED()` so it doesn't run when disabled.
- Change-only helpers: Use `KARMA_TRACE_CHANGED(cat, key, ...)` at noisy callsites (render loops, per-frame systems).
- Debug vs trace: Use [debug] for temporary, problem-specific logging that should be removed once fixed. Use [trace] for recurring, broadly useful diagnostics (most debugging should be trace). When unsure, prefer trace and categorize it.

## Debugging core tenets

- Do not rely on "eureka" moments from reading code. Use the logging system to isolate problems.
- Always compare against working builds. We routinely have multiple working build targets (bgfx/diligent, physx/jolt, imgui/rmlui). Use differences in output and behavior to narrow the fault domain.
- Prefer trace-first debugging once a quick fix fails. If one or two targeted changes don't solve it, switch to trace instrumentation and comparison across builds.

## Fallbacks and Backwards Compatibility

- This project is in rapid development, and fallback/comapatibility code is neither necessary nor desired.
- Avoid fallback and backwards compatiblity code except as temporary fixtures to avoid broken builds.
- If you find fallback or backwards compatibility code, please notify the human operator.
 
## JSON files are source-of-truth

- Two important rules to always follow:
  - Never hardcode tunable variables
  - Never hardcode fallbacks
- JSON config files are parsed at system startup in the following order:
  - Server startup:
    - <branch>/data/server/config.json (minimalistic master file)
    - <branch>/demo/servers/<server>/config.json (server-specific config)
  - Client startup:
    - <branch>/data/client/config.json (minimalistic master file)
    - <branch>/demo/users/<user>/config.json (user-specific config)
- All tunable variables should be set through appropriate config.json file.
  - In general, do not add keys to the master files
  - Use the demo/ directory to create test data in an appropriate subdirectory.
- Mandatory variables should use ReadRequired*Config(<key>)
  - Do not use fallbacks
  - ReadRequired*Config(<key>) makes it mandatory for a key to be specified in some config.json.
  - Missing keys cause fail on startup that alert the user that the key is missing.
  - No further error reporting code is required.
- Testing note:
  - Passing `--config '<key>:<value>'` allows command-line override of specific config keys
  - Command-line config options override config.json files.

## Comparing build targets (recommended workflow)

- Reproduce the issue in the failing build.
- Run a known-working build with the same trace channels.
- Diff the trace output to see where behavior diverges (first missing/extra action, different IDs, sizes, or sequence).
- Make the smallest change necessary to align the behavior, then re-test across the matrix.

