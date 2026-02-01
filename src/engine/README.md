# src/engine/README.md

Karma is the **engine layer** of this project. It provides reusable systems for
rendering, input mapping, physics, audio, windowing, networking transport, and
UI bridges.

This subtree is designed to be extracted into its own repo. While it lives in
this mono-repo, BZ3 consumes it through `karma/...` include paths (public
headers live under `include/karma/` and point directly at engine headers).

## What Karma does not contain
- Game rules or gameplay logic
- BZ3 network protocol or world session logic
- HUD or console logic

Those live under `src/game/`.

## How to work with Karma
Start with:

1) `src/engine/AGENTS.md` (detailed orientation)
2) The subsystem README in the directory you care about

Example requests:
- “Please update the input mapper. Read `src/engine/input/README.md` and
  `src/engine/input/AGENTS.md`.”
- “We need to adjust renderer output. Read `src/engine/renderer/README.md`.”
