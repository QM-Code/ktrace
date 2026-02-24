# Runtime Load Map (m-karma)

## Include Trace From Entrypoints (Header-Only, Project Headers Only)

```text
server
demo/server/main.cpp
  -> karma/app/server/runner.hpp -> app/server/runner.cpp
    -> karma/cli/server/app_options.hpp -> cli/server/app_options.cpp
      -> karma/cli/shared/parse.hpp -> cli/shared/parse.cpp
  -> demo/server/runtime.hpp -> demo/server/runtime.cpp
    -> karma/cli/server/app_options.hpp -> cli/server/app_options.cpp
      -> karma/cli/shared/parse.hpp -> cli/shared/parse.cpp
```

```text
client
demo/client/main.cpp
  -> karma/app/client/runner.hpp -> app/client/runner.cpp
    -> karma/cli/client/app_options.hpp -> cli/client/app_options.cpp
      -> karma/cli/shared/parse.hpp -> cli/shared/parse.cpp
  -> demo/client/runtime.hpp -> demo/client/runtime.cpp
    -> karma/cli/client/app_options.hpp -> cli/client/app_options.cpp
      -> karma/cli/shared/parse.hpp -> cli/shared/parse.cpp
```

## Include Path Conventions

- `#include "karma/..."`
  Project headers resolved from the `m-karma/include` include root (public-facing module paths).
- `#include "demo/..."`
  Demo-side headers resolved from `m-karma/src/demo/...`.

So `karma/...` is not a C++ language special form; it is a project include-root convention.

## What `demo/{client|server}/main.cpp` Is Doing

Each demo entrypoint is intentionally small: it passes `argc/argv`, a `RunSpec`, and a runtime callback into
`karma::app::{client|server}::Run(...)`, then returns that result as process exit code.

In other words, `main.cpp` does not contain startup logic itself. It delegates startup orchestration to the runner.

## Runner Concepts (Plain Terms)

- `RunSpec.parse_app_name`
  Used as the fallback executable name while parsing CLI options (for help/error naming when `argv[0]` is missing or odd).
- `RunSpec.bootstrap_app_name`
  Used when bootstrap builds app-level path/config context (data path spec, user-config resolution context, etc.).
- `RuntimeHook`
  A callback (`std::function<int(const AppOptions&)>`) that runner invokes after CLI parse/bootstrap succeeds.
  In the demo binaries this is `karma::demo::{client|server}::RunRuntime`.
- `AppOptions`
  The parsed CLI state object passed from runner into bootstrap and then into the runtime hook.

## Why `runner.hpp` Includes `karma/cli/*/app_options.hpp`

`runner.hpp` exposes these public API types:

- `using RuntimeHook = std::function<int(const ...::AppOptions&)>;`
- `int Run(..., const RuntimeHook& runtime_hook);`

Because `AppOptions` is part of the runner API surface, the header depends on `app_options.hpp`.

## Actual Startup Sequence (Shared Skeleton)

Client and server follow the same top-level sequence:

1. `main()` calls `karma::app::{client|server}::Run(...)`.
2. Runner computes `parse_app_name` and `bootstrap_app_name` fallbacks.
3. Runner parses CLI args into `AppOptions` via `ParseAppOptions(...)`.
4. Runner calls `RunBootstrap(options, argc, argv, bootstrap_app_name)`.
5. Bootstrap configures logging, data-root/config loading, and config overlays.
6. Bootstrap loads i18n for the runtime role.
7. If `--help` was requested, runner prints localized help and exits `0` (runtime hook is not called).
8. Runner resolves configured app name and invokes `runtime_hook(options)`.
9. Any thrown exception is logged and converted to an exit code.

## Client vs Server Bootstrap Differences

- Client bootstrap loads `data/client/config.json` plus `data/server/config.json`, then applies common CLI config overlays.
- Server bootstrap loads `data/server/config.json`, applies optional `--server-config`, then applies common CLI config overlays.
- Client bootstrap performs required-config validation before runtime begins; server bootstrap currently does not run an equivalent required-key sweep there.

## Runtime Hook Boundary

The runner/bootstrap side is startup orchestration. `RunRuntime(...)` is where app-specific runtime behavior begins.

- Client demo runtime: connect/auth/session demo flow and network event loop.
- Server demo runtime: transport init, pre-auth/session hooks, and server event loop.
